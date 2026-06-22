#pragma once
#include <string>

// 系统钥匙串抽象（跨平台）
//
// 存储 SSH 凭据里的秘密（密码 / 私钥口令）。
// 调用方用稳定 key 读写，例如：
//   SecretStore::Set("mtc:cred:<id>:password", "P@ssw0rd");
//   SecretStore::Get("mtc:cred:<id>:password", out);
//
// 平台实现：
//   macOS   - Security 框架（Keychain，kSecClassGenericPassword）
//   Windows - 凭据管理器（CredWriteW/CredReadW/CredDeleteW）
//   Linux   - libsecret（Secret Service D-Bus）
class SecretStore {
public:
    // 写入/覆盖一条秘密。返回是否成功。
    static bool Set(const std::string& key, const std::string& secret);

    // 读取一条秘密。返回是否成功（key 不存在也算失败）。
    static bool Get(const std::string& key, std::string& outSecret);

    // 删除一条秘密（不存在也返回 true）。
    static bool Delete(const std::string& key);

    // 钥匙串里的统一服务名 / 目标前缀。
    static constexpr const char* kServiceName = "com.mtc.terminal-manager";
};
