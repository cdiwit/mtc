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
extern char** environ;
#endif

bool TerminalLauncher::Launch(const Profile& profile) {
    auto env = BuildEnvironment(profile.environmentVariables);
    
#ifdef _WIN32
    return LaunchWindows(profile, env);
#elif defined(__linux__)
    return LaunchLinux(profile, env);
#elif defined(__APPLE__)
    return LaunchMacOS(profile, env);
#else
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
bool TerminalLauncher::LaunchWindows(
    const Profile& profile,
    const std::map<std::string, std::string>& env
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
    
    return false;
}
#endif

#ifdef __linux__
bool TerminalLauncher::LaunchLinux(
    const Profile& profile,
    const std::map<std::string, std::string>& env
) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // 子进程
        // 设置环境变量
        for (const auto& pair : env) {
            setenv(pair.first.c_str(), pair.second.c_str(), 1);
        }
        
        // 切换工作目录
        if (!profile.workingDirectory.empty()) {
            if (chdir(profile.workingDirectory.c_str()) != 0) {
                // 目录切换失败
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
        
        _exit(1);
    }
    
    return pid > 0;
}
#endif

#ifdef __APPLE__
bool TerminalLauncher::LaunchMacOS(
    const Profile& profile,
    const std::map<std::string, std::string>& env
) {
    // 构建 AppleScript 命令
    std::string script = "tell application \"Terminal\"\n";
    script += "  activate\n";
    script += "  do script \"";
    
    // 切换目录
    if (!profile.workingDirectory.empty()) {
        script += "cd '" + profile.workingDirectory + "'";
    }
    
    // 设置环境变量
    for (const auto& var : profile.environmentVariables) {
        script += " && export " + var.name + "='" + var.value + "'";
    }
    
    script += "\"\n";
    script += "end tell";
    
    // 转义单引号
    std::string escapedScript;
    for (char c : script) {
        if (c == '\'') {
            escapedScript += "'\"'\"'";
        } else {
            escapedScript += c;
        }
    }
    
    std::string cmd = "osascript -e '" + escapedScript + "'";
    return system(cmd.c_str()) == 0;
}
#endif
