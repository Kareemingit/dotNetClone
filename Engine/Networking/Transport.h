#pragma once
#include "..\GeneralEntities\HttpContext.h"
using namespace std;

class Transport {
private:
    socket_t httpListenSock;
    socket_t httpsListenSock;
    SSL_CTX* sslCtx;

    void initOpenSsl() {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        sslCtx = SSL_CTX_new(TLS_server_method());

        if (SSL_CTX_use_certificate_file(sslCtx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
            SSL_CTX_use_PrivateKey_file(sslCtx, "server.key", SSL_FILETYPE_PEM) <= 0) {
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }
    }

public:
    Transport() {
        initOpenSsl();
    }
    ~Transport() {
        // Clean up sockets
    }

    void setSockets(socket_t http, socket_t https) {
        httpListenSock = http;
        httpsListenSock = https;
    }

    void freeClientResources(http_client* ci) {
        if (ci->ssl) {
            SSL_free(ci->ssl);
        }
        CLOSE_SOCKET(ci->socket);
    }

    int handshakeClient(http_client* ci) {
        if (!ci->ssl) return 1;
        int ret = SSL_accept(ci->ssl);
        if (ret == 1) {
            ci->state = STATE_CONNECTED;
            return 1;
        }
        int err = SSL_get_error(ci->ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 2;
        }
        return 0;
    }

    bool acceptClient(http_client* ci, socket_t newSock, int port, socket_t listenSock) {
        ci->socket = newSock;
        ci->localPort = port;
        ci->bufferSize = 8192;
        ci->bytesRead = 0;
        if (newSock != INVALID_SOCKET) {
            if (listenSock == httpListenSock) {
                ci->ssl = nullptr;
                ci->state = STATE_CONNECTED;
                setNonBlocking(newSock);

                cout << "[CONN] New client on port " << port << endl;
            }
            if (listenSock == httpsListenSock) {
                SSL* ssl = SSL_new(sslCtx);
                SSL_set_fd(ssl, (int)newSock);
                setNonBlocking(newSock);
                ci->ssl = ssl;
                ci->state = STATE_HANDSHAKING;
                int ret = SSL_accept(ssl);
                if (ret == 1) {
                    ci->state = STATE_CONNECTED;
                }
                else {
                    int err = SSL_get_error(ssl, ret);
                    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                        cout << "[CONN] Handshake failed: Actual error, clean up" << endl;
                        return false;
                    }
                }
            }
        }
        else {
            cout << "[CONN] Invalid Socket" << endl;
            return false;
        }
        return true;
    }

    int sendToClient(http_client* client, const char* response) {
        if (client->ssl) {
            return SSL_write(client->ssl, response, (int)strlen(response));
        }
        else {
            return send(client->socket, response, (int)strlen(response), 0);
        }
    }

    int receiveFromClient(http_client* client, char* buffer, size_t bufferSize) {
        if (client->ssl) {
            return SSL_read(client->ssl, buffer, (int)bufferSize);
        }
        else {
            return recv(client->socket, buffer, (int)bufferSize, 0);
        }
    }
};
