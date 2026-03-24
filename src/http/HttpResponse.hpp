#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

namespace mojoraw::core {

class HttpResponse {
private:
    int         statusCode_ = 200;
    std::string statusText_ = "OK";
    std::unordered_map<std::string, std::string> headers_;
    std::vector<std::string>                     cookies_;
    std::string body_;

    static std::string statusText(int code) {
        switch (code) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 409: return "Conflict";
            case 413: return "Payload Too Large";
            case 422: return "Unprocessable Entity";
            case 429: return "Too Many Requests";
            case 500: return "Internal Server Error";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            default:  return "Unknown";
        }
    }

public:
    /* ---- Status ---- */
    HttpResponse& status(int code) {
        statusCode_ = code;
        statusText_ = statusText(code);
        return *this;
    }

    /* ---- Headers ---- */
    HttpResponse& setHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
        return *this;
    }

    /* ---- Body helpers ---- */
    HttpResponse& send(const std::string& content) {
        body_ = content;
        headers_["Content-Type"] = "text/plain; charset=utf-8";
        return *this;
    }

    HttpResponse& render(const std::string& html) {
        body_ = html;
        headers_["Content-Type"] = "text/html; charset=utf-8";
        return *this;
    }

    HttpResponse& json(const std::string& jsonStr) {
        body_ = jsonStr;
        headers_["Content-Type"] = "application/json; charset=utf-8";
        return *this;
    }

    /* Serve static file content with explicit Content-Type. */
    HttpResponse& sendFile(const std::string& content, const std::string& mimeType) {
        body_ = content;
        headers_["Content-Type"] = mimeType;
        return *this;
    }

    /* ---- Redirect ---- */
    HttpResponse& redirect(const std::string& url, int code = 302) {
        status(code);
        headers_["Location"] = url;
        body_.clear();
        return *this;
    }

    /* ---- Cookie ---- */
    HttpResponse& setCookie(const std::string& name,
                            const std::string& value,
                            const std::string& options = "") {
        std::string c = name + "=" + value;
        if (!options.empty()) c += "; " + options;
        cookies_.push_back(c);
        return *this;
    }

    /* ---- Serialização final ---- */
    std::string toString(bool keepAlive) const {
        std::ostringstream out;

        out << "HTTP/1.1 " << statusCode_ << " " << statusText_ << "\r\n";

        for (const auto& [k, v] : headers_)
            out << k << ": " << v << "\r\n";

        for (const auto& c : cookies_)
            out << "Set-Cookie: " << c << "\r\n";

        out << "Content-Length: " << body_.size() << "\r\n";
        out << "Connection: " << (keepAlive ? "keep-alive" : "close") << "\r\n";
        out << "\r\n";
        out << body_;

        return out.str();
    }
};

} // namespace mojoraw::core
