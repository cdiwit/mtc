#pragma once
#include <string>
#include <vector>

// 环境变量结构
struct EnvVariable {
    std::string name;
    std::string value;
};

// 终端类型枚举
enum class TerminalType {
    Auto,           // 自动检测
    // Windows
    WindowsTerminal,
    PowerShell,
    Cmd,
    // Linux
    GnomeTerminal,
    Konsole,
    Xterm,
    // macOS
    TerminalApp,
    ITerm2
};

// 配置项结构
struct Profile {
    std::string id;
    std::string name;
    std::string description;
    std::string workingDirectory;
    TerminalType terminalType = TerminalType::Auto;
    std::vector<EnvVariable> environmentVariables;
    std::string createdAt;
    std::string updatedAt;
};

// 应用程序设置
struct AppSettings {
    TerminalType defaultTerminalType = TerminalType::Auto;
    std::string language = "zh-CN";
    std::string theme = "system";
    bool autoBackup = true;
};

// 完整配置
struct AppConfig {
    std::string version = "1.0";
    std::vector<Profile> profiles;
    AppSettings settings;
};

// 终端类型转换工具
inline std::string TerminalTypeToString(TerminalType type) {
    switch (type) {
        case TerminalType::WindowsTerminal: return "wt";
        case TerminalType::PowerShell: return "powershell";
        case TerminalType::Cmd: return "cmd";
        case TerminalType::GnomeTerminal: return "gnome-terminal";
        case TerminalType::Konsole: return "konsole";
        case TerminalType::Xterm: return "xterm";
        case TerminalType::TerminalApp: return "terminal.app";
        case TerminalType::ITerm2: return "iterm2";
        default: return "auto";
    }
}

inline TerminalType StringToTerminalType(const std::string& str) {
    if (str == "wt") return TerminalType::WindowsTerminal;
    if (str == "powershell") return TerminalType::PowerShell;
    if (str == "cmd") return TerminalType::Cmd;
    if (str == "gnome-terminal") return TerminalType::GnomeTerminal;
    if (str == "konsole") return TerminalType::Konsole;
    if (str == "xterm") return TerminalType::Xterm;
    if (str == "terminal.app") return TerminalType::TerminalApp;
    if (str == "iterm2") return TerminalType::ITerm2;
    return TerminalType::Auto;
}

inline std::string TerminalTypeDisplayName(TerminalType type) {
    switch (type) {
        case TerminalType::Auto: return "自动检测";
        case TerminalType::WindowsTerminal: return "Windows Terminal";
        case TerminalType::PowerShell: return "PowerShell";
        case TerminalType::Cmd: return "CMD";
        case TerminalType::GnomeTerminal: return "GNOME Terminal";
        case TerminalType::Konsole: return "Konsole";
        case TerminalType::Xterm: return "XTerm";
        case TerminalType::TerminalApp: return "Terminal.app";
        case TerminalType::ITerm2: return "iTerm2";
        default: return "未知";
    }
}
