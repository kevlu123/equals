#include <WinSock2.h>

#include <stdint.h>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <stdexcept>

#pragma comment(lib, "Ws2_32.lib")

constexpr uint16_t PORT = 37495;

struct NetworkException : std::runtime_error {
    NetworkException(const char* msg) : std::runtime_error(msg) {}
};

inline bool InitNetwork() {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

struct TcpServer {
    using MessageCallback = std::function<void(std::vector<uint8_t>)>;

    TcpServer() {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            throw NetworkException("Couldn't create socket");
        }

        sockaddr_in sockAddr{};
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sockAddr.sin_port = htons(PORT);

        if (bind(sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR) {
            auto x =  WSAGetLastError();
            closesocket(sock);
            throw NetworkException("Couldn't bind socket");
        }

        if (listen(sock, 16) == SOCKET_ERROR) {
            closesocket(sock);
            throw NetworkException("Couldn't listen on socket");
        }

        //unsigned long mode = 1;
        //ioctlsocket(sock->channel, FIONBIO, &mode);
    }

    ~TcpServer() {
        quit = true;
        closesocket(sock);
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& thread : threads) {
            thread.join();
        }
    }

    void Run(MessageCallback callback) {
        this->callback = callback;
        threads.push_back(std::thread(&TcpServer::Accept, this));
    }

private:

    void Accept() {
        while (!quit) {
            sockaddr_in clientAddr{};
            int sockLen = sizeof(clientAddr);
            SOCKET clientSock = accept(sock, (struct sockaddr*)&clientAddr, &sockLen);
            if (clientSock == INVALID_SOCKET) {
                return;
            }

            std::unique_lock<std::mutex> lock(mtx, std::try_to_lock);
            if (lock.owns_lock()) {
                threads.push_back(std::thread(&TcpServer::Receive, this, clientSock));
            } else {
                // Quitting
                closesocket(clientSock);
            }
        }
    }

    void Receive(SOCKET clientSock) {
        std::vector<uint8_t> buffer(1024);
        size_t offset = 0;
        while (!quit) {
            int bytesRead = recv(
                clientSock,
                (char*)buffer.data() + offset,
                (int)(buffer.size() - offset),
                0);
            if (bytesRead <= 0) {
                closesocket(clientSock);
                return;
            }

            offset += bytesRead;
            if (offset < 4) {
                continue;
            }

            uint32_t messageSize = *(uint32_t*)buffer.data();
            if (offset < (size_t)messageSize) {
                continue;
            }

            if (messageSize < 4 || messageSize > buffer.size()) {
                closesocket(clientSock);
                return;
            }

            if (offset < messageSize) {
                continue;
            }

            std::vector<uint8_t> message(buffer.begin() + 4, buffer.begin() + messageSize);
            callback(std::move(message));
            std::copy(buffer.begin() + messageSize, buffer.begin() + offset, buffer.begin());
            offset -= messageSize;
        }
        closesocket(clientSock);
    }

    MessageCallback callback;
    SOCKET sock;
    std::atomic_bool quit;
    std::mutex mtx;
    std::vector<std::thread> threads;
};

inline bool SendArgv(int argc, wchar_t** argv) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        return false;
    }

    sockaddr_in sockAddr{};
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sockAddr.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    std::vector<uint8_t> buffer;
    for (int i = 1; i < argc; i++) {
        std::wstring arg = argv[i];
        int messageLen = 4 + (int)arg.size() * 2;
        buffer.push_back((messageLen >>  0) & 0xFF);
        buffer.push_back((messageLen >>  8) & 0xFF);
        buffer.push_back((messageLen >> 16) & 0xFF);
        buffer.push_back((messageLen >> 24) & 0xFF);
        for (wchar_t c : arg) {
            buffer.push_back((c >> 0) & 0xFF);
            buffer.push_back((c >> 8) & 0xFF);
        }
    }

    bool success = send(sock, (char*)buffer.data(), (int)buffer.size(), 0) != SOCKET_ERROR;
    closesocket(sock);
    return success;
}
