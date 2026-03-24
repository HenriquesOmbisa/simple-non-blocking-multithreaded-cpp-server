#include "core/EventLoop.hpp"

int main() {
    mojoraw::core::EventLoop loop;
    loop.init(8080);
    loop.run();
}