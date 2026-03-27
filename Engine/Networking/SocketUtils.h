#pragma once

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#define GET_NET_ERROR WSAGetLastError()
#else
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
typedef int socket_t;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define CLOSE_SOCKET close
#define GET_NET_ERROR errno
#endif

#define HTTP_DEFAULT_PORT 80
#define HTTPS_DEFAULT_PORT 443


inline void initSockets() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif
}

inline void cleanUpSockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

inline void setNonBlocking(socket_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        // Handle error
    }
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return;
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}
