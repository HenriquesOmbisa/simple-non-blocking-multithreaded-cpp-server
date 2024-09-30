#ifndef ROUTE_HPP
#define ROUTE_HPP

#include <string>
#include <functional>

namespace http{
    struct Route
    {
        std::string method;
        std::string path;
        std::function<std::string()> callback;
        Route(std::string method, std::string path, std::function<std::string()> callback) : method(method), path(path), callback(callback) {}
    };
}

#endif