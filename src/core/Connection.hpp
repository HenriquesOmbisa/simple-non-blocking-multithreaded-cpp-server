#pragma once
#include <string>
#include <chrono>

namespace mojoraw::core {

    enum class ConnState {
        HTTP,
        WEBSOCKET,
        CLOSING
    };

    struct Connection {
        int fd = -1;
        std::string readBuffer{};
        std::string writeBuffer{};
        ConnState state = ConnState::HTTP;
        bool keepAlive = true;
        std::chrono::steady_clock::time_point lastActivity = std::chrono::steady_clock::now();
    };

}
