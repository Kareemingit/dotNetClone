#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "../GeneralEntities/HttpContext.h"
using namespace std;
enum HttpMethod {
    HTTP_GET = 1,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_HEAD,
    HTTP_OPTIONS,
    HTTP_PATCH,
    HTTP_TRACE,
    HTTP_CONNECT
};

enum ParseState {
    PARSE_METHOD,
    PARSE_URI,
    PARSE_VERSION,
    PARSE_HEADER_KEY,
    PARSE_HEADER_VALUE,
    PARSE_DONE,
    PARSE_ERROR
};

class HttpParser {
private:
    static inline uint8_t detectMethod(const char* data, uint16_t len) {
        switch (len) {
        case 3:
            if (data[0] == 'G' && data[1] == 'E' && data[2] == 'T') return HTTP_GET;
            if (data[0] == 'P' && data[1] == 'U' && data[2] == 'T') return HTTP_PUT;
            break;
        case 4:
            if (memcmp(data, "POST", 4) == 0) return HTTP_POST;
            if (memcmp(data, "HEAD", 4) == 0) return HTTP_HEAD;
            break;
        case 5:
            if (memcmp(data, "PATCH", 5) == 0) return HTTP_PATCH;
            if (memcmp(data, "TRACE", 5) == 0) return HTTP_TRACE;
            break;
        case 6:
            if (memcmp(data, "DELETE", 6) == 0) return HTTP_DELETE;
            break;
        case 7:
            if (memcmp(data, "OPTIONS", 7) == 0) return HTTP_OPTIONS;
            if (memcmp(data, "CONNECT", 7) == 0) return HTTP_CONNECT;
            break;
        }
        return 255;
    }

    static int parseFirstLine(http_request* req, size_t& next) {
        const char* data = req->buffer_raw_ptr.data();
        size_t len = req->buffer_raw_ptr.size();
        size_t i = 0;
        size_t token_start = 0;
        ParseState state = PARSE_METHOD;
        while (i < len) {
            char c = data[i];
            switch (state) {
            case PARSE_METHOD:
                if (c == ' ') {
                    req->method_offset = token_start;
                    req->method_len = (uint16_t)(i - token_start);
                    req->method_id = detectMethod(data + token_start, req->method_len);
                    token_start = i + 1;
                    state = PARSE_URI;
                }
                break;
            case PARSE_URI:
                if (c == ' ') {
                    req->uri_offset = (uint32_t)token_start;
                    req->uri_len = (uint32_t)(i - token_start);
                    token_start = i + 1;
                    state = PARSE_VERSION;
                }
                break;
            case PARSE_VERSION:
                if (c == '\r') {
                    if (i + 1 >= len || data[i + 1] != '\n') {
                        state = PARSE_ERROR;
                        break;
                    }
                    size_t vlen = i - token_start;
                    if (vlen >= 8 && memcmp(data + token_start, "HTTP/", 5) == 0) {
                        req->version_major = data[token_start + 5] - '0';
                        req->version_minor = data[token_start + 7] - '0';
                    }
                    state = PARSE_DONE;
                    next = i + 2;
                    i++;
                }
                break;
            case PARSE_DONE:
                return PARSE_DONE;
            case PARSE_ERROR:
                return PARSE_ERROR;
            }
            i++;
        }
    }

    static int parseHeaders(http_request* req, size_t start) {
        const char* data = req->buffer_raw_ptr.data();
        size_t len = req->buffer_raw_ptr.size();
        size_t i = start;
        size_t token_start = start;
        ParseState state = PARSE_HEADER_KEY;
        uint16_t header_index = 0;
        while (i < len && header_index < HEADER_MAX_COUNT) {
            char c = data[i];
            switch (state) {
            case PARSE_HEADER_KEY:
                if (c == '\r' && i + 1 < len && data[i + 1] == '\n') {
                    req->num_headers = header_index;
                    req->body_start_offset = (uint32_t)(i + 2);
                    return PARSE_DONE;
                }
                if (c == ':') {
                    req->headers[header_index].name_offset = (uint32_t)token_start;
                    req->headers[header_index].name_len = (uint32_t)(i - token_start);
                    token_start = i + 1;
                    state = PARSE_HEADER_VALUE;
                }
                break;
            case PARSE_HEADER_VALUE:
                if (c == '\r') {
                    if (i + 1 >= len || data[i + 1] != '\n') {
                        return PARSE_ERROR;
                    }
                    req->headers[header_index].value_offset = (uint32_t)token_start;
                    req->headers[header_index].value_len = (uint32_t)(i - token_start);
                    header_index++;
                    token_start = i + 2;
                    state = PARSE_HEADER_KEY;
                    i++;
                }
                break;
            }
            i++;
        }
        return PARSE_DONE;
    }
public:
    static void parseRequest(http_request* req) {
        size_t next = 0;
        int firstLineState = parseFirstLine(req, next);
        if (firstLineState == PARSE_ERROR) {
            cerr << "[ERROR] Failed to parse request." << endl;
            return;
        }
        int headerState = parseHeaders(req, next);
        if (headerState == PARSE_ERROR) {
            cerr << "[ERROR] Failed to parse headers." << endl;
            return;
        }
        const vector<char>& raw = req->buffer_raw_ptr;
        req->content_length = (int)(raw.size() - req->body_start_offset);
    }

    static void printRequest(http_request* req) {
        const vector<char>& raw = req->buffer_raw_ptr;
		cout << "---- Content-Length: " << req->content_length << " ----\n";
        cout << "---- Parsed Request ----\n";

        cout << "Method: ";
        if (req->method_len > 0)
            cout.write(&raw[req->method_offset], req->method_len);
        cout << " (ID=" << (int)req->method_id << ")\n";

        cout << "URI: ";
        if (req->uri_len > 0)
            cout.write(&raw[req->uri_offset], req->uri_len);
        cout << "\n";

        cout << "Version: HTTP/"
            << (int)req->version_major << "."
            << (int)req->version_minor << "\n";

        cout << "------------------------\n\n";

        cout << "Headers:\n";
        for (int i = 0; i < req->num_headers; i++) {
            const auto& header = req->headers[i];
            cout << "  " << i + 1 << ". ";
            if (header.name_len > 0)
                cout.write(&raw[header.name_offset], header.name_len);
            cout << ": ";
            if (header.value_len > 0)
                cout.write(&raw[header.value_offset], header.value_len);
            cout << "\n";
        }
		int body_len = (int)(raw.size() - req->body_start_offset);
        if (body_len > 0) {
            for(char c : string(&raw[req->body_start_offset], body_len)) {
                if (isprint(c) || c == '\n' || c == '\r') {
                    cout << c;
                }
                else
					break;
			}
        }
    }
};
