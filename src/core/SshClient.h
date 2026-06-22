#pragma once
#include "Types.h"
#include <cstdint>
#include <string>
#include <vector>

// SSH/SFTP 客户端（封装 libssh2），仅用于远程文件浏览/下载。
// 远程终端会话不经此类，直接由 TerminalLauncher 走外部 ssh 命令。
class SshClient {
public:
    SshClient();
    ~SshClient();

    SshClient(const SshClient&) = delete;
    SshClient& operator=(const SshClient&) = delete;

    // 建立连接并按凭据类型认证。
    // 失败时返回 false，可通过 LastError() 取原因。
    bool Connect(const SshHost& host, const Credential& cred);

    bool IsConnected() const { return m_connected; }

    // 列出远程目录。RemoteEntry.name 仅含文件名（不含路径）。
    struct RemoteEntry {
        std::string name;
        bool isDir = false;
        uint64_t size = 0;
        int64_t mtime = 0;  // Unix 时间戳（秒）
    };
    std::vector<RemoteEntry> ListDir(const std::string& remotePath);

    // 下载远程文件到本地路径。
    bool DownloadFile(const std::string& remotePath, const std::string& localPath);

    void Close();

    const std::string& LastError() const { return m_lastError; }

private:
    bool m_connected = false;
    void* m_session = nullptr;   // LIBSSH2_SESSION*
    int m_socket = -1;
    std::string m_lastError;

    void SetLastError(const std::string& msg);
    static std::string PasswordKey(const std::string& credId);
    static std::string PassphraseKey(const std::string& credId);
};
