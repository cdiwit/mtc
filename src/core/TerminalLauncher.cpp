#include "TerminalLauncher.h"
#include "ConfigManager.h"
#include <cstdlib>
#include <fstream>
#include <string>
#include <ctime>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#elif defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cstring>
#include <fcntl.h>
#include <cerrno>
#include <ctime>
#include <cstdio>
#include <fstream>
#include <memory>
#include <array>
#include <vector>
extern char** environ;
#endif

bool TerminalLauncher::Launch(const Profile& profile, std::string* errorMsg) {
    // 远程配置：走 SSH 路径（在外部终端里跑 ssh）
    if (profile.IsRemote()) {
        return LaunchRemote(profile, errorMsg);
    }

    auto env = BuildEnvironment(profile.environmentVariables);

#ifdef _WIN32
    return LaunchWindows(profile, env, errorMsg);
#elif defined(__linux__)
    return LaunchLinux(profile, env, errorMsg);
#elif defined(__APPLE__)
    return LaunchMacOS(profile, env, errorMsg);
#else
    if (errorMsg) *errorMsg = "Unsupported platform";
    return false;
#endif
}

std::string TerminalLauncher::ShellSingleQuote(const std::string& s) {
    std::string r = "'";
    for (char c : s) {
        if (c == '\'') r += "'\\''";
        else r += c;
    }
    r += "'";
    return r;
}

std::string TerminalLauncher::BuildRemoteInnerScript(const Profile& profile) {
    // 这段脚本将通过 `ssh ... bash -s < script` 由 stdin 送到远程执行，
    // 因此无需担心命令行的引号转义问题，脚本自身的单引号由远程 bash 正常解析。
    std::string script = "#!/bin/bash\n";
    if (!profile.remoteWorkingDirectory.empty()) {
        script += "cd " + ShellSingleQuote(profile.remoteWorkingDirectory) + " 2>/dev/null\n";
    }
    for (const auto& var : profile.environmentVariables) {
        if (!var.name.empty()) {
            script += "export " + var.name + "=" + ShellSingleQuote(var.value) + "\n";
        }
    }
    for (const auto& cmd : profile.startupCommands) {
        if (!cmd.empty()) {
            script += cmd + "\n";
        }
    }
    // 落到交互式远程 shell
    script += "exec \"$SHELL\" -l -i\n";
    return script;
}

std::map<std::string, std::string> TerminalLauncher::BuildEnvironment(
    const std::vector<EnvVariable>& customVars
) {
    std::map<std::string, std::string> env;
    
    // 获取当前环境变量
#ifdef _WIN32
    // Helper to convert Wide String to UTF-8
    auto WideToUtf8 = [](const std::wstring& wstr) -> std::string {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    };

    wchar_t* envStrings = GetEnvironmentStringsW();
    if (envStrings) {
        for (wchar_t* p = envStrings; *p; ) {
            std::wstring entry(p);
            size_t pos = entry.find(L'=');
            if (pos != std::wstring::npos && pos > 0) {
                std::wstring wname = entry.substr(0, pos);
                std::wstring wvalue = entry.substr(pos + 1);
                
                std::string name = WideToUtf8(wname);
                std::string value = WideToUtf8(wvalue);
                
                env[name] = value;
            }
            p += entry.length() + 1;
        }
        FreeEnvironmentStringsW(envStrings);
    }
    
    // 用自定义变量覆盖 (Windows 大小写不敏感处理)
    for (const auto& var : customVars) {
        bool found = false;
        // 查找是否存在同名变量（忽略大小写）
        for (auto it = env.begin(); it != env.end(); ++it) {
            if (_stricmp(it->first.c_str(), var.name.c_str()) == 0) {
                // 找到同名变量，删除旧的（为了保持用户的大小写偏好），插入新的
                env.erase(it);
                env[var.name] = var.value;
                found = true;
                break;
            }
        }
        
        if (!found) {
            env[var.name] = var.value;
        }
    }
#else
    for (char** e = environ; *e; ++e) {
        std::string entry(*e);
        size_t pos = entry.find('=');
        if (pos != std::string::npos) {
            env[entry.substr(0, pos)] = entry.substr(pos + 1);
        }
    }
    
    // 用自定义变量覆盖 (Linux/Mac 大小写敏感)
    for (const auto& var : customVars) {
        env[var.name] = var.value;
    }
#endif
    
    return env;
}

std::vector<TerminalType> TerminalLauncher::GetAvailableTerminals() {
    std::vector<TerminalType> terminals;
    terminals.push_back(TerminalType::Auto);
    
#ifdef _WIN32
    terminals.push_back(TerminalType::WindowsTerminal);
    terminals.push_back(TerminalType::PowerShell);
    terminals.push_back(TerminalType::Cmd);
#elif defined(__linux__)
    terminals.push_back(TerminalType::ExoOpen);
    terminals.push_back(TerminalType::QTerminal);
    terminals.push_back(TerminalType::GnomeTerminal);
    terminals.push_back(TerminalType::Konsole);
    terminals.push_back(TerminalType::Xfce4Terminal);
    terminals.push_back(TerminalType::MateTerminal);
    terminals.push_back(TerminalType::Alacritty);
    terminals.push_back(TerminalType::Xterm);
#elif defined(__APPLE__)
    terminals.push_back(TerminalType::TerminalApp);
    terminals.push_back(TerminalType::ITerm2);
#endif
    
    return terminals;
}

std::string TerminalLauncher::GetTerminalDisplayName(TerminalType type) {
    return TerminalTypeDisplayName(type);
}

bool TerminalLauncher::IsTerminalAvailable(TerminalType type) {
#ifdef _WIN32
    switch (type) {
        case TerminalType::WindowsTerminal: {
            wchar_t path[MAX_PATH];
            return SearchPathW(nullptr, L"wt.exe", nullptr, MAX_PATH, path, nullptr) != 0;
        }
        case TerminalType::PowerShell:
        case TerminalType::Cmd:
            return true;  // 总是可用
        default:
            return false;
    }
#elif defined(__linux__)
    switch (type) {
        case TerminalType::ExoOpen:
            return system("which exo-open > /dev/null 2>&1") == 0;
        case TerminalType::QTerminal:
            return system("which qterminal > /dev/null 2>&1") == 0;
        case TerminalType::GnomeTerminal:
            return system("which gnome-terminal > /dev/null 2>&1") == 0;
        case TerminalType::Konsole:
            return system("which konsole > /dev/null 2>&1") == 0;
        case TerminalType::Xfce4Terminal:
            return system("which xfce4-terminal > /dev/null 2>&1") == 0;
        case TerminalType::MateTerminal:
            return system("which mate-terminal > /dev/null 2>&1") == 0;
        case TerminalType::Alacritty:
            return system("which alacritty > /dev/null 2>&1") == 0;
        case TerminalType::Xterm:
            return system("which xterm > /dev/null 2>&1") == 0;
        default:
            return false;
    }
#elif defined(__APPLE__)
    return type == TerminalType::TerminalApp || type == TerminalType::ITerm2;
#else
    return false;
#endif
}

TerminalType TerminalLauncher::AutoDetectTerminal() {
#ifdef _WIN32
    // 优先使用 Windows Terminal
    if (IsTerminalAvailable(TerminalType::WindowsTerminal)) {
        return TerminalType::WindowsTerminal;
    }
    return TerminalType::Cmd;
    
#elif defined(__linux__)
    // 按优先级检测（exo-open 优先，XFCE 标准方式）
    if (IsTerminalAvailable(TerminalType::ExoOpen)) {
        return TerminalType::ExoOpen;
    }
    if (IsTerminalAvailable(TerminalType::QTerminal)) {
        return TerminalType::QTerminal;
    }
    if (IsTerminalAvailable(TerminalType::GnomeTerminal)) {
        return TerminalType::GnomeTerminal;
    }
    if (IsTerminalAvailable(TerminalType::Konsole)) {
        return TerminalType::Konsole;
    }
    if (IsTerminalAvailable(TerminalType::Xfce4Terminal)) {
        return TerminalType::Xfce4Terminal;
    }
    if (IsTerminalAvailable(TerminalType::MateTerminal)) {
        return TerminalType::MateTerminal;
    }
    if (IsTerminalAvailable(TerminalType::Alacritty)) {
        return TerminalType::Alacritty;
    }
    if (IsTerminalAvailable(TerminalType::Xterm)) {
        return TerminalType::Xterm;
    }
    return TerminalType::Auto;  // 没有找到任何可用终端
    
#elif defined(__APPLE__)
    return TerminalType::TerminalApp;
    
#else
    return TerminalType::Auto;
#endif
}

#ifdef _WIN32
// Helper to convert UTF-8 to Wide String
std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Helper to formatted error message
std::string GetLastErrorAsString(DWORD errorMessageID) {
    if(errorMessageID == 0) {
        return "Unknown error (0)";
    }
    
    LPWSTR messageBuffer = nullptr;
    size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);
    
    std::string message;
    if (size > 0 && messageBuffer) {
        std::wstring wmsg(messageBuffer, size);
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wmsg[0], (int)wmsg.size(), NULL, 0, NULL, NULL);
        message.resize(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, &wmsg[0], (int)wmsg.size(), &message[0], size_needed, NULL, NULL);
        
        LocalFree(messageBuffer);
    } else {
        message = "Failed to format error message";
    }

    return message + " (Code: " + std::to_string(errorMessageID) + ")";
}

#include <algorithm> // For std::sort
#include <fstream>
#include <filesystem>
#include "../utils/PathUtils.h"

// ... (previous code)

bool TerminalLauncher::LaunchWindows(
    const Profile& profile,
    const std::map<std::string, std::string>& env,
    std::string* errorMsg
) {
    std::wstring command, args;
    std::wstring workDir = Utf8ToWide(profile.GetWorkingDirectory());
    
    TerminalType type = profile.terminalType;
    if (type == TerminalType::Auto) {
        type = AutoDetectTerminal();
    }
    
    // Windows Terminal 特殊处理：使用 PowerShell 脚本初始化环境变量
    if (type == TerminalType::WindowsTerminal) {
        try {
            fs::path tempDir = fs::temp_directory_path();
            std::string filename = "mtc_init_" + std::to_string(std::time(nullptr)) + ".ps1";
            fs::path scriptPath = tempDir / filename;
            
            std::ofstream script(scriptPath);
            if (script.is_open()) {
                // 写入 UTF-8 BOM，防止中文乱码
                unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
                script.write(reinterpret_cast<char*>(bom), sizeof(bom));
                
                script << "# MTC Initialization Script" << std::endl;
                
                // 设置所有环境变量
                for (const auto& var : profile.environmentVariables) {
                    // 转义单引号
                    std::string value = var.value;
                    size_t pos = 0;
                    while ((pos = value.find("'", pos)) != std::string::npos) {
                        value.replace(pos, 1, "''");
                        pos += 2;
                    }
                    
                    script << "$env:" << var.name << " = '" << value << "'" << std::endl;
                }
                
                // 切换目录
                std::string effectiveWorkDir = profile.GetWorkingDirectory();
                if (!effectiveWorkDir.empty()) {
                    std::string wd = effectiveWorkDir;
                    // 转义
                    size_t pos = 0;
                    while ((pos = wd.find("'", pos)) != std::string::npos) {
                        wd.replace(pos, 1, "''");
                        pos += 2;
                    }
                    script << "Set-Location '" << wd << "'" << std::endl;
                }

                // Execute startup commands
                for (const auto& cmd : profile.startupCommands) {
                    if (!cmd.empty()) {
                        std::string escapedCmd = cmd;
                        size_t cmdPos = 0;
                        while ((cmdPos = escapedCmd.find("'", cmdPos)) != std::string::npos) {
                            escapedCmd.replace(cmdPos, 1, "''");
                            cmdPos += 2;
                        }
                        script << escapedCmd << std::endl;
                    }
                }

                // 清理脚本自身 (可选，但为了调试先保留，或者利用 Start-Job 删除)
                // script << "Remove-Item $MyInvocation.MyCommand.Path" << std::endl;
                
                script.close();
                
                // 构建 wt 命令行
                // wt.exe new-tab --title "Name" powershell -NoExit -ExecutionPolicy Bypass -File "path"
                command = L"wt.exe";
                args = L"new-tab --title \"" + Utf8ToWide(profile.name) + L"\" powershell -NoExit -ExecutionPolicy Bypass -File \"" + scriptPath.wstring() + L"\"";
                
                // 启动进程 (wt.exe 不需要特殊的 envBlock，因为它可能复用进程)
                // 我们仍然传递 envBlock，以防它是新进程
            }
        } catch (...) {
            // Fallback to default behavior if script creation fails
        }
    }
    
    // 如果没有被特殊处理覆盖，则使用默认逻辑
    if (command.empty()) {
        switch (type) {
            case TerminalType::WindowsTerminal:
                // Fallback (虽然上面处理了，但防止异常)
                command = L"wt.exe";
                if (!workDir.empty()) {
                    args = L"-d \"" + workDir + L"\"";
                }
                break;
            case TerminalType::PowerShell:
                command = L"powershell.exe";
                if (!workDir.empty()) {
                    args = L"-NoExit -Command \"Set-Location '" + workDir + L"'\"";
                } else {
                    args = L"-NoExit";
                }
                break;
            case TerminalType::Cmd:
            default:
                command = L"cmd.exe";
                if (!workDir.empty()) {
                    args = L"/K cd /d \"" + workDir + L"\"";
                } else {
                    args = L"/K";
                }
                break;
        }
    }

    // 构建 Unicode 环境变量块 (即便是 wt，如果是新进程也需要这个)
    // Windows 要求环境变量块必须按名称的字母顺序排序（不区分大小写）
    std::vector<std::pair<std::wstring, std::wstring>> envVars;
    envVars.reserve(env.size());
    
    for (const auto& pair : env) {
        envVars.emplace_back(Utf8ToWide(pair.first), Utf8ToWide(pair.second));
    }
    
    std::sort(envVars.begin(), envVars.end(), 
        [](const std::pair<std::wstring, std::wstring>& a, const std::pair<std::wstring, std::wstring>& b) {
            return _wcsicmp(a.first.c_str(), b.first.c_str()) < 0;
        }
    );
    
    std::wstring envBlock;
    for (const auto& pair : envVars) {
        envBlock += pair.first + L"=" + pair.second + L'\0';
    }
    envBlock += L'\0';  // 双 null 结尾
    
    std::wstring cmdLine = command + L" " + args;
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    // 使用 CREATE_NEW_CONSOLE 确保创建新的控制台窗口
    DWORD flags = CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE;
    
    BOOL success = CreateProcessW(
        nullptr,
        &cmdLine[0],
        nullptr,
        nullptr,
        FALSE,
        flags,
        (LPVOID)envBlock.c_str(),
        workDir.empty() ? nullptr : workDir.c_str(),
        &si,
        &pi
    );
    
    // 立即保存错误码
    DWORD lastError = ::GetLastError();
    
    if (success) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    
    if (errorMsg) {
        *errorMsg = GetLastErrorAsString(lastError);
    }

    return false;
}
#endif

#ifdef __linux__
bool TerminalLauncher::LaunchLinux(
    const Profile& profile,
    const std::map<std::string, std::string>& env,
    std::string* errorMsg
) {
    // Create a temporary shell script to set up environment
    bool hasStartupCommands = !profile.startupCommands.empty();
    std::string scriptPath;
    std::string effectiveWorkDir = profile.GetWorkingDirectory();

    if (hasStartupCommands || !effectiveWorkDir.empty() || !profile.environmentVariables.empty()) {
        scriptPath = "/tmp/mtc_init_" + std::to_string(std::time(nullptr)) + ".sh";
        std::ofstream scriptFile(scriptPath);
        if (!scriptFile.is_open()) {
            if (errorMsg) *errorMsg = "Failed to create init script";
            return false;
        }

        scriptFile << "#!/bin/bash\n";

        // Change directory
        if (!effectiveWorkDir.empty()) {
            std::string escaped = effectiveWorkDir;
            for (size_t pos = 0; (pos = escaped.find('\'', pos)) != std::string::npos; pos += 4) {
                escaped.replace(pos, 1, "'\"'\"'");
            }
            scriptFile << "cd '" << escaped << "'\n";
        }

        // Set environment variables
        for (const auto& var : profile.environmentVariables) {
            std::string escapedValue = var.value;
            for (size_t pos = 0; (pos = escapedValue.find('\'', pos)) != std::string::npos; pos += 4) {
                escapedValue.replace(pos, 1, "'\"'\"'");
            }
            scriptFile << "export " << var.name << "='" << escapedValue << "'\n";
        }

        // If there are startup commands, write them to a separate temp file.
        // They need to run AFTER the user's shell loads (nvm, PATH etc.),
        // so we use $SHELL -i -c to get full environment first.
        if (hasStartupCommands) {
            std::string startupPath = "/tmp/mtc_startup_" + std::to_string(std::time(nullptr)) + ".sh";
            std::ofstream startupFile(startupPath);
            for (const auto& cmd : profile.startupCommands) {
                if (!cmd.empty()) {
                    startupFile << cmd << "\n";
                }
            }
            startupFile.close();
            chmod(startupPath.c_str(), 0600);

            scriptFile << "rm -f '" << scriptPath << "'\n";
            // Launch user's shell interactively (-i) so .zshrc/.bashrc loads first,
            // then run startup commands, then start a clean interactive shell
            scriptFile << "exec \"$SHELL\" -i -c \"source '" << startupPath
                       << "'; rm -f '" << startupPath << "'; exec \\\"\\$SHELL\\\"\"\n";
        } else {
            scriptFile << "rm -f '" << scriptPath << "'\n";
            scriptFile << "exec \"$SHELL\"\n";
        }
        scriptFile.close();

        chmod(scriptPath.c_str(), 0700);
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        if (errorMsg) *errorMsg = "Failed to create pipe: " + std::string(strerror(errno));
        if (!scriptPath.empty()) remove(scriptPath.c_str());
        return false;
    }

    if (fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) == -1) {
        if (errorMsg) *errorMsg = "Failed to set FD_CLOEXEC: " + std::string(strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        if (!scriptPath.empty()) remove(scriptPath.c_str());
        return false;
    }

    // Resolve terminal type BEFORE fork (in parent process)
    // so system("which ...") works reliably
    TerminalType type = profile.terminalType;
    if (type == TerminalType::Auto) {
        type = AutoDetectTerminal();
    }
    if (type == TerminalType::Auto) {
        if (errorMsg) *errorMsg = "未找到支持的终端模拟器，请安装 exo-open、qterminal、gnome-terminal、konsole、xfce4-terminal、mate-terminal、alacritty 或 xterm";
        close(pipefd[0]);
        close(pipefd[1]);
        if (!scriptPath.empty()) remove(scriptPath.c_str());
        return false;
    }

    pid_t pid = fork();

    if (pid == -1) {
         if (errorMsg) *errorMsg = "Failed to fork: " + std::string(strerror(errno));
         close(pipefd[0]);
         close(pipefd[1]);
         if (!scriptPath.empty()) remove(scriptPath.c_str());
         return false;
    }

    if (pid == 0) {
        // Child
        close(pipefd[0]);

        // Set only custom environment variables (system vars are inherited via fork)
        // Setting all vars could override PATH and break execlp
        for (const auto& var : profile.environmentVariables) {
            setenv(var.name.c_str(), var.value.c_str(), 1);
        }

        // Ensure PATH includes standard directories so execlp can find terminal emulators
        const char* currentPath = getenv("PATH");
        std::string safePath = currentPath ? currentPath : "";
        safePath += ":/usr/bin:/usr/local/bin:/bin:/snap/bin";
        setenv("PATH", safePath.c_str(), 1);

        if (!scriptPath.empty()) {
            switch (type) {
                case TerminalType::ExoOpen:
                    execlp("exo-open", "exo-open", "--launch", "TerminalEmulator",
                           "/bin/bash", scriptPath.c_str(), nullptr);
                    break;
                case TerminalType::QTerminal:
                    execlp("qterminal", "qterminal", "-e",
                           "/bin/bash", scriptPath.c_str(), nullptr);
                    break;
                case TerminalType::GnomeTerminal:
                    execlp("gnome-terminal", "gnome-terminal", "--",
                           "/bin/bash", scriptPath.c_str(), nullptr);
                    break;
                case TerminalType::Konsole:
                    execlp("konsole", "konsole", "-e",
                           "/bin/bash", scriptPath.c_str(), nullptr);
                    break;
                case TerminalType::Xfce4Terminal:
                    execlp("xfce4-terminal", "xfce4-terminal", "--",
                           "/bin/bash", scriptPath.c_str(), nullptr);
                    break;
                case TerminalType::MateTerminal:
                    execlp("mate-terminal", "mate-terminal", "--",
                           "/bin/bash", scriptPath.c_str(), nullptr);
                    break;
                case TerminalType::Alacritty:
                    execlp("alacritty", "alacritty", "-e",
                           "/bin/bash", scriptPath.c_str(), nullptr);
                    break;
                case TerminalType::Xterm:
                default:
                    execlp("xterm", "xterm", "-e",
                           "/bin/bash", scriptPath.c_str(), nullptr);
                    break;
            }
        } else {
            switch (type) {
                case TerminalType::ExoOpen:
                    execlp("exo-open", "exo-open", "--launch", "TerminalEmulator", nullptr);
                    break;
                case TerminalType::QTerminal:
                    execlp("qterminal", "qterminal", nullptr);
                    break;
                case TerminalType::GnomeTerminal:
                    execlp("gnome-terminal", "gnome-terminal", "--", nullptr);
                    break;
                case TerminalType::Konsole:
                    execlp("konsole", "konsole", nullptr);
                    break;
                case TerminalType::Xfce4Terminal:
                    execlp("xfce4-terminal", "xfce4-terminal", nullptr);
                    break;
                case TerminalType::MateTerminal:
                    execlp("mate-terminal", "mate-terminal", nullptr);
                    break;
                case TerminalType::Alacritty:
                    execlp("alacritty", "alacritty", nullptr);
                    break;
                case TerminalType::Xterm:
                default:
                    execlp("xterm", "xterm", nullptr);
                    break;
            }
        }

        // If we get here, exec failed
        std::string errDetail = "Failed to exec " +
            TerminalTypeToString(type) + ": " + strerror(errno) +
            " (PATH=" + (getenv("PATH") ? getenv("PATH") : "(null)") + ")";
        // Write length + string through pipe
        size_t len = errDetail.size();
        (void)write(pipefd[1], &len, sizeof(len));
        (void)write(pipefd[1], errDetail.c_str(), len);
        _exit(1);
    }

    // Parent
    close(pipefd[1]);

    size_t errLen = 0;
    ssize_t count = read(pipefd[0], &errLen, sizeof(errLen));
    if (count > 0 && errLen > 0 && errLen < 4096) {
        std::vector<char> buf(errLen + 1, '\0');
        (void)read(pipefd[0], buf.data(), errLen);
        if (errorMsg) *errorMsg = std::string(buf.data());
    } else if (count > 0) {
        if (errorMsg) *errorMsg = "Unknown launch error";
    }
    close(pipefd[0]);

    if (count > 0) {
        waitpid(pid, nullptr, 0);
        if (!scriptPath.empty()) remove(scriptPath.c_str());
        return false;
    }

    return true;
}
#endif

#ifdef __APPLE__
bool TerminalLauncher::LaunchMacOS(
    const Profile& profile,
    const std::map<std::string, std::string>& env,
    std::string* errorMsg
) {
    // Create a temporary shell script to set up environment silently
    std::string scriptPath = "/tmp/mtc_init_" + std::to_string(std::time(nullptr)) + ".sh";

    std::ofstream scriptFile(scriptPath);
    if (!scriptFile.is_open()) {
        if (errorMsg) *errorMsg = "Failed to create init script";
        return false;
    }

    scriptFile << "#!/bin/bash\n";

    // Change directory
    std::string effectiveWorkDir = profile.GetWorkingDirectory();
    if (!effectiveWorkDir.empty()) {
        std::string escaped = effectiveWorkDir;
        for (size_t pos = 0; (pos = escaped.find('\'', pos)) != std::string::npos; pos += 4) {
            escaped.replace(pos, 1, "'\"'\"'");
        }
        scriptFile << "cd '" << escaped << "'\n";
    }

    // Set environment variables
    for (const auto& var : profile.environmentVariables) {
        std::string escapedName = var.name;
        std::string escapedValue = var.value;
        for (size_t pos = 0; (pos = escapedValue.find('\'', pos)) != std::string::npos; pos += 4) {
            escapedValue.replace(pos, 1, "'\"'\"'");
        }
        scriptFile << "export " << escapedName << "='" << escapedValue << "'\n";
    }

    // Execute startup commands
    for (const auto& cmd : profile.startupCommands) {
        if (!cmd.empty()) {
            scriptFile << cmd << "\n";
        }
    }

    // Remove the script itself and clear screen
    scriptFile << "rm -f '" << scriptPath << "'\n";
    scriptFile << "clear\n";
    scriptFile.close();

    chmod(scriptPath.c_str(), 0700);

    // Use AppleScript to open Terminal and source the script
    // Use . (dot) instead of source to avoid quoting issues
    std::string appleScript = "tell application \"Terminal\"\n"
        "  activate\n"
        "  do script \". " + scriptPath + "\"\n"
        "end tell";

    // Escape single quotes for osascript
    std::string escapedScript;
    for (char c : appleScript) {
        if (c == '\'') {
            escapedScript += "'\"'\"'";
        } else {
            escapedScript += c;
        }
    }

    std::string cmd = "osascript -e '" + escapedScript + "' 2>&1";

    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        remove(scriptPath.c_str());
        if (errorMsg) *errorMsg = "Failed to run osascript: " + std::string(strerror(errno));
        return false;
    }

    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), fp) != nullptr) {
        result += buffer.data();
    }

    int status = pclose(fp);

    if (status != 0) {
        remove(scriptPath.c_str());
        if (errorMsg) {
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            *errorMsg = result;
        }
        return false;
    }

    return true;
}
#endif

// ============================================================================
// 远程 SSH 启动
// ============================================================================
bool TerminalLauncher::LaunchRemote(const Profile& profile, std::string* errorMsg) {
    const SshHost* host = ConfigManager::GetInstance().GetSshHost(profile.sshHostId);
    if (!host) {
        if (errorMsg) *errorMsg = "找不到 SSH 主机配置（可能已被删除）";
        return false;
    }
    const Credential* cred = profile.credentialId.empty()
        ? nullptr
        : ConfigManager::GetInstance().GetCredential(profile.credentialId);

    // ssh 主干：-t 强制 TTY；非标准端口加 -p；私钥认证加 -i
    std::string userAtHost = host->host;
    if (!host->username.empty()) {
        userAtHost = host->username + "@" + host->host;
    }
    std::string sshCore = "ssh -t";
    if (host->port != 22) {
        sshCore += " -p " + std::to_string(host->port);
    }
    if (cred && cred->type == CredentialType::PrivateKey && !cred->keyPath.empty()) {
        sshCore += " -i " + ShellSingleQuote(cred->keyPath);
    }
    sshCore += " " + userAtHost;

    // 远程执行的内层脚本（cd + export + 启动命令 + 交互 shell）
    std::string innerScript = BuildRemoteInnerScript(profile);

#if defined(__APPLE__) || defined(__linux__)
    // 写入临时文件，通过 `bash -s < script` 由 stdin 送往远程，规避命令行转义
    std::string innerPath = "/tmp/mtc_remote_inner_" +
        std::to_string(std::time(nullptr)) + ".sh";
    {
        std::ofstream f(innerPath);
        if (!f.is_open()) {
            if (errorMsg) *errorMsg = "无法创建远程脚本临时文件";
            return false;
        }
        f << innerScript;
    }
    chmod(innerPath.c_str(), 0600);

    std::string sshCmd = sshCore + " bash -s < " + ShellSingleQuote(innerPath) +
                         "; rm -f " + ShellSingleQuote(innerPath);
#endif

#ifdef __APPLE__
    // AppleScript 字符串里转义反斜杠和双引号
    std::string asCmd;
    for (char c : sshCmd) {
        if (c == '\\' || c == '"') asCmd += '\\';
        asCmd += c;
    }
    std::string appleScript =
        "tell application \"Terminal\"\n"
        "  activate\n"
        "  do script \"" + asCmd + "\"\n"
        "end tell";

    // osascript 用单引号包裹，转义内部单引号
    std::string escapedScript;
    for (char c : appleScript) {
        if (c == '\'') escapedScript += "'\"'\"'";
        else escapedScript += c;
    }
    std::string cmd = "osascript -e '" + escapedScript + "' 2>&1";

    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        remove(innerPath.c_str());
        if (errorMsg) *errorMsg = "无法运行 osascript: " + std::string(strerror(errno));
        return false;
    }
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), fp) != nullptr) {
        result += buffer.data();
    }
    int status = pclose(fp);
    if (status != 0) {
        remove(innerPath.c_str());
        if (errorMsg) {
            if (!result.empty() && result.back() == '\n') result.pop_back();
            *errorMsg = result;
        }
        return false;
    }
    return true;
#endif

#ifdef __linux__
    // 包装脚本：跑 ssh，结束后清理临时文件
    std::string wrapperPath = "/tmp/mtc_remote_launch_" +
        std::to_string(std::time(nullptr)) + ".sh";
    {
        std::ofstream f(wrapperPath);
        if (!f.is_open()) {
            remove(innerPath.c_str());
            if (errorMsg) *errorMsg = "无法创建启动脚本临时文件";
            return false;
        }
        f << "#!/bin/bash\n" << sshCmd << "\n";
    }
    chmod(wrapperPath.c_str(), 0700);

    TerminalType type = profile.terminalType;
    if (type == TerminalType::Auto) type = AutoDetectTerminal();
    if (type == TerminalType::Auto) {
        remove(innerPath.c_str());
        remove(wrapperPath.c_str());
        if (errorMsg) *errorMsg = "未找到支持的终端模拟器";
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        remove(innerPath.c_str());
        remove(wrapperPath.c_str());
        if (errorMsg) *errorMsg = "fork 失败: " + std::string(strerror(errno));
        return false;
    }
    if (pid == 0) {
        // 子进程：让 PATH 覆盖常见目录后执行终端模拟器
        const char* currentPath = getenv("PATH");
        std::string safePath = currentPath ? currentPath : "";
        safePath += ":/usr/bin:/usr/local/bin:/bin:/snap/bin";
        setenv("PATH", safePath.c_str(), 1);

        switch (type) {
            case TerminalType::ExoOpen:
                execlp("exo-open", "exo-open", "--launch", "TerminalEmulator",
                       "/bin/bash", wrapperPath.c_str(), nullptr); break;
            case TerminalType::QTerminal:
                execlp("qterminal", "qterminal", "-e",
                       "/bin/bash", wrapperPath.c_str(), nullptr); break;
            case TerminalType::GnomeTerminal:
                execlp("gnome-terminal", "gnome-terminal", "--",
                       "/bin/bash", wrapperPath.c_str(), nullptr); break;
            case TerminalType::Konsole:
                execlp("konsole", "konsole", "-e",
                       "/bin/bash", wrapperPath.c_str(), nullptr); break;
            case TerminalType::Xfce4Terminal:
                execlp("xfce4-terminal", "xfce4-terminal", "--",
                       "/bin/bash", wrapperPath.c_str(), nullptr); break;
            case TerminalType::MateTerminal:
                execlp("mate-terminal", "mate-terminal", "--",
                       "/bin/bash", wrapperPath.c_str(), nullptr); break;
            case TerminalType::Alacritty:
                execlp("alacritty", "alacritty", "-e",
                       "/bin/bash", wrapperPath.c_str(), nullptr); break;
            case TerminalType::Xterm:
            default:
                execlp("xterm", "xterm", "-e",
                       "/bin/bash", wrapperPath.c_str(), nullptr); break;
        }
        _exit(1);
    }
    return true;
#endif

#ifdef _WIN32
    std::filesystem::path tempDir = std::filesystem::temp_directory_path();
    std::string innerName = "mtc_remote_inner_" + std::to_string(std::time(nullptr)) + ".sh";
    std::filesystem::path innerPath = tempDir / innerName;
    {
        std::ofstream f(innerPath);
        if (!f.is_open()) {
            if (errorMsg) *errorMsg = "无法创建远程脚本临时文件";
            return false;
        }
        f << innerScript;
    }

    // cmd 支持 < 重定向，把内层脚本通过 stdin 送往远程 bash
    std::wstring innerW = innerPath.wstring();
    std::wstring sshCmdW = Utf8ToWide(sshCore) + L" bash -s < \"" + innerW + L"\"";

    TerminalType type = profile.terminalType;
    if (type == TerminalType::Auto) type = AutoDetectTerminal();

    std::wstring command, args;
    bool useWt = (type == TerminalType::WindowsTerminal) && IsTerminalAvailable(TerminalType::WindowsTerminal);
    if (useWt) {
        command = L"wt.exe";
        args = L"new-tab cmd /k \"" + sshCmdW + L"\"";
    } else {
        command = L"cmd.exe";
        args = L"/k \"" + sshCmdW + L"\"";
    }

    std::wstring cmdLine = command + L" " + args;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    DWORD flags = CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE;

    BOOL ok = CreateProcessW(nullptr, &cmdLine[0], nullptr, nullptr, FALSE,
                             flags, nullptr, nullptr, &si, &pi);
    DWORD lastError = ::GetLastError();
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    if (errorMsg) *errorMsg = GetLastErrorAsString(lastError);
    return false;
#endif
}

