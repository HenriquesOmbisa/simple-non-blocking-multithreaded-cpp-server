#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <regex>
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

namespace mojoraw::core {

class Router {
public:
    using Handler    = std::function<HttpResponse(HttpRequest&)>;
    using Middleware = std::function<bool(HttpRequest&, HttpResponse&)>;

     /* ----------------------------------------
         Route registration
     ---------------------------------------- */
    void get   (const std::string& path, Handler h) { addRoute("GET",    path, std::move(h)); }
    void post  (const std::string& path, Handler h) { addRoute("POST",   path, std::move(h)); }
    void put   (const std::string& path, Handler h) { addRoute("PUT",    path, std::move(h)); }
    void patch (const std::string& path, Handler h) { addRoute("PATCH",  path, std::move(h)); }
    void del   (const std::string& path, Handler h) { addRoute("DELETE", path, std::move(h)); }

    /* Global middleware - return false to short-circuit request handling. */
    void use(Middleware mw) { middlewares_.push_back(std::move(mw)); }

    /* ----------------------------------------
       Request dispatch
    ---------------------------------------- */
    HttpResponse route(HttpRequest& req) const {
        // Execute middleware chain in registration order.
        HttpResponse mwRes;
        for (auto& mw : middlewares_) {
            if (!mw(req, mwRes)) return mwRes;
        }

        // Try to match against registered routes.
        for (const auto& r : routes_) {
            std::unordered_map<std::string, std::string> params;
            if (match(r, req.method, req.path, params)) {
                req.params = std::move(params);
                return r.handler(req);
            }
        }

        // Default 404 response.
        HttpResponse res;
        return res.status(404).json(R"({"error":"Not Found","status":404})");
    }

private:
    struct Route {
        std::string              method;
        std::regex               pathRegex;
        std::vector<std::string> paramKeys;
        Handler                  handler;
    };

    std::vector<Route>      routes_;
    std::vector<Middleware> middlewares_;

    void addRoute(const std::string& method, const std::string& path, Handler h) {
        Route route;
        route.method = method;
        route.pathRegex = compilePathRegex(path, route.paramKeys);
        route.handler = std::move(h);
        routes_.push_back(std::move(route));
    }

    /* Split "/users/:id/orders" into {"users", ":id", "orders"}. */
    static std::vector<std::string> splitPath(const std::string& path) {
        std::vector<std::string> segs;
        std::stringstream ss(path);
        std::string seg;
        while (std::getline(ss, seg, '/'))
            if (!seg.empty()) segs.push_back(seg);
        return segs;
    }

    bool match(const Route& route,
               const std::string& method,
               const std::string& path,
               std::unordered_map<std::string, std::string>& params) const {
        if (route.method != method) return false;

        std::smatch m;
        if (!std::regex_match(path, m, route.pathRegex)) return false;

        for (std::size_t i = 0; i < route.paramKeys.size(); ++i) {
            const std::size_t group = i + 1;
            if (group < m.size()) {
                params[route.paramKeys[i]] = m[group].str();
            }
        }
        return true;
    }

    static std::regex compilePathRegex(const std::string& path,
                                       std::vector<std::string>& paramKeys) {
        const std::vector<std::string> segments = splitPath(path.empty() ? "/" : path);
        std::string pattern = "^";

        if (segments.empty()) {
            pattern += "/?";
        } else {
            for (const std::string& seg : segments) {
                pattern += "/";
                if (!seg.empty() && seg[0] == ':') {
                    paramKeys.push_back(seg.substr(1));
                    pattern += "([^/]+)";
                } else if (seg == "*") {
                    paramKeys.push_back("wildcard");
                    pattern += "(.*)";
                } else {
                    pattern += escapeRegexLiteral(seg);
                }
            }
            pattern += "/?";
        }

        pattern += "$";
        return std::regex(pattern);
    }

    static std::string escapeRegexLiteral(const std::string& value) {
        std::string out;
        out.reserve(value.size() * 2);
        for (char c : value) {
            switch (c) {
                case '.': case '^': case '$': case '|': case '(': case ')':
                case '[': case ']': case '{': case '}': case '*': case '+':
                case '?': case '\\':
                    out.push_back('\\');
                    out.push_back(c);
                    break;
                default:
                    out.push_back(c);
                    break;
            }
        }
        return out;
    }
};

} // namespace mojoraw::core
