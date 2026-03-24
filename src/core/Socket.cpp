#include "Socket.hpp"

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace mojoraw::core {

    int Socket::createServer(int port, bool reusePort) {
        int serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd < 0) {
            perror("socket");
            return -1;
        }

        int opt = 1;
        if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt SO_REUSEADDR");
            close(serverFd);
            return -1;
        }

        if (reusePort) {
            if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
                perror("setsockopt SO_REUSEPORT");
                close(serverFd);
                return -1;
            }
        }

        setNonBlocking(serverFd);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(static_cast<uint16_t>(port));

        if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(serverFd);
            return -1;
        }

        if (listen(serverFd, SOMAXCONN) < 0) {
            perror("listen");
            close(serverFd);
            return -1;
        }

        return serverFd;
    }

    int Socket::acceptClient(int serverFd) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientLen);

        if (clientFd < 0) {
            return -1;
        }

        setNonBlocking(clientFd);
        return clientFd;
    }

    void Socket::setNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) { perror("fcntl get"); return; }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            perror("fcntl set");
        }
    }

    void Socket::closeFd(int fd) {
        close(fd);
    }

}
