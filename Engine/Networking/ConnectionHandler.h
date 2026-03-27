#pragma once
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "SocketUtils.h"
using namespace std;
#ifdef _WIN32
extern "C" {
#include <openssl/applink.c>
}
#endif // _WIN32



enum ClientState {
    STATE_HANDSHAKING,
    STATE_CONNECTED,
    STATE_DISCONNECTED
};

struct ClientInfo {
    socket_t socket;
    int localPort;
    SSL* ssl = nullptr;
    ClientState state = STATE_CONNECTED;
    char* buffer = nullptr;
    size_t bufferSize = 0;
    size_t bytesRead = 0;
};


