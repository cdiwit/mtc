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
    ExoOpen,
    QTerminal,
    GnomeTerminal,
    Konsole,
    Xfce4Terminal,
    MateTerminal,
    Alacritty,
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
    std::string workingDirectory;          // Windows 工作目录
    std::string linuxWorkingDirectory;     // Linux 工作目录
    std::string macWorkingDirectory;       // macOS 工作目录
    TerminalType terminalType = TerminalType::Auto;
    std::vector<EnvVariable> environmentVariables;
    std::vector<std::string> startupCommands;  // 终端启动后自动执行的命令列表
    std::string createdAt;
    std::string updatedAt;

    std::string GetWorkingDirectory() const;
};

struct ProfileTreeNode {
    std::string label;
    std::string fullPath;
    std::vector<ProfileTreeNode> children;
    std::vector<const Profile*> profiles;
};

// 应用程序设置
struct AppSettings {
    TerminalType defaultTerminalType = TerminalType::Auto;
    std::string language = "zh-CN";
    std::string theme = "system";
    bool autoBackup = true;
    std::vector<std::string> searchHistory;
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
        case TerminalType::ExoOpen: return "exo-open";
        case TerminalType::QTerminal: return "qterminal";
        case TerminalType::GnomeTerminal: return "gnome-terminal";
        case TerminalType::Konsole: return "konsole";
        case TerminalType::Xfce4Terminal: return "xfce4-terminal";
        case TerminalType::MateTerminal: return "mate-terminal";
        case TerminalType::Alacritty: return "alacritty";
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
    if (str == "exo-open") return TerminalType::ExoOpen;
    if (str == "qterminal") return TerminalType::QTerminal;
    if (str == "gnome-terminal") return TerminalType::GnomeTerminal;
    if (str == "konsole") return TerminalType::Konsole;
    if (str == "xfce4-terminal") return TerminalType::Xfce4Terminal;
    if (str == "mate-terminal") return TerminalType::MateTerminal;
    if (str == "alacritty") return TerminalType::Alacritty;
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
        case TerminalType::ExoOpen: return "XFCE 默认终端";
        case TerminalType::QTerminal: return "QTerminal";
        case TerminalType::GnomeTerminal: return "GNOME Terminal";
        case TerminalType::Konsole: return "Konsole";
        case TerminalType::Xfce4Terminal: return "Xfce Terminal";
        case TerminalType::MateTerminal: return "MATE Terminal";
        case TerminalType::Alacritty: return "Alacritty";
        case TerminalType::Xterm: return "XTerm";
        case TerminalType::TerminalApp: return "Terminal.app";
        case TerminalType::ITerm2: return "iTerm2";
        default: return "未知";
    }
}

inline std::string Profile::GetWorkingDirectory() const {
#ifdef _WIN32
    if (!workingDirectory.empty()) return workingDirectory;
    if (!linuxWorkingDirectory.empty()) return linuxWorkingDirectory;
    if (!macWorkingDirectory.empty()) return macWorkingDirectory;
#elif defined(__linux__)
    if (!linuxWorkingDirectory.empty()) return linuxWorkingDirectory;
    if (!workingDirectory.empty()) return workingDirectory;
    if (!macWorkingDirectory.empty()) return macWorkingDirectory;
#elif defined(__APPLE__)
    if (!macWorkingDirectory.empty()) return macWorkingDirectory;
    if (!workingDirectory.empty()) return workingDirectory;
    if (!linuxWorkingDirectory.empty()) return linuxWorkingDirectory;
#endif
    return "";
}
