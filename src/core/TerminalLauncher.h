#pragma once
#include "Types.h"
#include <map>
#include <string>
#include <vector>

class TerminalLauncher {
public:
    // 启动终端
    static bool Launch(const Profile& profile, std::string* errorMsg = nullptr);
    
    // 获取当前平台可用的终端类型
    static std::vector<TerminalType> GetAvailableTerminals();
    
    // 获取终端类型的显示名称
    static std::string GetTerminalDisplayName(TerminalType type);
    
    // 检查特定终端是否可用
    static bool IsTerminalAvailable(TerminalType type);
    
private:
    // 构建环境变量映射
    static std::map<std::string, std::string> BuildEnvironment(
        const std::vector<EnvVariable>& customVars
    );
    
    // 自动检测最佳终端
    static TerminalType AutoDetectTerminal();
    
    // 平台特定实现
#ifdef _WIN32
    static bool LaunchWindows(const Profile& profile, 
                              const std::map<std::string, std::string>& env,
                              std::string* errorMsg);
#elif defined(__linux__)
    static bool LaunchLinux(const Profile& profile,
                            const std::map<std::string, std::string>& env,
                            std::string* errorMsg);
#elif defined(__APPLE__)
    static bool LaunchMacOS(const Profile& profile,
                            const std::map<std::string, std::string>& env,
                            std::string* errorMsg);
#endif
};
