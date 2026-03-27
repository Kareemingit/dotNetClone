#pragma once

struct MyData {
    int id;
    float value;
    char message[256];
};

#pragma pack(push , 1)
struct http_request {
    // 64-bit pointers/handles first (8 bytes)
    void* buffer_raw_ptr;// The heap-allocated raw HTTP string
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
    } headers[32];
};
#pragma pack(pop)
