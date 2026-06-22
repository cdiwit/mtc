#include "SshClient.h"
#include "SecretStore.h"

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <cstring>
#include <fstream>
#include <algorithm>
#include <cctype>

// ===== 跨平台 socket 头文件 =====
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   typedef int socklen_t;
   // WSAStartup 只做一次
   struct WsaInit {
       WsaInit() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
       ~WsaInit() { WSACleanup(); }
   };
   static WsaInit g_wsaInit;
#  define MTC_INVALID_SOCKET INVALID_SOCKET
#  define MTC_SOCKET_ERR SOCKET_ERROR
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <arpa/inet.h>
#  define MTC_INVALID_SOCKET (-1)
#  define MTC_SOCKET_ERR (-1)
#endif

// 全局 libssh2 初始化在 main.cpp 的 OnInit/OnExit 中完成。

SshClient::SshClient() = default;

SshClient::~SshClient() {
    Close();
}

void SshClient::SetLastError(const std::string& msg) {
    m_lastError = msg;
}

std::string SshClient::PasswordKey(const std::string& credId) {
    return "mtc:cred:" + credId + ":password";
}

std::string SshClient::PassphraseKey(const std::string& credId) {
    return "mtc:cred:" + credId + ":passphrase";
}

static bool CreateConnectedSocket(const std::string& host, int port, int& outSock, std::string& err) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;       // IPv4 或 IPv6
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    std::string portStr = std::to_string(port);
    int rc = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
    if (rc != 0 || !res) {
        err = "无法解析主机 " + host + ":" + portStr;
        return false;
    }

    int sock = MTC_INVALID_SOCKET;
    std::string lastErr = "未知错误";
    for (addrinfo* p = res; p; p = p->ai_next) {
        sock = static_cast<int>(socket(p->ai_family, p->ai_socktype, p->ai_protocol));
        if (sock == MTC_INVALID_SOCKET) {
            lastErr = std::string("socket 创建失败: ") + strerror(errno);
            continue;
        }
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
            break;  // 连接成功
        }
        lastErr = std::string(strerror(errno));  // 记录原因（如 Operation not permitted）
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        sock = MTC_INVALID_SOCKET;
    }
    freeaddrinfo(res);

    if (sock == MTC_INVALID_SOCKET) {
        err = "连接 " + host + ":" + portStr + " 失败 (" + lastErr + ")";
        return false;
    }

    outSock = sock;
    return true;
}

bool SshClient::Connect(const SshHost& host, const Credential& cred) {
    Close();

    // 1. TCP 连接
    std::string sockErr;
    if (!CreateConnectedSocket(host.host, host.port, m_socket, sockErr)) {
        SetLastError(sockErr);
        return false;
    }

    // 2. 创建会话
    LIBSSH2_SESSION* session = libssh2_session_init_ex(nullptr, nullptr, nullptr, nullptr);
    if (!session) {
        SetLastError("创建 SSH 会话失败");
#ifdef _WIN32
        closesocket(m_socket);
#else
        close(m_socket);
#endif
        m_socket = -1;
        return false;
    }
    m_session = session;
    libssh2_session_set_blocking(session, 1);

    // 3. 握手
    int rc = libssh2_session_handshake(session, m_socket);
    if (rc != 0) {
        char* errmsg = nullptr;
        int errlen = 0;
        libssh2_session_last_error(session, &errmsg, &errlen, 0);
        SetLastError(std::string("SSH 握手失败: ") + (errmsg ? errmsg : "未知错误"));
        Close();
        return false;
    }

    // 4. 认证
    const char* username = host.username.c_str();
    bool authed = false;

    if (cred.type == CredentialType::Password) {
        std::string password;
        if (SecretStore::Get(PasswordKey(cred.id), password)) {
            authed = libssh2_userauth_password(session, username, password.c_str()) == 0;
            if (!authed) SetLastError("密码认证失败（用户名或密码错误）");
        } else {
            SetLastError("未在钥匙串中找到密码，请在凭据管理中重新填写");
        }
    } else if (cred.type == CredentialType::PrivateKey) {
        std::string passphrase;
        SecretStore::Get(PassphraseKey(cred.id), passphrase);  // 口令可空
        const char* passphrasePtr = passphrase.empty() ? nullptr : passphrase.c_str();
        rc = libssh2_userauth_publickey_fromfile(
            session, username,
            nullptr,                        // publicKey 自动从私钥推导
            cred.keyPath.c_str(),
            passphrasePtr);
        authed = rc == 0;
        if (!authed) {
            char* errmsg = nullptr;
            int errlen = 0;
            libssh2_session_last_error(session, &errmsg, &errlen, 0);
            SetLastError(std::string("私钥认证失败: ") + (errmsg ? errmsg : "请检查私钥路径与口令"));
        }
    } else {  // SshAgent
        LIBSSH2_AGENT* agent = libssh2_agent_init(session);
        if (agent) {
            if (libssh2_agent_connect(agent) == 0 &&
                libssh2_agent_list_identities(agent) == 0) {
                struct libssh2_agent_publickey* identity = nullptr;
                struct libssh2_agent_publickey* prev = nullptr;
                int arc = 0;
                while (libssh2_agent_get_identity(agent, &identity, prev) == 0) {
                    arc = libssh2_agent_userauth(agent, username, identity);
                    if (arc == 0) { authed = true; break; }
                    prev = identity;
                }
            }
            libssh2_agent_free(agent);
        }
        if (!authed) SetLastError("SSH Agent 认证失败（请确认 ssh-agent 已加载对应密钥）");
    }

    if (!authed) {
        if (m_lastError.empty()) SetLastError("认证失败");
        Close();
        return false;
    }

    m_connected = true;
    return true;
}

std::vector<SshClient::RemoteEntry> SshClient::ListDir(const std::string& remotePath) {
    std::vector<RemoteEntry> result;
    if (!m_connected) {
        SetLastError("未连接");
        return result;
    }

    LIBSSH2_SFTP* sftp = libssh2_sftp_init(reinterpret_cast<LIBSSH2_SESSION*>(m_session));
    if (!sftp) {
        SetLastError("初始化 SFTP 失败");
        return result;
    }

    LIBSSH2_SFTP_HANDLE* handle = libssh2_sftp_opendir(sftp, remotePath.c_str());
    if (!handle) {
        char* errmsg = nullptr;
        int errlen = 0;
        libssh2_session_last_error(reinterpret_cast<LIBSSH2_SESSION*>(m_session),
                                   &errmsg, &errlen, 0);
        SetLastError(std::string("打开目录失败: ") + (errmsg ? errmsg : remotePath));
        libssh2_sftp_shutdown(sftp);
        return result;
    }

    char namebuf[512];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = 0;
    while ((rc = libssh2_sftp_readdir(handle, namebuf, sizeof(namebuf), &attrs)) > 0) {
        std::string name(namebuf, static_cast<size_t>(rc));
        if (name == "." || name == "..") continue;

        RemoteEntry e;
        e.name = name;
        e.isDir = (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
                  ? LIBSSH2_SFTP_S_ISDIR(attrs.permissions) : false;
        e.size = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ? attrs.filesize : 0;
        e.mtime = (attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) ? attrs.mtime : 0;
        result.push_back(e);
    }

    libssh2_sftp_closedir(handle);
    libssh2_sftp_shutdown(sftp);

    if (rc < 0) {
        SetLastError("读取目录内容失败");
    }

    // 目录在前、文件在后，各自按名称排序
    std::sort(result.begin(), result.end(), [](const RemoteEntry& a, const RemoteEntry& b) {
        if (a.isDir != b.isDir) return a.isDir;
        // 大小写不敏感比较
        std::string al = a.name, bl = b.name;
        std::transform(al.begin(), al.end(), al.begin(), ::tolower);
        std::transform(bl.begin(), bl.end(), bl.begin(), ::tolower);
        return al < bl;
    });

    return result;
}

bool SshClient::DownloadFile(const std::string& remotePath, const std::string& localPath) {
    if (!m_connected) {
        SetLastError("未连接");
        return false;
    }

    LIBSSH2_SFTP* sftp = libssh2_sftp_init(reinterpret_cast<LIBSSH2_SESSION*>(m_session));
    if (!sftp) {
        SetLastError("初始化 SFTP 失败");
        return false;
    }

    LIBSSH2_SFTP_HANDLE* handle = libssh2_sftp_open(sftp, remotePath.c_str(),
                                                    LIBSSH2_FXF_READ, 0);
    if (!handle) {
        SetLastError("打开远程文件失败: " + remotePath);
        libssh2_sftp_shutdown(sftp);
        return false;
    }

    std::ofstream out(localPath, std::ios::binary);
    if (!out.is_open()) {
        SetLastError("写入本地文件失败: " + localPath);
        libssh2_sftp_close_handle(handle);
        libssh2_sftp_shutdown(sftp);
        return false;
    }

    char buf[32 * 1024];
    ssize_t n = 0;
    bool ok = true;
    while ((n = libssh2_sftp_read(handle, buf, sizeof(buf))) > 0) {
        out.write(buf, static_cast<std::streamsize>(n));
        if (!out) { ok = false; SetLastError("写入本地文件中断"); break; }
    }
    if (n < 0) { ok = false; SetLastError("读取远程文件中断"); }

    out.close();
    libssh2_sftp_close_handle(handle);
    libssh2_sftp_shutdown(sftp);
    return ok;
}

void SshClient::Close() {
    if (m_session) {
        LIBSSH2_SESSION* session = reinterpret_cast<LIBSSH2_SESSION*>(m_session);
        if (m_connected) {
            libssh2_session_disconnect(session, "MTC normal shutdown");
        }
        libssh2_session_free(session);
        m_session = nullptr;
    }
    if (m_socket != -1) {
#ifdef _WIN32
        closesocket(m_socket);
#else
        close(m_socket);
#endif
        m_socket = -1;
    }
    m_connected = false;
}
