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

struct RequestTask {
    socket_t client_socket;
    char* buffer;
    size_t buffer_size;
    bool is_ssl;
    SSL* ssl_ptr;
};

class FrameworkEngine {
private:
    BufferQueue<RequestTask> request_tasks;
    unordered_set<socket_t> abandoned_sockets;
    unordered_map<socket_t, http_request*> current_processing_requests;
    std::thread worker_thread;
    std::atomic<bool> running;
    mutex engine_mutex;

    void processingLoop() {
        while (true) {
            RequestTask task = request_tasks.dequeue();
            if (task.buffer == nullptr) {
                cout << "[Error] Dequeued a task with a null buffer!" << endl;
                continue;
            }
            // 1. & 2. PARSE & CALL C# 
            // (Assumed: http_request* req = parser.parse(task.buffer);)
            cout << task.buffer << endl;

            // 3. CHECK DISCONNECTION BEFORE SENDING
            bool isAbandoned = false;
            {
                std::lock_guard<std::mutex> lock(engine_mutex);
                if (abandoned_sockets.count(task.client_socket)) {
                    isAbandoned = true;
                    abandoned_sockets.erase(task.client_socket);
                }
            }

            if (!isAbandoned) {
                const char* response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 2\r\n"
                    "Content-Type: text/plain\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "OK";

                if (task.is_ssl) {
                    SSL_write(task.ssl_ptr, response, (int)strlen(response));
                }
                else {
                    send(task.client_socket, response, (int)strlen(response), 0);
                }
            }

            // 4. CLEAN UP EVERYTHING
            // Since you put http_request in heap, delete it here
            // delete task.meta_ptr; 
            free(task.buffer);

            if (task.is_ssl && task.ssl_ptr) {
                SSL_shutdown(task.ssl_ptr);
                SSL_free(task.ssl_ptr);
            }
            CLOSE_SOCKET(task.client_socket);
        }
    }

    void start() {
        if (running) return;
        running = true;
        worker_thread = std::thread(&FrameworkEngine::processingLoop, this);
    }
    void stop() {
        if (!running) return;
        running = false;
        RequestTask stop_signal = { 0 };
        request_tasks.enqueue(stop_signal);

        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }
public:
    FrameworkEngine() : running(false) {
        start();
    }

    ~FrameworkEngine() {
        stop();
    }

    void forwardRequest(RequestTask message) {
        request_tasks.enqueue(message);
    }
    void markSocketDisconnected(socket_t sock) {
        lock_guard<std::mutex> lock(engine_mutex);
        abandoned_sockets.insert(sock);
    }
};

class ConnectionHandler {
private:
    socket_t http;
    socket_t https;
    unordered_map<socket_t , ClientInfo>* clients;
    fd_set* readfds;
    FrameworkEngine* framework_engine;
    bool handleClientRequest(ClientInfo& client) {
        //char buffer[4096];
        char* writePtr = client.buffer + client.bytesRead;
        size_t remainingSpace = client.bufferSize - client.bytesRead;
        const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 2\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "OK";
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
            RequestTask task;
            task.client_socket = client.socket;
            task.buffer = client.buffer;
            task.buffer_size = client.bytesRead;
            task.is_ssl = (client.ssl != nullptr);
            task.ssl_ptr = client.ssl;
            client.buffer = nullptr;
            framework_engine->forwardRequest(task);
            return false;
        }
        return true;
    }

public:
    void handleNewConnection(socket_t listenSock, int port, SSL_CTX* sslCtx) {
        if (FD_ISSET(listenSock, readfds)) {
            socket_t newSock = accept(listenSock, NULL, NULL);
            ClientInfo ci;
            ci.socket = newSock;
            ci.localPort = port;
            ci.bufferSize = 8192;
            ci.buffer = (char*)malloc(ci.bufferSize);
            ci.bytesRead = 0;
            if (newSock != INVALID_SOCKET) {
                if (listenSock == http) {
                    ci.ssl = nullptr;
                    ci.state = STATE_CONNECTED;
                    setNonBlocking(newSock);

                    cout << "[CONN] New client on port " << port
                        << ". Total clients: " << clients->size() << endl;
                }

                if (listenSock == https) {
                    SSL* ssl = SSL_new(sslCtx);
                    SSL_set_fd(ssl, (int)newSock);
                    setNonBlocking(newSock);
                    ci.ssl = ssl;
                    ci.state = STATE_HANDSHAKING;
                    int ret = SSL_accept(ssl);
                    if (ret == 1) {
                        ci.state = STATE_CONNECTED;
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
                
                clients->insert({ newSock, ci });
            }
            else
                cout << "[CONN] Invalid Socket" << endl;
        }
    }

    void handleEventLoop() {
        for (auto it = clients->begin(); it != clients->end(); ) {
            socket_t fd = it->first;
            ClientInfo& ci = it->second;

            if (FD_ISSET(fd, readfds)) {
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
                    it = clients->erase(it);
                    continue;
                }
            }
            
            if (it != clients->end())
                ++it;
        }
    }

    ConnectionHandler(unordered_map<socket_t,ClientInfo>* cInfos, fd_set* globalSet, socket_t forHttp, socket_t forHttps) {
        clients = cInfos;
        readfds = globalSet;
        http = forHttp;
        https = forHttps;
        framework_engine = new FrameworkEngine();
    }
};


class WebServerEngine {
private:
    socket_t httpListenSock;
    socket_t httpsListenSock;
    unordered_map<socket_t , ClientInfo> clients;
    fd_set readfds;
    SSL_CTX* sslCtx;
    ConnectionHandler* connectionHandler = NULL;

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
            connectionHandler->handleNewConnection(httpListenSock, HTTP_DEFAULT_PORT, sslCtx);
            connectionHandler->handleNewConnection(httpsListenSock, HTTPS_DEFAULT_PORT, sslCtx);

            // 2. Handle IO for existing clients
            connectionHandler->handleEventLoop();
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

        if (bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
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
        connectionHandler = new ConnectionHandler(&clients, &readfds, httpListenSock, httpsListenSock);
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
        delete connectionHandler;
    }
};

int main() {
    try {
        WebServerEngine server;
        int x;
        cin >> x;
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
