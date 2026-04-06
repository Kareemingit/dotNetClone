#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include "GeneralEntities/BufferQueue.h"
#include "GeneralEntities/HttpContext.h"
#include "Networking/SocketUtils.h"
#include "Networking/Transport.h"
using namespace std;

struct Dispatcher {
    BufferQueue<http_request*> requestQueue;
    BufferQueue<http_response*> responseQueue;
};

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

    static int parseFirstLine(http_request* req) {
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

    static int parseHeaders(http_request* req) {
        return 0;
	}
public:
    static void parseRequest(http_request* req) {
		int firstLineState = parseFirstLine(req);
        if(firstLineState == PARSE_ERROR) {
            cerr << "[ERROR] Failed to parse request." << endl;
            return;
		}
		int headerState = parseHeaders(req);
        if (headerState == PARSE_ERROR) {
            cerr << "[ERROR] Failed to parse headers." << endl;
            return;
        }
    }
};

class FrameworkEngine {
private:
    thread workerThread;
    bool running;
    Dispatcher& buffer;

    void processQueue() {
        while (running) {
			http_request* req = buffer.requestQueue.dequeue();
			/*cout << endl << "--------------------------------------------------------------" << endl;
            for(char c : req->buffer_raw_ptr) {
                cout << c;
			}
            cout << endl << "--------------------------------------------------------------" << endl;*/

            //HttpParser::parseRequest(req);
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
		auto& vec = client_req_messages[client.socket];
        char* writePtr = &vec[client.bytesRead];
        size_t remainingSpace = client.bufferSize - client.bytesRead - 1;

		int bytes = transport.receiveFromClient(&client, writePtr, remainingSpace);

        if (bytes <= 0) return 0;

        client.bytesRead += bytes;
        vec[client.bytesRead] = '\0';
		
        const char* headerEnd = strstr(&vec[0], "\r\n\r\n");

        if (headerEnd) {
            cout << "Request complete or headers finished." << endl;
            vec.resize(client.bytesRead);
            http_request* req = new http_request;
            req->client_socket = (uintptr_t)client.socket;
            req->buffer_raw_ptr = std::move(vec);
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
    
    void handleClientResponse() {
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
					int hsResult = transport.handshakeClient(ci);
                    if (hsResult == 0) {
                        cout << "[SSL] Handshake failed, closing." << endl;
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

        FD_SET(httpListenSock, &readfds);
        FD_SET(httpsListenSock, &readfds);

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


void printRequest(http_request* req) {
    const vector<char>& raw = req->buffer_raw_ptr;

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
}

int main() {
    try {
        /*Dispatcher ReqResbuffer;
        FrameworkEngine framework(ReqResbuffer);
        WebServerEngine server(ReqResbuffer);*/

        cout << "Test 1: Full Browser Request\n";

        vector<char> vec1 = {
            'D','E','L' ,'E' , 'T' , 'E' ,' ','/','a','/','v','e','r','y','/','l','o','n','g','/','p','a','t','h',' ',
            'H','T','T','P','/','1','.','1','\r','\n',
            '\r','\n'
        };

        http_request* req = new http_request{};
        req->buffer_raw_ptr = std::move(vec1);
		cout << "parser 1" << endl;
        auto start = std::chrono::high_resolution_clock::now();
        HttpParser::parseRequest(req);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Elapsed time: " << elapsed.count() << " seconds\n";
        printRequest(req);

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
*/

/*

        vector<char> vec = {
    'G','E','T',' ','/',' ','H','T','T','P','/','1','.','1','\r','\n',

    'H','o','s','t',':',' ','l','o','c','a','l','h','o','s','t','\r','\n',
    'C','o','n','n','e','c','t','i','o','n',':',' ','k','e','e','p','-','a','l','i','v','e','\r','\n',

    's','e','c','-','c','h','-','u','a',':',' ','"','N','o','t',':','A','-','B','r','a','n','d','"',';','v','=','"','9','9','"',',',' ',
    '"','G','o','o','g','l','e',' ','C','h','r','o','m','e','"',';','v','=','"','1','4','5','"',',',' ',
    '"','C','h','r','o','m','i','u','m','"',';','v','=','"','1','4','5','"','\r','\n',

    's','e','c','-','c','h','-','u','a','-','m','o','b','i','l','e',':',' ','?','0','\r','\n',
    's','e','c','-','c','h','-','u','a','-','p','l','a','t','f','o','r','m',':',' ','"','W','i','n','d','o','w','s','"','\r','\n',

    'U','p','g','r','a','d','e','-','I','n','s','e','c','u','r','e','-','R','e','q','u','e','s','t','s',':',' ','1','\r','\n',

    'U','s','e','r','-','A','g','e','n','t',':',' ',
    'M','o','z','i','l','l','a','/','5','.','0',' ',
    '(','W','i','n','d','o','w','s',' ','N','T',' ','1','0','.','0',';',' ','W','i','n','6','4',';',' ','x','6','4',')',' ',
    'A','p','p','l','e','W','e','b','K','i','t','/','5','3','7','.','3','6',' ',
    '(','K','H','T','M','L',',',' ','l','i','k','e',' ','G','e','c','k','o',')',' ',
    'C','h','r','o','m','e','/','1','4','5','.','0','.','0','.','0',' ',
    'S','a','f','a','r','i','/','5','3','7','.','3','6','\r','\n',

    'S','e','c','-','P','u','r','p','o','s','e',':',' ','p','r','e','f','e','t','c','h',';','p','r','e','r','e','n','d','e','r','\r','\n',

    'A','c','c','e','p','t',':',' ',
    't','e','x','t','/','h','t','m','l',',',
    'a','p','p','l','i','c','a','t','i','o','n','/','x','h','t','m','l','+','x','m','l',',',
    'a','p','p','l','i','c','a','t','i','o','n','/','x','m','l',';','q','=','0','.','9',',',
    'i','m','a','g','e','/','a','v','i','f',',',
    'i','m','a','g','e','/','w','e','b','p',',',
    'i','m','a','g','e','/','a','p','n','g',',',
    '*','/','*',';','q','=','0','.','8',',',
    'a','p','p','l','i','c','a','t','i','o','n','/','s','i','g','n','e','d','-','e','x','c','h','a','n','g','e',';','v','=','b','3',';','q','=','0','.','7','\r','\n',

    'S','e','c','-','F','e','t','c','h','-','S','i','t','e',':',' ','n','o','n','e','\r','\n',
    'S','e','c','-','F','e','t','c','h','-','M','o','d','e',':',' ','n','a','v','i','g','a','t','e','\r','\n',
    'S','e','c','-','F','e','t','c','h','-','U','s','e','r',':',' ','?','1','\r','\n',
    'S','e','c','-','F','e','t','c','h','-','D','e','s','t',':',' ','d','o','c','u','m','e','n','t','\r','\n',

    'A','c','c','e','p','t','-','E','n','c','o','d','i','n','g',':',' ',
    'g','z','i','p',',',' ','d','e','f','l','a','t','e',',',' ','b','r',',',' ','z','s','t','d','\r','\n',

    'A','c','c','e','p','t','-','L','a','n','g','u','a','g','e',':',' ',
    'e','n','-','U','S',',',
    'e','n',';','q','=','0','.','9',',',
    'p','t',';','q','=','0','.','8',',',
    't','r',';','q','=','0','.','7',',',
    'a','r',';','q','=','0','.','6','\r','\n',

    '\r','\n'
        };
*/