#pragma once
#include "Types.h"
#include "SearchHistory.h"
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

    // SSH 主机操作
    const std::vector<SshHost>& GetSshHosts() const { return m_config.sshHosts; }
    const SshHost* GetSshHost(const std::string& id) const;
    void AddSshHost(const SshHost& host);
    void UpdateSshHost(const std::string& id, const SshHost& host);
    void DeleteSshHost(const std::string& id);

    // 凭据操作（注意：真正的密码/口令存系统钥匙串，不在配置里）
    const std::vector<Credential>& GetCredentials() const { return m_config.credentials; }
    const Credential* GetCredential(const std::string& id) const;
    // 返回已存入的凭据（含新生成的 id），调用方据此把秘密写入钥匙串
    Credential AddCredential(const Credential& cred);
    void UpdateCredential(const std::string& id, const Credential& cred);
    void DeleteCredential(const std::string& id);

    // 导入导出
    bool ExportConfig(const fs::path& filePath);
    bool ImportConfig(const fs::path& filePath);
    
    // 设置
    const AppSettings& GetSettings() const { return m_config.settings; }
    void UpdateSettings(const AppSettings& settings);
    void AddSearchHistory(const std::string& keyword);
    void RemoveSearchHistory(const std::string& keyword);
    void ClearSearchHistory();
    
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
