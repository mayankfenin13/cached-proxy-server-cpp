#include <iostream>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "proxy_parse.h"
#include "proxy_parse.cpp"
#include "LRUCache.h"

#define MAX_CLIENTS 10
#define MAX_BYTES 4096

using namespace std;

sem_t sem;
LRUCache* cache;

void sendErrorMessage(int clientSocket, int statusCode) {
    const char* statusText;
    const char* body;
    switch (statusCode) {
        case 400:
            statusText = "400 Bad Request";
            body = "Bad Request\n";
            break;
        case 404:
            statusText = "404 Not Found";
            body = "Not Found\n";
            break;
        case 502:
            statusText = "502 Bad Gateway";
            body = "Bad Gateway\n";
            break;
        case 500:
        default:
            statusText = "500 Internal Server Error";
            body = "Internal Server Error\n";
            break;
    }
    char header[256];
    snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "Content-Length: %zu\r\n"
        "\r\n",
        statusText, strlen(body));
    send(clientSocket, header, strlen(header), 0);
    send(clientSocket, body, strlen(body), 0);
}

char* recv_data(int clientSocket) {
    char* data = (char*)malloc(sizeof(char));
    *data = '\0';
    char* buffer = (char*)calloc(MAX_BYTES, sizeof(char));
    int bytes_count, total_bytes_count = 0;
    do {
        bytes_count = recv(clientSocket, buffer, MAX_BYTES-1, 0);
        total_bytes_count += bytes_count;
        data = (char*)realloc(data, strlen(data)+bytes_count+1);
        strcat(data, buffer);
        buffer[bytes_count] = '\0';
        data[total_bytes_count] = '\0';
        if (strstr(buffer, "\r\n\r\n")) break;
        memset(buffer, 0, MAX_BYTES);
    } while (bytes_count > 0);

    if (bytes_count == -1) {
        cerr << "Error receiving data from client" << endl;
        sendErrorMessage(clientSocket, 500);
        free(data);
        free(buffer);
        sem_post(&sem);
        return nullptr;
    } else if (total_bytes_count == 0) {
        cerr << "No data received from client" << endl;
        free(data);
        free(buffer);
        sem_post(&sem);
        return nullptr;
    }

    data[total_bytes_count] = '\0';

    return data;
}

int connectToServer(char* host, int port) {
    addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, to_string(port).c_str(), &hints, &res) != 0) {
        cout << "Error parsing hostname" << endl;
        return -1;
    }

    int serverSocket = -1;
    for (; res; res = res->ai_next) {
        serverSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (serverSocket == -1) continue;

        if (connect(serverSocket, res->ai_addr, res->ai_addrlen) != -1) {
            break;
        } else {
            serverSocket = -1;
        }
    }

    freeaddrinfo(res);

    return serverSocket;
}

int send_data(int socketId, char* buffer, ssize_t len) {
    ssize_t bytes_sent = 0;
    ssize_t total_bytes_sent = 0;
    do {
        bytes_sent = send(socketId, buffer+total_bytes_sent, (len-total_bytes_sent)*sizeof(char), 0);
        if (bytes_sent == -1) {
            return -1;
        }
        total_bytes_sent += bytes_sent;
    } while (total_bytes_sent < len && bytes_sent > 0);
    return 0;
}

void* thread_fn(void* clientSocketPtr) {
    sem_wait(&sem);
    int p;
    sem_getvalue(&sem, &p);
    cout << "Semaphore pre value: " << p << endl;
    int clientSocket = *((int*)clientSocketPtr);
    free((int*)clientSocketPtr);

    char* req_buf = recv_data(clientSocket);
    if (!req_buf) return nullptr;

    ParsedRequest* request = ParsedRequest_create();
    if (ParsedRequest_parse(request, req_buf, strlen(req_buf)) == -1) {
        cout << "Error parsing request" << endl;
        sendErrorMessage(clientSocket, 400);
        free(req_buf);
        return nullptr;
    }
    free(req_buf);
    
    if (strlen(request->port) == 0) strcpy(request->port, "80");
    
    
    if (ParsedHeader_set(request, "Connection", "close") == -1) {
        cout << "Set header key is not working" << endl;
    }
    
    string request_host;
    if (request->port) {
        request_host = string(request->host) + ":" + string(request->port);
    } else {
        request_host = string(request->host);
    }
    if (ParsedHeader_set(request, "Host", request_host.c_str()) == -1) {
        cout << "Set Host Header key is not working" << endl;
    }
    
    mutex m;
    unique_lock<mutex> lock(m);
    string cache_data = cache->get(request_host);
    lock.unlock();
    if (cache_data.length()) {
        cout << "Found the response" << endl;
        if (send_data(clientSocket, (char*)cache_data.c_str(), cache_data.length()) == -1) {
            cout << "Error sending response to client" << endl;
            sendErrorMessage(clientSocket, 500);
            sem_post(&sem);
            
            shutdown(clientSocket, SHUT_RDWR);
            close(clientSocket);
            sem_getvalue(&sem, &p);
            cout << "Semaphore post value: " << p << endl;
            return nullptr;
        }
    } else {
        cout << "Didn't find the response" << endl;
        int serverSocket = connectToServer(request->host, stoi(request->port));
        if (serverSocket == -1) {
            cout << "Error connecting server" << endl;
            sendErrorMessage(clientSocket, 502);
            sem_post(&sem);
            ParsedRequest_destroy(request);
            return nullptr;
        }
        char* buf = (char*)calloc(MAX_REQ_LEN, sizeof(char));
        if (ParsedRequest_unparse(request, buf, MAX_REQ_LEN) == -1) {
            cout << "Error unparsing request" << endl;
            sendErrorMessage(clientSocket, 500);
            sem_post(&sem);
            free(buf);
            return nullptr;
        }
    
        int len = strstr(buf, "\r\n\r\n") - buf + 4;
        buf[len] = '\0';
    
        if (send_data(serverSocket, buf, len) == -1) {
            cout << "Error sending data to server" << endl;
            sendErrorMessage(clientSocket, 502);
            ParsedRequest_destroy(request);
            sem_post(&sem);
            free(buf);
            return nullptr;
        }
        ParsedRequest_destroy(request);
    
        free(buf);
        char* buffer = (char*)calloc(MAX_BYTES, sizeof(char));
        string data;
    
        int bytes_count;
        do {
            bytes_count = recv(serverSocket, buffer, MAX_BYTES, 0);
            if (bytes_count == -1) {
                cout << "Error receiving data from server" << endl;
                sendErrorMessage(clientSocket, 502);
                sem_post(&sem);
                free(buffer);
                return nullptr;
            }
            buffer[bytes_count] = '\0';
            data += buffer;
            if (send_data(clientSocket, buffer, bytes_count) == -1) {
                cout << "Error sending response to client" << endl;
                sendErrorMessage(clientSocket, 500);
                sem_post(&sem);
                free(buffer);
                return nullptr;
            }
            memset(buffer, 0, MAX_BYTES);
        } while (bytes_count);
    
        mutex m;
        unique_lock<mutex> lock(m);
        cache->put(request_host, data);
        lock.unlock();
        free(buffer);
        shutdown(serverSocket, SHUT_RDWR);
        close(serverSocket);
    }

    shutdown(clientSocket, SHUT_RDWR);
    close(clientSocket);
    sem_post(&sem);
    sem_getvalue(&sem, &p);
    cout << "Semaphore post value: " << p << endl;
    return nullptr;
}

int main() {
    int PORT = 8080;
    cache = new LRUCache(100);

    sem_init(&sem, 0, MAX_CLIENTS);
    sockaddr_in server_addr, client_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    int socketId = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(socketId, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(socketId, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        cerr << "Couldn't bind the server to port " << PORT << endl;
        exit(1);
    }

    if (listen(socketId, MAX_CLIENTS) == -1) {
        cerr << "Listening failed..." << endl;
        exit(1);
    }

    while (true) {
        socklen_t p = sizeof(client_addr);
        int clientSocket = accept(socketId, (sockaddr*)&client_addr, (socklen_t*)&p);
        if (clientSocket == -1) {
            cerr << "Couldn't accept the client connection" << endl;
            continue;
        }

        cout << "Client connected!" << endl;
        cout << "IP Address: " << inet_ntoa(client_addr.sin_addr) << endl;
        cout << "Port: " << ntohs(client_addr.sin_port) << endl;

        pthread_t thread;
        int* clientSocketPtr = (int*)malloc(sizeof(int));
        *clientSocketPtr = clientSocket;
        pthread_create(&thread, nullptr, thread_fn, clientSocketPtr);
    }

    return 0;
}