#pragma once

#include <string>

namespace mojoraw::core {

class Socket {
public:
    // reusePort=true enables SO_REUSEPORT for lock-free multi-thread accept loops.
    static int  createServer   (int port, bool reusePort = false);
    static int  acceptClient   (int serverFd);
    static void setNonBlocking (int fd);
    static void closeFd        (int fd);
};

} // namespace mojoraw::core
