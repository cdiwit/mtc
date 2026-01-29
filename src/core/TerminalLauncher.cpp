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
#include <cstring>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <memory>
#include <array>
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
    wchar_t* envStrings = GetEnvironmentStringsW();
    if (envStrings) {
        for (wchar_t* p = envStrings; *p; ) {
            std::wstring entry(p);
            size_t pos = entry.find(L'=');
            if (pos != std::wstring::npos && pos > 0) {
                // 转换 wstring 到 string (简单转换，仅 ASCII)
                std::string name, value;
                for (size_t i = 0; i < pos; ++i) {
                    name += static_cast<char>(entry[i]);
                }
                for (size_t i = pos + 1; i < entry.length(); ++i) {
                    value += static_cast<char>(entry[i]);
                }
                env[name] = value;
            }
            p += entry.length() + 1;
        }
        FreeEnvironmentStringsW(envStrings);
    }
#else
    for (char** e = environ; *e; ++e) {
        std::string entry(*e);
        size_t pos = entry.find('=');
        if (pos != std::string::npos) {
            env[entry.substr(0, pos)] = entry.substr(pos + 1);
        }
    }
#endif
    
    // 用自定义变量覆盖
    for (const auto& var : customVars) {
        env[var.name] = var.value;
    }
    
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
    return TerminalType::Xterm;
    
#elif defined(__APPLE__)
    return TerminalType::TerminalApp;
    
#else
    return TerminalType::Auto;
#endif
}

#ifdef _WIN32
// Helper to formatted error message
std::string GetLastErrorAsString() {
    DWORD errorMessageID = ::GetLastError();
    if(errorMessageID == 0) {
        return std::string();
    }
    
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(STATUS_PRIVATE | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);

    return message + " (" + std::to_string(errorMessageID) + ")";
}

bool TerminalLauncher::LaunchWindows(
    const Profile& profile,
    const std::map<std::string, std::string>& env,
    std::string* errorMsg
) {
    // 构建 Unicode 环境变量块
    std::wstring envBlock;
    for (const auto& pair : env) {
        // 转换 string 到 wstring
        std::wstring wname(pair.first.begin(), pair.first.end());
        std::wstring wvalue(pair.second.begin(), pair.second.end());
        envBlock += wname + L"=" + wvalue + L'\0';
    }
    envBlock += L'\0';  // 双 null 结尾
    
    // 确定终端命令和参数
    std::wstring command, args;
    std::wstring workDir(profile.workingDirectory.begin(), profile.workingDirectory.end());
    
    TerminalType type = profile.terminalType;
    if (type == TerminalType::Auto) {
        type = AutoDetectTerminal();
    }
    
    switch (type) {
        case TerminalType::WindowsTerminal:
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
    
    if (success) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    
    if (errorMsg) {
        *errorMsg = GetLastErrorAsString();
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
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        if (errorMsg) *errorMsg = "Failed to create pipe: " + std::string(strerror(errno));
        return false;
    }

    // Set O_CLOEXEC so pipe is closed in child on successful exec
    // This is crucial: if exec succeeds, write end is closed, read returns 0.
    // If exec fails, child writes errno and exits.
    if (fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) == -1) {
        if (errorMsg) *errorMsg = "Failed to set FD_CLOEXEC: " + std::string(strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    pid_t pid = fork();
    
    if (pid == -1) {
         if (errorMsg) *errorMsg = "Failed to fork: " + std::string(strerror(errno));
         close(pipefd[0]);
         close(pipefd[1]);
         return false;
    }
    
    if (pid == 0) {
        // Child
        close(pipefd[0]); // Close read end

        // Set environment
        for (const auto& pair : env) {
            setenv(pair.first.c_str(), pair.second.c_str(), 1);
        }
        
        // Change work dir
        if (!profile.workingDirectory.empty()) {
            if (chdir(profile.workingDirectory.c_str()) != 0) {
                int err = errno;
                write(pipefd[1], &err, sizeof(err));
                _exit(1);
            }
        }
        
        TerminalType type = profile.terminalType;
        if (type == TerminalType::Auto) {
            type = AutoDetectTerminal();
        }
        
        switch (type) {
            case TerminalType::GnomeTerminal:
                execlp("gnome-terminal", "gnome-terminal", "--", nullptr);
                break;
            case TerminalType::Konsole:
                execlp("konsole", "konsole", nullptr);
                break;
            case TerminalType::Xterm:
            default:
                execlp("xterm", "xterm", nullptr);
                break;
        }
        
        // If we get here, exec failed
        int err = errno;
        write(pipefd[1], &err, sizeof(err));
        _exit(1);
    }
    
    // Parent
    close(pipefd[1]); // Close write end
    
    int childErr = 0;
    ssize_t count = read(pipefd[0], &childErr, sizeof(childErr));
    close(pipefd[0]);
    
    if (count > 0) {
        // Child wrote error code (exec or chdir failed)
        if (errorMsg) *errorMsg = strerror(childErr);
        waitpid(pid, nullptr, 0); // Cleanup zombie
        return false;
    }
    
    // If count == 0, write end was closed (exec succeeded)
    return true;
}
#endif

#ifdef __APPLE__
bool TerminalLauncher::LaunchMacOS(
    const Profile& profile,
    const std::map<std::string, std::string>& env,
    std::string* errorMsg
) {
    // Build AppleScript
    std::string script = "tell application \"Terminal\"\n";
    script += "  activate\n";
    script += "  do script \"";
    
    // Change directory
    if (!profile.workingDirectory.empty()) {
        script += "cd '" + profile.workingDirectory + "'";
    }
    
    // Set environment variables
    for (const auto& var : profile.environmentVariables) {
        script += " && export " + var.name + "='" + var.value + "'";
    }
    
    script += "\"\n";
    script += "end tell";
    
    // Escape single quotes for shell
    std::string escapedScript;
    for (char c : script) {
        if (c == '\'') {
            escapedScript += "'\"'\"'";
        } else {
            escapedScript += c;
        }
    }
    
    // Redirect stderr to stdout to capture error message
    std::string cmd = "osascript -e '" + escapedScript + "' 2>&1";
    
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        if (errorMsg) *errorMsg = "Failed to run osascript: " + std::string(strerror(errno));
        return false;
    }

    // Read output (which contains error if failed)
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), fp) != nullptr) {
        result += buffer.data();
    }
    
    int status = pclose(fp);
    
    if (status != 0) {
        if (errorMsg) {
             // Remove trailing newline
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
