#pragma once
#include "Types.h"
#include <string>
#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

class ConfigManager {
public:
    static ConfigManager& GetInstance();
    
    // 禁止拷贝
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // 初始化（指定应用程序目录）
    bool Initialize(const fs::path& appDir);
    
    // 配置文件操作
    bool LoadConfig();
    bool SaveConfig();
    
    // Profile 操作
    const std::vector<Profile>& GetProfiles() const { return m_config.profiles; }
    const Profile* GetProfile(const std::string& id) const;
    Profile* GetProfileMutable(const std::string& id);
    
    void AddProfile(const Profile& profile);
    void UpdateProfile(const std::string& id, const Profile& profile);
    void DeleteProfile(const std::string& id);
    Profile DuplicateProfile(const std::string& id);
    
    // 导入导出
    bool ExportConfig(const fs::path& filePath);
    bool ImportConfig(const fs::path& filePath);
    
    // 设置
    const AppSettings& GetSettings() const { return m_config.settings; }
    void UpdateSettings(const AppSettings& settings);
    
    // 备份
    bool CreateBackup();
    
    // 获取数据目录
    fs::path GetDataDir() const { return m_dataDir; }
    
private:
    ConfigManager() = default;
    
    AppConfig m_config;
    fs::path m_dataDir;
    fs::path m_configPath;
    
    void EnsureDataDirectory();
    std::string GenerateUuid();
    std::string GetCurrentTimestamp();
};
