#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include "GeneralEntities/BufferQueue.h"
#include "GeneralEntities/HttpContext.h"
#include "Networking/SocketUtils.h"
#include "Networking/Transport.h"
using namespace std;

struct Dispatcher {
    BufferQueue<http_request*> requestQueue;
    BufferQueue<http_response*> responseQueue;
};


class FrameworkEngine {
private:
    thread workerThread;
    bool running;
    Dispatcher& buffer;

    void processQueue() {
        while (running) {
			http_request* req = buffer.requestQueue.dequeue();
            for(char c : req->buffer_raw_ptr) {
                cout << c;
			}
            const char* response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 2\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "\r\n"
                "OK";
			http_response* resp = new http_response;
            resp->client_socket = req->client_socket;
            resp->buffer_raw_ptr = (void*)response;
			buffer.responseQueue.enqueue(resp);
			cout << "\n[Framework] Processing request from socket " << req->client_socket << endl;
        }
    }
public:
    FrameworkEngine(Dispatcher& buff) : buffer(buff), running(true) {
        workerThread = std::thread(&FrameworkEngine::processQueue, this);
    }

    ~FrameworkEngine() {
        running = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }
    
};

class WebServerEngine {
private:
    Dispatcher& buffer;
    thread workerThread;
    bool running;
    socket_t httpListenSock;
    socket_t httpsListenSock;
    unordered_map<socket_t , http_client*> clients;
    unordered_map<socket_t, vector<char>> client_req_messages;
    fd_set readfds;
	fd_set writefds;
	Transport transport;

    int handleClientRequest(http_client& client) {
        if (client_req_messages.find(client.socket) == client_req_messages.end()) {
            client_req_messages[client.socket] = vector<char>(client.bufferSize);
            client.bytesRead = 0;
        }

        char* basePtr = &client_req_messages[client.socket][0];
        char* writePtr = basePtr + client.bytesRead;
        size_t remainingSpace = client.bufferSize - client.bytesRead - 1;

		int bytes = transport.receiveFromClient(&client, writePtr, remainingSpace);

        if (bytes <= 0) return 0;

        client.bytesRead += bytes;
        basePtr[client.bytesRead] = '\0';
		
        const char* headerEnd = strstr(basePtr, "\r\n\r\n");

        if (headerEnd) {
            cout << "Request complete or headers finished." << endl;
            vector<char> fullRequest = std::move(client_req_messages[client.socket]);
            http_request* req = new http_request;
            req->client_socket = (uintptr_t)client.socket;
            req->buffer_raw_ptr = std::move(fullRequest);
            buffer.requestQueue.enqueue(req);
            cout << "[Framework] Request enqueued from socket " << client.socket << endl;
            client_req_messages.erase(client.socket);
            return 1;
        }
        else {
            cout << "Request incomplete, waiting for more packets..." << endl;
            return 2;
        }

		return 1;
    }
    
    bool handleClientResponse() {
        while (running)
        {
            http_response* resp = buffer.responseQueue.dequeue();
            http_client* client = clients[resp->client_socket];
            if(clients.find(resp->client_socket) == clients.end()) {
                cout << "[WARN] Client socket " << resp->client_socket << " not found for response." << endl;
                continue;
			}
			transport.sendToClient(client, (const char*)resp->buffer_raw_ptr);
            if (client_req_messages.count(client->socket)) {
                client_req_messages.erase(client->socket);
            }
        }
    }
    
    void handleEventLoop() {
        for (auto it = clients.begin(); it != clients.end(); ) {
			cout << "Checking client on port " << it->second->socket << " for activity..." << endl;
            socket_t fd = it->first;
            http_client* ci = it->second;
            bool disconnected = false;
            if (FD_ISSET(fd, &readfds) || FD_ISSET(fd, &writefds)) {
                if (ci->state == STATE_HANDSHAKING) {
                    if (!transport.handshakeClient(ci)) {
                        cout << "[SSL] Handshake failed, closing." << endl;
                        transport.freeClientResources(ci);
                        it = clients.erase(it);
					}
                    else {
                        ++it;
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
                cout << "Client : " << ci->socket << " Disconnected" << endl;
                if (client_req_messages.count(fd)) {
                    client_req_messages.erase(fd);
                }
				transport.freeClientResources(ci);
                delete ci;
                it = clients.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    void handleNewConnection(socket_t listenSock, int port) {
        if (FD_ISSET(listenSock, &readfds)) {
            socket_t newSock = accept(listenSock, NULL, NULL);
            http_client* ci = new http_client;
            if (transport.acceptClient(ci, newSock, port, listenSock)) {
                clients.insert({ newSock, ci });
            }
            else {
				transport.freeClientResources(ci);
                delete ci;
            }
        }
    }

    void initFDSet() {
        FD_ZERO(&readfds);
		FD_ZERO(&writefds);

        // Add listening sockets to the set 
        FD_SET(httpListenSock, &readfds);
        FD_SET(httpsListenSock, &readfds);

        // Add client sockets to the set 
        for (const auto& client : clients) {
            socket_t fd = client.first;
            http_client* ci = client.second;

            if (ci->state == STATE_HANDSHAKING) {
                // During handshake, we might be waiting for either
                FD_SET(fd, &readfds);
                FD_SET(fd, &writefds);
            }
            else {
                // Normal connected state: wait for request data
                FD_SET(fd, &readfds);
            }
        }
    }

    void runServer() {
        cout << "Server running..." << endl;
        cout << "[LOG] Listening for HTTP on port: " << HTTP_DEFAULT_PORT << endl;
        cout << "[LOG] Listening for HTTPS on port: " << HTTPS_DEFAULT_PORT << endl;
        workerThread = std::thread(&WebServerEngine::handleClientResponse, this);
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
            int activity = select(max_fd+1, &readfds, &writefds, NULL, NULL);

            if (activity == SOCKET_ERROR) {
                cerr << "[ERROR] Select error: " << WSAGetLastError() << endl;
                break;
            }

            // 1. Check for new connections and tag them with the correct port
            handleNewConnection(httpListenSock, HTTP_DEFAULT_PORT);
            handleNewConnection(httpsListenSock, HTTPS_DEFAULT_PORT);

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
    WebServerEngine(Dispatcher& buff) : buffer(buff){
        initSockets();
        httpListenSock = createListenSocket(HTTP_DEFAULT_PORT);
        httpsListenSock = createListenSocket(HTTPS_DEFAULT_PORT);

        if (httpListenSock == INVALID_SOCKET || httpsListenSock == INVALID_SOCKET) {
            cerr << "[FATAL] Failed to initialize listening sockets." << endl;
            return;
        }
		running = true;
		transport = Transport();
		transport.setSockets(httpListenSock, httpsListenSock);
        runServer();
    }

    ~WebServerEngine() {
        running = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
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
        Dispatcher ReqResbuffer;
        FrameworkEngine framework(ReqResbuffer);
        WebServerEngine server(ReqResbuffer);
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    return 0;
}

/*
if (clients.find(clientSock) == clients.end())
            return false;
        http_client* client = clients[clientSock];
        if (client->ssl) {
            SSL_write(client->ssl, response, (int)strlen(response));
        }
        else {
            send(client->socket, response, (int)strlen(response), 0);
        }
        if (client_req_messages.count(client->socket)) {
            client_req_messages.erase(client->socket);
        }
        ---------------------------------
                int bytes = 0;
        if (client.ssl) bytes = SSL_read(client.ssl, writePtr, (int)remainingSpace);
        else bytes = recv(client.socket, writePtr, (int)remainingSpace, 0);
*/

//int ret = SSL_accept(ci->ssl);
//if (ret == 1) {
//    ci->state = STATE_CONNECTED;
//    cout << "[SSL] Handshake completed for client." << endl;
//    ++it;
//}
//else {
//    int err = SSL_get_error(ci->ssl, ret);
//    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
//        cout << "[SSL] Handshake failed, closing." << endl;
//        SSL_free(ci->ssl);
//        CLOSE_SOCKET(fd);
//        it = clients.erase(it);
//    }
//    else {
//        ++it;
//    }
//}

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