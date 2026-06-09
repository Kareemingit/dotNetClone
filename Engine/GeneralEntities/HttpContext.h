#pragma once
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "..\Networking\SocketUtils.h"
#ifdef _WIN32
extern "C" {
#include <openssl/applink.c>
}
#endif // _WIN32
#define HEADER_MAX_COUNT 32

using namespace std;
struct MyData {
    int id;
    float value;
    char message[256];
};


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



#pragma pack(push , 1)
struct http_request {
    // 64-bit pointers/handles first (8 bytes)
    std::vector<char> buffer_raw_ptr;// The heap-allocated raw HTTP string
    uintptr_t client_socket;// Use uintptr_t for cross-platform socket handles

    // 32-bit offsets (4 bytes)
    uint32_t uri_offset;
    uint32_t uri_len;
    uint32_t body_start_offset;
    uint32_t content_length;//vital for body parsing

    // 16-bit / 8-bit values (2 or 1 bytes)
    uint16_t method_offset;
    uint16_t method_len;
    uint16_t num_headers;

    uint8_t method_id;
    uint8_t version_major;
    uint8_t version_minor;

    // Header array (Fixed size for stack/inline efficiency)
    struct header_field {
        uint32_t name_offset;
        uint32_t name_len;
        uint32_t value_offset;
        uint32_t value_len;
    } headers[HEADER_MAX_COUNT];
};
#pragma pack(pop)

struct http_response {
    void* buffer_raw_ptr;
	size_t buffer_size;
    uintptr_t client_socket;
};
