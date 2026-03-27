#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include "Networking/ConnectionHandler.h"
#include <unordered_set>
#include <unordered_map>
#include "GeneralEntities/BufferQueue.h"
#include "GeneralEntities/HttpContext.h"
#include "Networking/SocketUtils.h"
using namespace std;

class WebServerEngine {
private:
    socket_t httpListenSock;
    socket_t httpsListenSock;
    unordered_map<socket_t , http_client*> clients;
    unordered_map<socket_t, char*> client_req_messages;
    fd_set readfds;
    SSL_CTX* sslCtx;

    int handleClientRequest(http_client& client) {
        if (client_req_messages.find(client.socket) == client_req_messages.end()) {
            client_req_messages[client.socket] = (char*)malloc(client.bufferSize);
            client.bytesRead = 0;
        }

        char* basePtr = client_req_messages[client.socket];
        char* writePtr = basePtr + client.bytesRead;
        size_t remainingSpace = client.bufferSize - client.bytesRead - 1;

        int bytes = 0;
        if (client.ssl) bytes = SSL_read(client.ssl, writePtr, (int)remainingSpace);
        else bytes = recv(client.socket, writePtr, (int)remainingSpace, 0);

        if (bytes <= 0) return 0;

        client.bytesRead += bytes;
        basePtr[client.bytesRead] = '\0';
		
        const char* headerEnd = strstr(basePtr, "\r\n\r\n");

        if (headerEnd) {
            cout << "Request complete or headers finished." << endl;
            return 1;
        }
        else {
            cout << "Request incomplete, waiting for more packets..." << endl;
            return 2;
        }

		return 1;
    }
    
    bool handleClientResponse(http_client& client, char* res) {

    }
    
    void handleEventLoop() {
        for (auto it = clients.begin(); it != clients.end(); ) {
			cout << "Checking client on port " << it->second->socket << " for activity..." << endl;
            socket_t fd = it->first;
            http_client* ci = it->second;
            bool disconnected = false;
            if (FD_ISSET(fd, &readfds)) {
                if (ci->state == STATE_HANDSHAKING) {
                    int ret = SSL_accept(ci->ssl);
                    if (ret == 1) {
                        ci->state = STATE_CONNECTED;
                        cout << "[SSL] Handshake completed for client." << endl;
                        ++it;
                    }
                    else {
                        int err = SSL_get_error(ci->ssl, ret);
                        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                            cout << "[SSL] Handshake failed, closing." << endl;
                            SSL_free(ci->ssl);
                            CLOSE_SOCKET(fd);
                            it = clients.erase(it);
                        }
                        else {
                            ++it;
                        }
                    }
                    continue;
                }
                
                else {
                    if (handleClientRequest(*ci) <= 0) {
                        disconnected = true;
                    }
                }
            }
            
            if (disconnected) {
                if (client_req_messages.count(fd)) {
                    free(client_req_messages[fd]);
                    client_req_messages.erase(fd);
                }
                if (ci->ssl) SSL_free(ci->ssl);
                CLOSE_SOCKET(fd);
                delete ci;
                it = clients.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    void handleNewConnection(socket_t listenSock, int port, SSL_CTX* sslCtx) {
        if (FD_ISSET(listenSock, &readfds)) {
            socket_t newSock = accept(listenSock, NULL, NULL);
            http_client* ci = new http_client;
            ci->socket = newSock;
            ci->localPort = port;
            ci->bufferSize = 8192;
            ci->bytesRead = 0;
            if (newSock != INVALID_SOCKET) {
                if (listenSock == httpListenSock) {
                    ci->ssl = nullptr;
                    ci->state = STATE_CONNECTED;
                    setNonBlocking(newSock);

                    cout << "[CONN] New client on port " << port
                        << ". Total clients: " << clients.size() << endl;
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
                            SSL_free(ssl);
                            CLOSE_SOCKET(newSock);
                            cout << "[CONN] Handshake failed: Actual error, clean up" << endl;
                            return;
                        }
                    }
                }
                clients.insert({ newSock, ci });
            }
            else
                cout << "[CONN] Invalid Socket" << endl;
        }
    }

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

    void initFDSet() {
        FD_ZERO(&readfds);

        // Add listening sockets to the set 
        FD_SET(httpListenSock, &readfds);
        FD_SET(httpsListenSock, &readfds);

        // Add client sockets to the set 
        for (const auto& client : clients) {
            FD_SET(client.first, &readfds);
        }
    }

    void runServer() {
        cout << "Server running..." << endl;
        cout << "[LOG] Listening for HTTP on port: " << HTTP_DEFAULT_PORT << endl;
        cout << "[LOG] Listening for HTTPS on port: " << HTTPS_DEFAULT_PORT << endl;
        while (true) {
            initFDSet();
            int max_fd = 0;
#ifndef _WIN32
            max_fd = (int)httpListenSock;
            if ((int)httpsListenSock > max_fd) max_fd = (int)httpsListenSock;
            for (const auto& client : clients) {
                if ((int)client.first > max_fd) max_fd = (int)client.first;
            }
#endif
            int activity = select(max_fd+1, &readfds, NULL, NULL, NULL);

            if (activity == SOCKET_ERROR) {
                cerr << "[ERROR] Select error: " << WSAGetLastError() << endl;
                break;
            }

            // 1. Check for new connections and tag them with the correct port
            handleNewConnection(httpListenSock, HTTP_DEFAULT_PORT, sslCtx);
            handleNewConnection(httpsListenSock, HTTPS_DEFAULT_PORT, sslCtx);

            // 2. Handle IO for existing clients
            handleEventLoop();
        }
    }

    socket_t createListenSocket(int port) {
        socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return INVALID_SOCKET;

        setNonBlocking(sock);
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (::bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            CLOSE_SOCKET(sock);
            return INVALID_SOCKET;
        }

        listen(sock, SOMAXCONN);
        return sock;
    }
public:
    WebServerEngine() {
        initSockets();
        httpListenSock = createListenSocket(HTTP_DEFAULT_PORT);
        httpsListenSock = createListenSocket(HTTPS_DEFAULT_PORT);

        if (httpListenSock == INVALID_SOCKET || httpsListenSock == INVALID_SOCKET) {
            cerr << "[FATAL] Failed to initialize listening sockets." << endl;
            return;
        }

        initOpenSsl();
        runServer();
    }

    ~WebServerEngine() {
        CLOSE_SOCKET(httpListenSock);
        CLOSE_SOCKET(httpsListenSock);
        for (const auto& client : clients) {
            CLOSE_SOCKET(client.first);
        }
        cleanUpSockets();

    }
};

int main() {
    try {
        WebServerEngine server;
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    return 0;
}

//cout << "Starting ProxyServer..." << endl;
//WebServerEngine server;
//cout << "Server shutting down." << endl;
//MyData d1 = { 14 , 0.25f , "POST HTTPS1/1"};
//MyData d2 = { 99 , 0.19f , "GET HTTPS1/1" };
//MyData d3 = { 528 , 0.37f , "PUT HTTPS1/1" };
//HostManager* host = new HostManager();
//std::cout << host->send(d1) << std::endl;
//std::cout << host->send(d2) << std::endl;
//std::cout << host->send(d3) << std::endl;

/*
        const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 2\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "OK";

        char* writePtr = client.buffer + client.bytesRead;
        size_t remainingSpace = client.bufferSize - client.bytesRead;

        int bytes = 0;
        if (client.ssl != nullptr) {
            bytes = SSL_read(client.ssl, writePtr, (int)remainingSpace);
        }
        else{
            bytes = recv(client.socket, writePtr, (int)remainingSpace, 0);
        }
        if (bytes <= 0) {
            cout << "[DISC] Client on port " << client.localPort << " disconnected." << endl;
            framework_engine->markSocketDisconnected(client.socket);
            return false;
        }
        client.bytesRead += bytes;
        client.buffer[client.bytesRead] = '\0';
        if (strstr(client.buffer, "\r\n\r\n")) {
            client.state = STATE_PROCESSING;
            framework_engine->forwardRequest(&client);
        }
        return true;


                        if (ci.state == STATE_HANDSHAKING) {
                    int ret = SSL_accept(ci.ssl);
                    if (ret == 1) {
                        ci.state = STATE_CONNECTED;
                        cout << "[SSL] Handshake completed for client." << endl;
                        ++it;
                    }
                    else {
                        int err = SSL_get_error(ci.ssl, ret);
                        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                            cout << "[SSL] Handshake failed, closing." << endl;
                            SSL_free(ci.ssl);
                            CLOSE_SOCKET(fd);
                            it = clients->erase(it);
                        }
                        else {
                            ++it;
                        }
                    }
                    continue;
                }

                bool keepAlive = handleClientRequest(ci);

                if (!keepAlive) {
                    if(ci.buffer != nullptr)
                        free(ci.buffer);
                    if (ci.ssl)
                        SSL_free(ci.ssl);
                    CLOSE_SOCKET(fd);
                    it = clients->erase(it);
                    if (it == clients->end()) break;
                }
            }

            if (it != clients->end())
                ++it;

                        std::lock_guard<std::mutex> lock(map_mutex);
        if (clients->count(sock)) {
            ClientInfo& ci = clients->at(sock);
            if (ci.ssl != nullptr) {
                SSL_write(ci.ssl, response, (int)strlen(response));
            }
            else {
                send(ci.socket, response, (int)strlen(response), 0);
            }

            ci.bytesRead = 0;
            memset(ci.buffer, 0, ci.bufferSize);
            ci.state = STATE_CONNECTED;
        }
*/