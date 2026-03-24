#pragma once

#include <unordered_map>
#include <vector>
#include <thread>
#include <sys/epoll.h>
#include "Connection.hpp"
#include "../http/HttpResponse.hpp"
#include "../http/HttpRequest.hpp"
#include "../http/HttpParser.hpp"
#include "../http/Router.hpp"

namespace mojoraw::core {

class EventLoop {
public:
    explicit EventLoop(Router& r);

    /* port    - TCP port to listen on
       threads - number of worker threads (0 = use all CPU cores) */
    void init(int port, int threads = 0);
    void run();

private:
    static constexpr int    MAX_EVENTS       = 1024;
   static constexpr size_t READ_BUFFER_SIZE = 65536; // 64 KB
   static constexpr size_t MAX_REQUEST_BYTES = 2 * 1024 * 1024; // 2 MB
   static constexpr int    EPOLL_TIMEOUT_MS  = 1000;
   static constexpr int    IDLE_TIMEOUT_MS   = 30000;

    Router& router_;
    int     port_       = 0;
    int     numThreads_ = 1;

    /* Each worker runs fully independently with its own
       serverFd + epollFd + connection map (no shared locks). */
    void runWorker();

    void handleAccept  (int epollFd, int serverFd,
                        std::unordered_map<int, Connection>& conns);
    void handleRead    (int fd,
                        std::unordered_map<int, Connection>& conns,
                        int epollFd);
    void handleWrite   (int fd,
                        std::unordered_map<int, Connection>& conns,
                        int epollFd);
    void closeConn     (int fd,
                        std::unordered_map<int, Connection>& conns,
                        int epollFd);
      void pruneIdleConnections(std::unordered_map<int, Connection>& conns,
                          int epollFd);
};

} // namespace mojoraw::core
