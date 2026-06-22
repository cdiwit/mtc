#include "SecretStore.h"

// ============================================================================
// macOS - Security 框架（Keychain Services）
// ============================================================================
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

static CFStringRef ServiceRef() {
    return CFStringCreateWithCString(kCFAllocatorDefault,
                                     SecretStore::kServiceName,
                                     kCFStringEncodingUTF8);
}

static CFStringRef AccountRef(const std::string& key) {
    return CFStringCreateWithCString(kCFAllocatorDefault, key.c_str(),
                                     kCFStringEncodingUTF8);
}

bool SecretStore::Set(const std::string& key, const std::string& secret) {
    // 先删除旧的（避免 SecItemAdd 因重复 account 报 errKCDuplicateItem）
    Delete(key);

    CFStringRef service = ServiceRef();
    CFStringRef account = AccountRef(key);
    CFDataRef passwordData = CFDataCreate(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(secret.data()),
        static_cast<CFIndex>(secret.size()));

    const void* keys[] = {
        kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData
    };
    const void* values[] = {
        kSecClassGenericPassword, service, account, passwordData
    };
    CFDictionaryRef query = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 4,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    OSStatus status = SecItemAdd(query, nullptr);

    CFRelease(query);
    CFRelease(passwordData);
    CFRelease(account);
    CFRelease(service);

    return status == errSecSuccess;
}

bool SecretStore::Get(const std::string& key, std::string& outSecret) {
    CFStringRef service = ServiceRef();
    CFStringRef account = AccountRef(key);

    const void* keys[] = {
        kSecClass, kSecAttrService, kSecAttrAccount,
        kSecReturnData, kSecMatchLimit
    };
    const void* values[] = {
        kSecClassGenericPassword, service, account,
        kCFBooleanTrue, kSecMatchLimitOne
    };
    CFDictionaryRef query = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 5,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDataRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, reinterpret_cast<CFTypeRef*>(&result));

    bool ok = false;
    if (status == errSecSuccess && result != nullptr) {
        const UInt8* bytes = CFDataGetBytePtr(result);
        CFIndex length = CFDataGetLength(result);
        outSecret.assign(reinterpret_cast<const char*>(bytes),
                         static_cast<size_t>(length));
        CFRelease(result);
        ok = true;
    }

    CFRelease(query);
    CFRelease(account);
    CFRelease(service);

    return ok;
}

bool SecretStore::Delete(const std::string& key) {
    CFStringRef service = ServiceRef();
    CFStringRef account = AccountRef(key);

    const void* keys[] = {
        kSecClass, kSecAttrService, kSecAttrAccount
    };
    const void* values[] = {
        kSecClassGenericPassword, service, account
    };
    CFDictionaryRef query = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    OSStatus status = SecItemDelete(query);

    CFRelease(query);
    CFRelease(account);
    CFRelease(service);

    return status == errSecSuccess || status == errSecItemNotFound;
}

// ============================================================================
// Windows - 凭据管理器（Credential Manager）
// ============================================================================
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#pragma comment(lib, "advapi32.lib")

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

static std::string WideToUtf8(const wchar_t* w, int len) {
    if (len <= 0) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w, len, &s[0], n, nullptr, nullptr);
    return s;
}

// TargetName 形如 "com.mtc.terminal-manager/<key>"
static std::wstring TargetName(const std::string& key) {
    return Utf8ToWide(std::string(kServiceName) + "/" + key);
}

bool SecretStore::Set(const std::string& key, const std::string& secret) {
    std::wstring target = TargetName(key);

    CREDENTIALW cred = {0};
    cred.Type = CRED_TYPE_GENERIC;
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.TargetName = const_cast<LPWSTR>(target.c_str());
    cred.UserName = const_cast<LPWSTR>(Utf8ToWide(kServiceName).c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(secret.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(
        const_cast<char*>(secret.data()));

    return CredWriteW(&cred, 0) == TRUE;
}

bool SecretStore::Get(const std::string& key, std::string& outSecret) {
    PCREDENTIALW cred = nullptr;
    if (CredReadW(TargetName(key).c_str(), CRED_TYPE_GENERIC, 0, &cred) != TRUE) {
        return false;
    }

    bool ok = false;
    if (cred && cred->CredentialBlob && cred->CredentialBlobSize > 0) {
        outSecret.assign(
            reinterpret_cast<const char*>(cred->CredentialBlob),
            static_cast<size_t>(cred->CredentialBlobSize));
        ok = true;
    }
    if (cred) CredFree(cred);
    return ok;
}

bool SecretStore::Delete(const std::string& key) {
    BOOL ok = CredDeleteW(TargetName(key).c_str(), CRED_TYPE_GENERIC, 0);
    // 不存在也算成功
    return ok == TRUE || GetLastError() == ERROR_NOT_FOUND;
}

// ============================================================================
// Linux - libsecret（Secret Service D-Bus API）
// ============================================================================
#elif defined(__linux__)
#include <libsecret/secret.h>

// 注意：schema 在运行时构建（非 const，因此不能静态初始化）。
static const SecretSchema* GetSchema() {
    static SecretSchema schema = {
        SecretStore::kServiceName,
        SECRET_SCHEMA_DONT_MATCH_NAME,
        { { nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING } }
    };
    return &schema;
}

bool SecretStore::Set(const std::string& key, const std::string& secret) {
    GError* error = nullptr;
    gboolean ok = secret_password_store_sync(
        GetSchema(), SECRET_COLLECTION_DEFAULT,
        ("MTC: " + key).c_str(),
        secret.c_str(),
        nullptr, &error,
        "account", key.c_str(),
        nullptr);

    if (error) {
        g_error_free(error);
    }
    return ok == TRUE;
}

bool SecretStore::Get(const std::string& key, std::string& outSecret) {
    GError* error = nullptr;
    gchar* result = secret_password_lookup_sync(
        GetSchema(), nullptr, &error,
        "account", key.c_str(),
        nullptr);

    if (error) {
        g_error_free(error);
        return false;
    }
    if (!result) {
        return false;
    }
    outSecret.assign(result);
    secret_password_free(result);
    return true;
}

bool SecretStore::Delete(const std::string& key) {
    GError* error = nullptr;
    gboolean ok = secret_password_clear_sync(
        GetSchema(), nullptr, &error,
        "account", key.c_str(),
        nullptr);

    if (error) {
        g_error_free(error);
        // 清理失败也返回 true，避免阻塞凭据删除流程
        return true;
    }
    return ok == TRUE;
}

#else
// 未知平台：无安全存储，全部失败（Get 始终返回 false）
#error "SecretStore: unsupported platform"
#endif
