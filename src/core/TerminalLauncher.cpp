#include "TerminalLauncher.h"
#include <cstdlib>

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
    // 按优先级检测
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

        // Execute startup commands
        for (const auto& cmd : profile.startupCommands) {
            if (!cmd.empty()) {
                scriptFile << cmd << "\n";
            }
        }

        // Remove the script itself
        scriptFile << "rm -f '" << scriptPath << "'\n";
        scriptFile << "clear\n";
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
        if (errorMsg) *errorMsg = "未找到支持的终端模拟器，请安装 gnome-terminal、konsole、xfce4-terminal、mate-terminal、alacritty 或 xterm";
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
                case TerminalType::GnomeTerminal:
                    execlp("gnome-terminal", "gnome-terminal", "--", "bash", "-c",
                           ("source '" + scriptPath + "'; exec bash").c_str(), nullptr);
                    break;
                case TerminalType::Konsole:
                    execlp("konsole", "konsole", "-e", "bash", "-c",
                           ("source '" + scriptPath + "'; exec bash").c_str(), nullptr);
                    break;
                case TerminalType::Xfce4Terminal:
                    execlp("xfce4-terminal", "xfce4-terminal", "--", "bash", "-c",
                           ("source '" + scriptPath + "'; exec bash").c_str(), nullptr);
                    break;
                case TerminalType::MateTerminal:
                    execlp("mate-terminal", "mate-terminal", "--", "bash", "-c",
                           ("source '" + scriptPath + "'; exec bash").c_str(), nullptr);
                    break;
                case TerminalType::Alacritty:
                    execlp("alacritty", "alacritty", "-e", "bash", "-c",
                           ("source '" + scriptPath + "'; exec bash").c_str(), nullptr);
                    break;
                case TerminalType::Xterm:
                default:
                    execlp("xterm", "xterm", "-e", "bash", "-c",
                           ("source '" + scriptPath + "'; exec bash").c_str(), nullptr);
                    break;
            }
        } else {
            switch (type) {
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
        write(pipefd[1], &len, sizeof(len));
        write(pipefd[1], errDetail.c_str(), len);
        _exit(1);
    }

    // Parent
    close(pipefd[1]);

    size_t errLen = 0;
    ssize_t count = read(pipefd[0], &errLen, sizeof(errLen));
    if (count > 0 && errLen > 0 && errLen < 4096) {
        std::vector<char> buf(errLen + 1, '\0');
        read(pipefd[0], buf.data(), errLen);
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
