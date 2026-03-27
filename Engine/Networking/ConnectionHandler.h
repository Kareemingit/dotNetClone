#pragma once
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "SocketUtils.h"
#ifdef _WIN32
extern "C" {
#include <openssl/applink.c>
}
#endif // _WIN32



enum ClientState {
    STATE_HANDSHAKING,
    STATE_CONNECTED,
    STATE_DISCONNECTED,
    STATE_PROCESSING
};

struct http_client {
    socket_t socket;
    int localPort;
    SSL* ssl = nullptr;
    ClientState state = STATE_CONNECTED;
    size_t bufferSize = 0;
    size_t bytesRead = 0;
};


