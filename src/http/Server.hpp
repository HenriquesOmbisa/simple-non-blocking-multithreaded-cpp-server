#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <cstdlib>
#include <map>
#include <vector>
#include <thread>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <functional>

#include "./Route.hpp"
#include "./Render.hpp"
constexpr unsigned int MAX_EPOLL_EVENTS = 1024;
constexpr unsigned int MAX_BUFFER_SIZE = 1024;
namespace http{
    class Server
    {
    private:
        uint32_t host;
        int port;
        int serverSocket;
        int clientSocket;
        int epollFD;
        int numRead;
        struct sockaddr_in serverAddr;
        struct sockaddr_in clientAddr;
        struct epoll_event epollEvent;
        struct epoll_event events[MAX_EPOLL_EVENTS];
        char buffer[MAX_BUFFER_SIZE];
        std::vector<std::thread> handles;
        std::function<std::string()> handle;
        std::vector<http::Route> routes;
        std::string method;

        void init();
        void run();
        void handleClient(int clientSocket_);
        std::string getFileExtension(const std::string& filename);
        std::string getMimeType(std::string& fileName);

    public:
        Server();
        void Listen( int port);
        void setHost(const std::string _host);
        void get(const std::string& path, std::function<std::string()>&& callback);
        ~Server();

    };
}

#endif