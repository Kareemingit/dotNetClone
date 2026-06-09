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
#include "Networking/HttpParser.h"
#include "runtime_host/HostManager.h"
#include "GeneralEntities/Dispatcher.h"
using namespace std;
Dispatcher* HostManager::buffer = nullptr;

class FrameworkEngine {
private:
    vector<thread> workerThreads;
    atomic<bool> running{ true };
	const int THREAD_COUNT = 10;
    Dispatcher& buffer;
	HostManager* hostManager;
    void processRequestQueue() {
        while (running) {
			http_request* req = buffer.requestQueue.dequeue();
			HttpParser::parseRequest(req);
			hostManager->send(req);
        }
    }
public:
    FrameworkEngine(Dispatcher& buff , HostManager* hm) : buffer(buff), hostManager(hm), running(true) {
        workerThreads = vector<thread>(THREAD_COUNT);
        for (auto& each_thread : workerThreads) {
            each_thread = thread(&FrameworkEngine::processRequestQueue, this);
        }
    }

    ~FrameworkEngine() {
        running = false;
        for (auto& each_thread : workerThreads) {
            if (each_thread.joinable())
                each_thread.join();
        }
    }
};

class ClientManager {
private:
    mutex mapMutex;
    atomic<bool> running{ true };
    unordered_map<socket_t, http_client*> clients;
    unordered_map<socket_t, http_request*> client_request_pointers;
    Dispatcher& buffer;
    Transport transport;
public:
    ClientManager(socket_t httpSock, socket_t httpsSock, Dispatcher& buffer) : buffer(buffer) {
        running = true;
        transport = Transport();
        transport.setSockets(httpSock, httpsSock);
    }

    ~ClientManager() {
        running = false;
        for (const auto& client : clients) {
            CLOSE_SOCKET(client.first);
        }
        for (auto& pair : clients) {
            delete pair.second;
        }
        clients.clear();
        for (auto& pair : client_request_pointers) {
            delete pair.second;
        }
        client_request_pointers.clear();
    }
    
    int handleClientRequest(http_client& client) {
        if (client_request_pointers.find(client.socket) == client_request_pointers.end()) {
            http_request* newReq = new http_request();
            newReq->buffer_raw_ptr.resize(client.bufferSize);
            client_request_pointers[client.socket] = newReq;
            client.bytesRead = 0;
        }
        auto& req = client_request_pointers[client.socket];
        char* writePtr = &req->buffer_raw_ptr[client.bytesRead];
        size_t remainingSpace = client.bufferSize - client.bytesRead - 1;

        int bytes = transport.receiveFromClient(&client, writePtr, remainingSpace);

        if (bytes <= 0) {
            if (client_request_pointers.count(client.socket)) {
                delete client_request_pointers[client.socket];
                client_request_pointers.erase(client.socket);
            }
            return 0;
        }
        client.bytesRead += bytes;
        req->buffer_raw_ptr[client.bytesRead] = '\0';

        const char* headerEnd = strstr(&req->buffer_raw_ptr[0], "\r\n\r\n");

        if (headerEnd) {
            size_t headerLen = headerEnd - &req->buffer_raw_ptr[0] + 4;

            size_t contentLength = 0;
            const char* contentLengthPtr = strstr(&req->buffer_raw_ptr[0], "Content-Length:");
            if (!contentLengthPtr) {
                contentLengthPtr = strstr(&req->buffer_raw_ptr[0], "content-length:"); // case-insensitivity
            }
            if (contentLengthPtr) {
                contentLength = strtol(contentLengthPtr + 15, nullptr, 10);
            }
            size_t totalExpectedBytes = headerLen + contentLength;

            if (client.bytesRead < totalExpectedBytes) {
                cout << "Headers finished, but waiting for body bytes: "<< client.bytesRead << "/" << totalExpectedBytes << endl;
                return 2;
            }
            cout << "Request complete or headers finished." << endl;
            http_request* req = client_request_pointers[client.socket];
            req->client_socket = (uintptr_t)client.socket;
            buffer.requestQueue.enqueue(req);
            //cout << "[Framework] Request enqueued from socket " << client.socket << endl;
            return 1;
        }
        else {
            cout << "Request incomplete, waiting for more packets..." << endl;
            return 2;
        }
        return 1;
    }
	
    void handleClientResponse() {
        while (running) {
            http_response* resp = buffer.responseQueue.dequeue();
            //lock_guard<mutex> lock(mapMutex);

            auto it = clients.find(resp->client_socket);
            if (it == clients.end()) {
                if (client_request_pointers.count(resp->client_socket)) {
                    delete client_request_pointers[resp->client_socket];
                    client_request_pointers.erase(resp->client_socket);
                }
				free(resp->buffer_raw_ptr);
                delete resp;
                continue;
            }

            http_client* client = it->second;
            transport.sendToClient(client, (const char*)resp->buffer_raw_ptr);

            // Success: Clean up request
            if (client_request_pointers.count(client->socket)) {
                delete client_request_pointers[client->socket];
                client_request_pointers.erase(client->socket);
            }
            delete resp;
        }
    }

    void initClientsFDSet(fd_set& readfds, fd_set& writefds) {
        for (const auto& client : clients) {
            socket_t fd = client.first;
            http_client* ci = client.second;

            if (ci->state == STATE_HANDSHAKING) {
                FD_SET(fd, &readfds);
                FD_SET(fd, &writefds);
            }
            else {
                FD_SET(fd, &readfds);
            }
        }
    }

    void handleEventLoop(fd_set& readfds, fd_set& writefds) {
        for (auto it = clients.begin(); it != clients.end(); ) {
            socket_t fd = it->first;
            http_client* ci = it->second;
            bool disconnected = false;
            if (FD_ISSET(fd, &readfds) || FD_ISSET(fd, &writefds)) {
                if (ci->state == STATE_HANDSHAKING) {
                    int hsResult = transport.handshakeClient(ci);
                    if (hsResult == 0) {
                        //cout << "[SSL] Handshake failed, closing." << endl;
                        transport.freeClientResources(ci);
                        it = clients.erase(it);
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
                transport.freeClientResources(ci);
                delete ci;
                it = clients.erase(it);
            }
            else {
                ++it;
            }
        }
    }
    
    void addClient(socket_t newSock, int port, socket_t listenSock, http_client* ci) {
        if (transport.acceptClient(ci, newSock, port, listenSock)) {
            clients.insert({ newSock, ci });
        }
        else {
            transport.freeClientResources(ci);
            delete ci;
        }
    }

    int getMaxFD(socket_t httpListenSock, socket_t httpsListenSock) {
#ifndef _WIN32
		int max_fd = max(httpListenSock, httpsListenSock);
        for (const auto& client : clients) {
            if ((int)client.first > max_fd) max_fd = (int)client.first;
        }
		return max_fd;
#endif
		return 0;
    }

    unordered_map<socket_t, http_client*>& getClients() {
        return clients;
    }

    Transport& getTransport() {
        return transport;
	}
};

class ServerManager {
private:
    socket_t httpListenSock;
    socket_t httpsListenSock;
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
    ServerManager() {
        initSockets();
        httpListenSock = createListenSocket(HTTP_DEFAULT_PORT);
        httpsListenSock = createListenSocket(HTTPS_DEFAULT_PORT);
        if (httpListenSock == INVALID_SOCKET || httpsListenSock == INVALID_SOCKET) {
            cerr << "[FATAL] Failed to initialize listening sockets." << endl;
            return;
        }
    }

    void handleNewConnection(socket_t listenSock, int port, ClientManager* clientManager) {
        socket_t newSock = accept(listenSock, NULL, NULL);
        http_client* ci = new http_client;
        clientManager->addClient(newSock, port, listenSock, ci);
    }

    ~ServerManager() {
        CLOSE_SOCKET(httpListenSock);
        CLOSE_SOCKET(httpsListenSock);
    }

    socket_t getHttpListenSock() const { return httpListenSock; }
    socket_t getHttpsListenSock() const { return httpsListenSock; }
};

class WebServerEngine {
private:
    thread workerThread;
	ServerManager* serverManager;
	ClientManager* clientManager;
    atomic<bool> running{ true };
    fd_set readfds;
    fd_set writefds;

    void initFDSet() {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(serverManager->getHttpListenSock(), &readfds);
        FD_SET(serverManager->getHttpsListenSock(), &readfds);
        clientManager->initClientsFDSet(readfds, writefds);
    }

    void runServer() {
        cout << "Server running..." << endl;
        cout << "[LOG] Listening for HTTP on port: " << HTTP_DEFAULT_PORT << endl;
        cout << "[LOG] Listening for HTTPS on port: " << HTTPS_DEFAULT_PORT << endl;
        workerThread = thread(&ClientManager::handleClientResponse, clientManager);
        while (running) {
            initFDSet();
            int max_fd = clientManager->getMaxFD(serverManager->getHttpListenSock(), serverManager->getHttpsListenSock());
            int activity = select(max_fd + 1, &readfds, &writefds, NULL, NULL);

            if (activity == SOCKET_ERROR) {
                cerr << "[ERROR] Select error: " << WSAGetLastError() << endl;
                break;
            }

            // 1. Check for new connections and tag them with the correct port
            if (FD_ISSET(serverManager->getHttpListenSock(), &readfds)) {
                serverManager->handleNewConnection(serverManager->getHttpListenSock(), HTTP_DEFAULT_PORT, clientManager);
            }
            if (FD_ISSET(serverManager->getHttpsListenSock(), &readfds)) {
                serverManager->handleNewConnection(serverManager->getHttpsListenSock(), HTTPS_DEFAULT_PORT, clientManager);
            }
            // 2. Handle IO for existing clients
            clientManager->handleEventLoop(readfds, writefds);
        }
    }

public:
    WebServerEngine(Dispatcher& buff){
		running = true;
		serverManager = new ServerManager();
        clientManager = new ClientManager(serverManager->getHttpListenSock(), serverManager->getHttpsListenSock(), buff);
        runServer();
    }

    ~WebServerEngine() {
		running = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
        delete serverManager;
		delete clientManager;
        cleanUpSockets();
    }
};


int main() {
    try {
        Dispatcher ReqResbuffer;
		HostManager hostManager(ReqResbuffer);
        FrameworkEngine framework(ReqResbuffer , &hostManager);
        WebServerEngine server(ReqResbuffer);
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    return 0;
}

/*

        };

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
*/