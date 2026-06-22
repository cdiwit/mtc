#include "ConfigManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <random>
#include <algorithm>
#include <chrono>

using json = nlohmann::json;

ConfigManager& ConfigManager::GetInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::Initialize(const fs::path& appDir) {
    m_dataDir = appDir / "data";
    m_configPath = m_dataDir / "config.json";
    
    EnsureDataDirectory();
    return LoadConfig();
}

void ConfigManager::EnsureDataDirectory() {
    // 确保 data 目录存在
    if (!fs::exists(m_dataDir)) {
        fs::create_directories(m_dataDir);
    }
    
    // 确保备份目录存在
    fs::path backupDir = m_dataDir / "backups";
    if (!fs::exists(backupDir)) {
        fs::create_directories(backupDir);
    }
}

bool ConfigManager::LoadConfig() {
    if (!fs::exists(m_configPath)) {
        // 创建默认配置
        m_config = AppConfig();
        return SaveConfig();
    }
    
    try {
        std::ifstream file(m_configPath);
        if (!file.is_open()) {
            return false;
        }
        
        json j;
        file >> j;
        
        m_config.version = j.value("version", "1.0");
        m_config.profiles.clear();
        
        // 解析 profiles
        if (j.contains("profiles")) {
            for (const auto& jp : j["profiles"]) {
                Profile profile;
                profile.id = jp.value("id", "");
                profile.name = jp.value("name", "");
                profile.description = jp.value("description", "");
                profile.workingDirectory = jp.value("workingDirectory", "");
                profile.linuxWorkingDirectory = jp.value("linuxWorkingDirectory", "");
                profile.macWorkingDirectory = jp.value("macWorkingDirectory", "");
                
                // 解析终端类型
                std::string termType = jp.value("terminalType", "auto");
                profile.terminalType = StringToTerminalType(termType);
                
                // 解析环境变量
                profile.environmentVariables.clear();
                if (jp.contains("environmentVariables")) {
                    for (const auto& jv : jp["environmentVariables"]) {
                        EnvVariable var;
                        var.name = jv.value("name", "");
                        var.value = jv.value("value", "");
                        if (!var.name.empty()) {
                            profile.environmentVariables.push_back(var);
                        }
                    }
                }
                
                // 解析启动命令
                profile.startupCommands.clear();
                if (jp.contains("startupCommands")) {
                    for (const auto& cmd : jp["startupCommands"]) {
                        if (cmd.is_string()) {
                            std::string s = cmd.get<std::string>();
                            if (!s.empty()) {
                                // 修复 macOS 自动纠正：em dash(U+2014) → --
                                const std::string emDash = "\xe2\x80\x94";
                                const std::string enDash = "\xe2\x80\x93";
                                size_t pos = 0;
                                while ((pos = s.find(emDash, pos)) != std::string::npos) {
                                    s.replace(pos, emDash.size(), "--");
                                    pos += 2;
                                }
                                pos = 0;
                                while ((pos = s.find(enDash, pos)) != std::string::npos) {
                                    s.replace(pos, enDash.size(), "--");
                                    pos += 2;
                                }
                                profile.startupCommands.push_back(s);
                            }
                        }
                    }
                }

                profile.createdAt = jp.value("createdAt", "");
                profile.updatedAt = jp.value("updatedAt", "");

                // 解析远程 SSH 配置（向后兼容：旧配置无这些字段时读空）
                profile.sshHostId = jp.value("sshHostId", "");
                profile.credentialId = jp.value("credentialId", "");
                profile.remoteWorkingDirectory = jp.value("remoteWorkingDirectory", "");

                m_config.profiles.push_back(profile);
            }
        }

        // 解析 SSH 主机
        m_config.sshHosts.clear();
        if (j.contains("sshHosts")) {
            for (const auto& jh : j["sshHosts"]) {
                SshHost host;
                host.id = jh.value("id", "");
                host.name = jh.value("name", "");
                host.host = jh.value("host", "");
                host.port = jh.value("port", 22);
                host.username = jh.value("username", "");
                host.createdAt = jh.value("createdAt", "");
                host.updatedAt = jh.value("updatedAt", "");
                if (!host.id.empty()) {
                    m_config.sshHosts.push_back(host);
                }
            }
        }

        // 解析凭据（仅元数据，秘密在系统钥匙串）
        m_config.credentials.clear();
        if (j.contains("credentials")) {
            for (const auto& jc : j["credentials"]) {
                Credential cred;
                cred.id = jc.value("id", "");
                cred.name = jc.value("name", "");
                cred.type = StringToCredentialType(jc.value("type", "password"));
                cred.keyPath = jc.value("keyPath", "");
                cred.createdAt = jc.value("createdAt", "");
                cred.updatedAt = jc.value("updatedAt", "");
                if (!cred.id.empty()) {
                    m_config.credentials.push_back(cred);
                }
            }
        }

        // 解析设置
        if (j.contains("settings")) {
            const auto& js = j["settings"];
            std::string defaultTerm = js.value("defaultTerminalType", "auto");
            m_config.settings.defaultTerminalType = StringToTerminalType(defaultTerm);
            m_config.settings.language = js.value("language", "zh-CN");
            m_config.settings.theme = js.value("theme", "system");
            m_config.settings.autoBackup = js.value("autoBackup", true);

            m_config.settings.searchHistory.clear();
            if (js.contains("searchHistory") && js["searchHistory"].is_array()) {
                for (const auto& item : js["searchHistory"]) {
                    if (item.is_string()) {
                        m_config.settings.searchHistory.push_back(item.get<std::string>());
                    }
                }
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        // 配置文件解析失败，使用默认配置
        m_config = AppConfig();
        return false;
    }
}

bool ConfigManager::SaveConfig() {
    try {
        // 自动备份（仅当配置文件存在时）
        if (m_config.settings.autoBackup && fs::exists(m_configPath)) {
            CreateBackup();
        }
        
        json j;
        j["version"] = m_config.version;
        
        // 序列化 profiles
        j["profiles"] = json::array();
        for (const auto& profile : m_config.profiles) {
            json jp;
            jp["id"] = profile.id;
            jp["name"] = profile.name;
            jp["description"] = profile.description;
            jp["workingDirectory"] = profile.workingDirectory;
            jp["linuxWorkingDirectory"] = profile.linuxWorkingDirectory;
            jp["macWorkingDirectory"] = profile.macWorkingDirectory;
            jp["terminalType"] = TerminalTypeToString(profile.terminalType);
            
            // 序列化环境变量
            jp["environmentVariables"] = json::array();
            for (const auto& var : profile.environmentVariables) {
                jp["environmentVariables"].push_back({
                    {"name", var.name},
                    {"value", var.value}
                });
            }
            
            // 序列化启动命令
            jp["startupCommands"] = json::array();
            for (const auto& cmd : profile.startupCommands) {
                jp["startupCommands"].push_back(cmd);
            }

            jp["createdAt"] = profile.createdAt;
            jp["updatedAt"] = profile.updatedAt;

            // 远程 SSH 配置
            jp["sshHostId"] = profile.sshHostId;
            jp["credentialId"] = profile.credentialId;
            jp["remoteWorkingDirectory"] = profile.remoteWorkingDirectory;

            j["profiles"].push_back(jp);
        }

        // 序列化 SSH 主机
        j["sshHosts"] = json::array();
        for (const auto& host : m_config.sshHosts) {
            j["sshHosts"].push_back({
                {"id", host.id},
                {"name", host.name},
                {"host", host.host},
                {"port", host.port},
                {"username", host.username},
                {"createdAt", host.createdAt},
                {"updatedAt", host.updatedAt}
            });
        }

        // 序列化凭据（仅元数据，不含密码/口令）
        j["credentials"] = json::array();
        for (const auto& cred : m_config.credentials) {
            j["credentials"].push_back({
                {"id", cred.id},
                {"name", cred.name},
                {"type", CredentialTypeToString(cred.type)},
                {"keyPath", cred.keyPath},
                {"createdAt", cred.createdAt},
                {"updatedAt", cred.updatedAt}
            });
        }

        // 序列化设置
        j["settings"] = {
            {"defaultTerminalType", TerminalTypeToString(m_config.settings.defaultTerminalType)},
            {"language", m_config.settings.language},
            {"theme", m_config.settings.theme},
            {"autoBackup", m_config.settings.autoBackup}
        };

        json searchHistoryJson = json::array();
        for (const auto& item : m_config.settings.searchHistory) {
            searchHistoryJson.push_back(item);
        }
        j["settings"]["searchHistory"] = searchHistoryJson;
        
        // 写入文件
        std::ofstream file(m_configPath);
        if (!file.is_open()) {
            return false;
        }
        
        file << std::setw(2) << j << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool ConfigManager::CreateBackup() {
    if (!fs::exists(m_configPath)) {
        return false;
    }
    
    // 生成备份文件名
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    
    std::ostringstream oss;
    oss << "config_" << std::put_time(&tm, "%Y-%m-%d_%H%M%S") << ".json";
    
    fs::path backupPath = m_dataDir / "backups" / oss.str();
    
    try {
        fs::copy_file(m_configPath, backupPath, fs::copy_options::overwrite_existing);
        
        // 只保留最近 10 个备份
        std::vector<fs::path> backups;
        for (const auto& entry : fs::directory_iterator(m_dataDir / "backups")) {
            if (entry.path().extension() == ".json") {
                backups.push_back(entry.path());
            }
        }
        
        if (backups.size() > 10) {
            std::sort(backups.begin(), backups.end());
            for (size_t i = 0; i < backups.size() - 10; ++i) {
                fs::remove(backups[i]);
            }
        }
        
        return true;
    }
    catch (...) {
        return false;
    }
}

const Profile* ConfigManager::GetProfile(const std::string& id) const {
    for (const auto& p : m_config.profiles) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

Profile* ConfigManager::GetProfileMutable(const std::string& id) {
    for (auto& p : m_config.profiles) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

void ConfigManager::AddProfile(const Profile& profile) {
    Profile newProfile = profile;
    newProfile.id = GenerateUuid();
    newProfile.createdAt = GetCurrentTimestamp();
    newProfile.updatedAt = newProfile.createdAt;
    
    m_config.profiles.push_back(newProfile);
    SaveConfig();
}

void ConfigManager::UpdateProfile(const std::string& id, const Profile& profile) {
    for (auto& p : m_config.profiles) {
        if (p.id == id) {
            std::string oldId = p.id;
            std::string oldCreatedAt = p.createdAt;
            p = profile;
            p.id = oldId;
            p.createdAt = oldCreatedAt;
            p.updatedAt = GetCurrentTimestamp();
            break;
        }
    }
    SaveConfig();
}

void ConfigManager::DeleteProfile(const std::string& id) {
    m_config.profiles.erase(
        std::remove_if(m_config.profiles.begin(), m_config.profiles.end(),
            [&id](const Profile& p) { return p.id == id; }),
        m_config.profiles.end()
    );
    SaveConfig();
}

Profile ConfigManager::DuplicateProfile(const std::string& id) {
    const Profile* original = GetProfile(id);
    if (!original) {
        return Profile();
    }

    Profile newProfile = *original;
    newProfile.name = original->name + " (副本)";
    newProfile.id = GenerateUuid();
    newProfile.createdAt = GetCurrentTimestamp();
    newProfile.updatedAt = newProfile.createdAt;

    m_config.profiles.push_back(newProfile);
    SaveConfig();

    return newProfile;
}

// ===== SSH 主机 =====
const SshHost* ConfigManager::GetSshHost(const std::string& id) const {
    for (const auto& h : m_config.sshHosts) {
        if (h.id == id) {
            return &h;
        }
    }
    return nullptr;
}

void ConfigManager::AddSshHost(const SshHost& host) {
    SshHost newHost = host;
    newHost.id = GenerateUuid();
    newHost.createdAt = GetCurrentTimestamp();
    newHost.updatedAt = newHost.createdAt;
    m_config.sshHosts.push_back(newHost);
    SaveConfig();
}

void ConfigManager::UpdateSshHost(const std::string& id, const SshHost& host) {
    for (auto& h : m_config.sshHosts) {
        if (h.id == id) {
            std::string oldId = h.id;
            std::string oldCreatedAt = h.createdAt;
            h = host;
            h.id = oldId;
            h.createdAt = oldCreatedAt;
            h.updatedAt = GetCurrentTimestamp();
            break;
        }
    }
    SaveConfig();
}

void ConfigManager::DeleteSshHost(const std::string& id) {
    m_config.sshHosts.erase(
        std::remove_if(m_config.sshHosts.begin(), m_config.sshHosts.end(),
            [&id](const SshHost& h) { return h.id == id; }),
        m_config.sshHosts.end()
    );
    // 清理引用了该主机的 Profile（置空，退化为本地终端）
    for (auto& p : m_config.profiles) {
        if (p.sshHostId == id) {
            p.sshHostId.clear();
        }
    }
    SaveConfig();
}

// ===== 凭据 =====
const Credential* ConfigManager::GetCredential(const std::string& id) const {
    for (const auto& c : m_config.credentials) {
        if (c.id == id) {
            return &c;
        }
    }
    return nullptr;
}

Credential ConfigManager::AddCredential(const Credential& cred) {
    Credential newCred = cred;
    newCred.id = GenerateUuid();
    newCred.createdAt = GetCurrentTimestamp();
    newCred.updatedAt = newCred.createdAt;
    m_config.credentials.push_back(newCred);
    SaveConfig();
    return newCred;
}

void ConfigManager::UpdateCredential(const std::string& id, const Credential& cred) {
    for (auto& c : m_config.credentials) {
        if (c.id == id) {
            std::string oldId = c.id;
            std::string oldCreatedAt = c.createdAt;
            c = cred;
            c.id = oldId;
            c.createdAt = oldCreatedAt;
            c.updatedAt = GetCurrentTimestamp();
            break;
        }
    }
    SaveConfig();
}

void ConfigManager::DeleteCredential(const std::string& id) {
    m_config.credentials.erase(
        std::remove_if(m_config.credentials.begin(), m_config.credentials.end(),
            [&id](const Credential& c) { return c.id == id; }),
        m_config.credentials.end()
    );
    // 清理引用了该凭据的 Profile
    for (auto& p : m_config.profiles) {
        if (p.credentialId == id) {
            p.credentialId.clear();
        }
    }
    SaveConfig();
}

bool ConfigManager::ExportConfig(const fs::path& filePath) {
    try {
        fs::copy_file(m_configPath, filePath, fs::copy_options::overwrite_existing);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool ConfigManager::ImportConfig(const fs::path& filePath) {
    // 先备份当前配置
    CreateBackup();
    
    try {
        // 验证导入文件格式
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }
        
        json j;
        file >> j;
        
        // 检查是否包含必要字段
        if (!j.contains("profiles")) {
            return false;
        }
        
        file.close();
        
        // 复制文件
        fs::copy_file(filePath, m_configPath, fs::copy_options::overwrite_existing);
        
        // 重新加载
        return LoadConfig();
    }
    catch (...) {
        return false;
    }
}

void ConfigManager::UpdateSettings(const AppSettings& settings) {
    m_config.settings = settings;
    SaveConfig();
}

void ConfigManager::AddSearchHistory(const std::string& keyword) {
    ::AddToSearchHistory(m_config.settings.searchHistory, keyword);
    SaveConfig();
}

void ConfigManager::RemoveSearchHistory(const std::string& keyword) {
    ::RemoveFromSearchHistory(m_config.settings.searchHistory, keyword);
    SaveConfig();
}

void ConfigManager::ClearSearchHistory() {
    ::ClearSearchHistory(m_config.settings.searchHistory);
    SaveConfig();
}

std::string ConfigManager::GenerateUuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    const char* hex = "0123456789abcdef";
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    
    for (char& c : uuid) {
        if (c == 'x') {
            c = hex[dis(gen)];
        } else if (c == 'y') {
            c = hex[(dis(gen) & 0x3) | 0x8];
        }
    }
    
    return uuid;
}

std::string ConfigManager::GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S+08:00");
    return oss.str();
}
