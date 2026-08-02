// ===========================================
// Sockets Module - 原版 AMX Mod X sockets.inc 签名
// 8 个 native: socket_open/close/recv/send/send2/change/is_readable/is_writable
// Windows 使用 Winsock2, 其他平台使用 POSIX sockets
// ===========================================

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#endif

#include "precompiled.h"
#include "native_sockets.h"
#include "amx.h"

#include <cstdio>
#include <cstring>
#include <vector>

// ===========================================
// 常量定义 (与原版 AMX Mod X sockets.inc 一致)
// ===========================================
#define SOCKET_TCP 1
#define SOCKET_UDP 2

#define SOCK_NON_BLOCKING (1 << 0) /* Set the socket a nonblocking */
#define SOCK_LIBC_ERRORS  (1 << 1) /* Enable libc error reporting */

#define SOCK_ERROR_OK               0 /* No error */
#define SOCK_ERROR_CREATE_SOCKET    1 /* Couldn't create a socket */
#define SOCK_ERROR_SERVER_UNKNOWN   2 /* Server unknown */
#define SOCK_ERROR_WHILE_CONNECTING 3 /* Error while connecting */

// libc 错误码 (用于空主机名 + SOCK_LIBC_ERRORS 场景)
#define ERROR_EHOSTUNREACH          113  // No route to host

// ===========================================
// 平台相关辅助
// ===========================================
#ifdef _WIN32
typedef SOCKET sock_t;
#define INVALID_SOCK INVALID_SOCKET
#define SOCK_CLOSE(s) closesocket(s)
#define SOCK_ERRNO() WSAGetLastError()
#else
typedef int sock_t;
#define INVALID_SOCK (-1)
#define SOCK_CLOSE(s) close(s)
#define SOCK_ERRNO() errno
#endif

// 非阻塞 connect 正在建立连接中的错误码
static bool IsConnectInProgress(int err)
{
#ifdef _WIN32
    return (err == WSAEINPROGRESS || err == WSAEWOULDBLOCK);
#else
    return (err == EINPROGRESS || err == EWOULDBLOCK);
#endif
}

// 将 socket 设置为非阻塞模式
static bool SetNonBlocking(sock_t s)
{
#ifdef _WIN32
    u_long flags = 1;
    return ioctlsocket(s, FIONBIO, &flags) == 0;
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags == -1)
        return false;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

#ifdef _WIN32
static bool g_wsaInitialized = false;
static bool EnsureWinsock()
{
    if (g_wsaInitialized)
        return true;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return false;
    g_wsaInitialized = true;
    return true;
}
#endif

// ===========================================
// Socket 句柄管理 (1-based, 0 无效)
// ===========================================
struct SocketEntry {
    sock_t sock;
    int protocol;  // SOCKET_TCP / SOCKET_UDP
    bool inUse;
};

static std::vector<SocketEntry> s_sockets;

static int AllocSocketId(sock_t s, int protocol)
{
    for (size_t i = 0; i < s_sockets.size(); i++) {
        if (!s_sockets[i].inUse) {
            s_sockets[i].sock = s;
            s_sockets[i].protocol = protocol;
            s_sockets[i].inUse = true;
            return (int)(i + 1);
        }
    }
    SocketEntry e;
    e.sock = s;
    e.protocol = protocol;
    e.inUse = true;
    s_sockets.push_back(e);
    return (int)s_sockets.size();
}

// 校验 socket 描述符有效性: 负值或超出表范围返回 nullptr
static SocketEntry *GetSocketEntry(int id)
{
    if (id < 1 || id > (int)s_sockets.size())
        return nullptr;
    SocketEntry *e = &s_sockets[id - 1];
    return e->inUse ? e : nullptr;
}

static void FreeSocketId(int id)
{
    if (id < 1 || id > (int)s_sockets.size())
        return;
    s_sockets[id - 1].inUse = false;
    s_sockets[id - 1].sock = INVALID_SOCK;
}

// 读取 AMX 字符串参数 (hostname 等)
static void AmxGetString(AMX *amx, cell param, char *out, size_t outLen)
{
    cell *addr = nullptr;
    amx_GetAddr(amx, param, &addr);
    if (addr) {
        amx_GetString(out, addr, 0, (int)outLen);
    } else {
        out[0] = '\0';
    }
}

// ============================================
// Native 实现
// ============================================

// socket_open(const _hostname[], _port, _protocol = SOCKET_TCP, &_error, _flags = 0)
// 返回 socket descriptor (正数) 或 -1
cell AMX_NATIVE_CALL amxx_socket_open(AMX *amx, cell *params)
{
#ifdef _WIN32
    if (!EnsureWinsock())
        return -1;
#endif

    int numParams = (int)(params[0] / sizeof(cell));

    char hostname[256];
    AmxGetString(amx, params[1], hostname, sizeof(hostname));

    int port = (int)params[2];
    int protocol = (int)params[3];

    // _error 必填无默认值, 始终写回
    cell *err_addr = nullptr;
    amx_GetAddr(amx, params[4], &err_addr);
    if (err_addr)
        *err_addr = SOCK_ERROR_OK;

    // _flags 可选 (默认 0), 仅在实参数量 >= 5 时读取
    bool nonblocking = false;
    bool libc_errors = false;
    if (numParams >= 5) {
        unsigned int flags = (unsigned int)params[5];
        nonblocking = (flags & SOCK_NON_BLOCKING) != 0;
        libc_errors = (flags & SOCK_LIBC_ERRORS) != 0;
    }

    if (hostname[0] == '\0') {
        if (err_addr)
            *err_addr = libc_errors ? ERROR_EHOSTUNREACH : SOCK_ERROR_SERVER_UNKNOWN;
        return -1;
    }

    char portStr[6];
    snprintf(portStr, sizeof(portStr), "%d", port);

    struct addrinfo hints;
    struct addrinfo *server_info = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = (protocol == SOCKET_UDP) ? SOCK_DGRAM : SOCK_STREAM;

    int gaiErr = getaddrinfo(hostname, portStr, &hints, &server_info);
    if (gaiErr != 0) {
        if (err_addr)
            *err_addr = libc_errors ? gaiErr : SOCK_ERROR_SERVER_UNKNOWN;
        return -1;
    }

    sock_t sockfd = INVALID_SOCK;
    bool haveSocket = false;

    for (struct addrinfo *server = server_info; server != nullptr; server = server->ai_next) {
        sockfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
        if (sockfd == INVALID_SOCK) {
            if (err_addr && *err_addr == SOCK_ERROR_OK)
                *err_addr = libc_errors ? SOCK_ERRNO() : SOCK_ERROR_CREATE_SOCKET;
            continue;
        }

        if (nonblocking && !SetNonBlocking(sockfd)) {
            if (err_addr && *err_addr == SOCK_ERROR_OK)
                *err_addr = libc_errors ? SOCK_ERRNO() : SOCK_ERROR_CREATE_SOCKET;
            SOCK_CLOSE(sockfd);
            sockfd = INVALID_SOCK;
            continue;
        }

        if (connect(sockfd, server->ai_addr, (int)server->ai_addrlen) == 0) {
            haveSocket = true;
            break;
        }

        int connectErr = SOCK_ERRNO();
        if (nonblocking && IsConnectInProgress(connectErr)) {
            // 非阻塞 connect 仍在进行, socket 保留, 由 socket_is_writable() 轮询
            haveSocket = true;
            break;
        }

        if (err_addr && *err_addr == SOCK_ERROR_OK)
            *err_addr = libc_errors ? connectErr : SOCK_ERROR_WHILE_CONNECTING;
        SOCK_CLOSE(sockfd);
        sockfd = INVALID_SOCK;
    }

    freeaddrinfo(server_info);

    if (!haveSocket)
        return -1;

    int id = AllocSocketId(sockfd, protocol);
    return (cell)id;
}

// socket_close(_socket)
// 1 成功 0 失败
cell AMX_NATIVE_CALL amxx_socket_close(AMX *amx, cell *params)
{
    int id = (int)params[1];
    SocketEntry *e = GetSocketEntry(id);
    if (!e)
        return 0;

    SOCK_CLOSE(e->sock);
    FreeSocketId(id);
    return 1;
}

// socket_recv(_socket, _data[], _length)
// 返回接收字节数 0=对端关闭 -1=失败
cell AMX_NATIVE_CALL amxx_socket_recv(AMX *amx, cell *params)
{
    int id = (int)params[1];
    SocketEntry *e = GetSocketEntry(id);
    if (!e)
        return -1;

    int length = (int)params[3];
    if (length < 1)
        length = 1;  // 按 max(1, _length) 限制, _length 按字节计

    cell *dest = nullptr;
    if (amx_GetAddr(amx, params[2], &dest) != AMX_ERR_NONE || !dest)
        return -1;

    std::vector<char> buf((size_t)length);
    int rc = recv(e->sock, buf.data(), length, 0);
    if (rc < 0)
        return -1;

    // 按字节拷贝到 cell 数组
    for (int i = 0; i < rc; i++)
        dest[i] = (cell)(unsigned char)buf[(size_t)i];

    // 有剩余空间时补上字符串终止符
    if (rc < length)
        dest[rc] = 0;

    return (cell)rc;
}

// socket_send(_socket, const _data[], _length)
// 返回发送字节数 -1=失败
cell AMX_NATIVE_CALL amxx_socket_send(AMX *amx, cell *params)
{
    int id = (int)params[1];
    SocketEntry *e = GetSocketEntry(id);
    if (!e)
        return -1;

    int length = (int)params[3];
    if (length < 1)
        length = 1;  // 按 max(1, _length) 限制, _length 按字节计

    cell *src = nullptr;
    if (amx_GetAddr(amx, params[2], &src) != AMX_ERR_NONE || !src)
        return -1;

    std::vector<char> buf((size_t)length);
    for (int i = 0; i < length; i++)
        buf[(size_t)i] = (char)(src[i] & 0xFF);

    int rc = send(e->sock, buf.data(), length, 0);
    return (cell)rc;
}

// socket_send2(_socket, const _data[], _length)
// 支持含 null 字节的数据, 直接用 send 原字节 (不依赖字符串终止符)
// 返回发送字节数 -1=失败
cell AMX_NATIVE_CALL amxx_socket_send2(AMX *amx, cell *params)
{
    int id = (int)params[1];
    SocketEntry *e = GetSocketEntry(id);
    if (!e)
        return -1;

    int length = (int)params[3];
    if (length < 1)
        length = 1;

    cell *src = nullptr;
    if (amx_GetAddr(amx, params[2], &src) != AMX_ERR_NONE || !src)
        return -1;

    std::vector<char> buf((size_t)length);
    for (int i = 0; i < length; i++)
        buf[(size_t)i] = (char)(src[i] & 0xFF);

    return (cell)send(e->sock, buf.data(), length, 0);
}

// socket_change(_socket, _timeout = 100000)
// deprecated, 等价 socket_is_readable
cell AMX_NATIVE_CALL amxx_socket_change(AMX *amx, cell *params)
{
    return amxx_socket_is_readable(amx, params);
}

// 用 select() 检查 socket 是否可读/可写, _timeout 微秒 (0 不阻塞)
static int PollSocketReady(int id, bool writable, int timeoutUsec)
{
    SocketEntry *e = GetSocketEntry(id);
    if (!e)
        return 0;

    // Windows 上 select 的 timeout 结构需手动填 (POSIX select 会修改 timeval)
    struct timeval tv;
    tv.tv_sec = timeoutUsec / 1000000;
    tv.tv_usec = timeoutUsec % 1000000;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(e->sock, &fds);

    int rc;
#ifdef _WIN32
    rc = select(0, writable ? nullptr : &fds, writable ? &fds : nullptr, nullptr, &tv);
#else
    rc = select((int)e->sock + 1, writable ? nullptr : &fds, writable ? &fds : nullptr, nullptr, &tv);
#endif
    return rc > 0 ? 1 : 0;
}

// socket_is_readable(_socket, _timeout = 100000)
// 1 可读 0 不可读
cell AMX_NATIVE_CALL amxx_socket_is_readable(AMX *amx, cell *params)
{
    int id = (int)params[1];
    // _timeout 可选, 默认 100000 微秒
    int timeout = 100000;
    if ((int)(params[0] / sizeof(cell)) >= 2)
        timeout = (int)params[2];
    return PollSocketReady(id, false, timeout);
}

// socket_is_writable(_socket, _timeout = 100000)
// 1 可写 0 不可写
cell AMX_NATIVE_CALL amxx_socket_is_writable(AMX *amx, cell *params)
{
    int id = (int)params[1];
    // _timeout 可选, 默认 100000 微秒
    int timeout = 100000;
    if ((int)(params[0] / sizeof(cell)) >= 2)
        timeout = (int)params[2];
    return PollSocketReady(id, true, timeout);
}

// ===== Native registration =====

AMX_NATIVE_INFO sockets_natives[] = {
    {"socket_open", amxx_socket_open},
    {"socket_close", amxx_socket_close},
    {"socket_recv", amxx_socket_recv},
    {"socket_send", amxx_socket_send},
    {"socket_send2", amxx_socket_send2},
    {"socket_change", amxx_socket_change},
    {"socket_is_readable", amxx_socket_is_readable},
    {"socket_is_writable", amxx_socket_is_writable},
    {nullptr, nullptr}
};

void RegisterSocketsNatives(AMX *amx)
{
    amx_Register(amx, sockets_natives, -1);
}

// 清理 sockets 状态 (地图切换时调用)
void ResetSocketsGlobals()
{
    for (size_t i = 0; i < s_sockets.size(); i++) {
        if (s_sockets[i].inUse) {
            SOCK_CLOSE(s_sockets[i].sock);
            s_sockets[i].inUse = false;
            s_sockets[i].sock = INVALID_SOCK;
        }
    }
}
