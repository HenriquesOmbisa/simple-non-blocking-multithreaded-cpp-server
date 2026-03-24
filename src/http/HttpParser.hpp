#pragma once
#include <sstream>
#include <algorithm>
#include <charconv>
#include <regex>
#include <string_view>
#include <cctype>

namespace mojoraw::core {

class HttpParser {
public:
    /* -------------------------------------------------------
         Checks whether the buffer contains a full HTTP request.
         Prevents partial parsing with edge-triggered epoll.
    ------------------------------------------------------- */
    static bool isComplete(const std::string& raw) {
        static constexpr std::string_view SEP = "\r\n\r\n";
        auto headerEnd = raw.find(SEP);
        if (headerEnd == std::string::npos) return false;

        // Check whether a request body is expected.
        auto clPos = raw.find("Content-Length:");
        if (clPos != std::string::npos && clPos < headerEnd) {
            std::size_t valStart = clPos + 15;
            while (valStart < raw.size() && raw[valStart] == ' ') ++valStart;
            std::size_t valEnd = raw.find("\r\n", valStart);
            if (valEnd == std::string::npos) return false;

            std::size_t contentLength = 0;
            auto [ptr, ec] = std::from_chars(
                raw.data() + valStart,
                raw.data() + valEnd,
                contentLength
            );
            if (ec != std::errc{}) return false;

            std::size_t bodyStart = headerEnd + SEP.size();
            return (raw.size() - bodyStart) >= contentLength;
        }
        return true;
    }

    /* -------------------------------------------------------
         Full parse: request line, headers, query,
         cookies, and body.
    ------------------------------------------------------- */
    static bool parseRequest(const std::string& raw, HttpRequest& req) {
        req = HttpRequest{};

        static constexpr std::string_view SEP = "\r\n\r\n";
        const std::size_t headerEnd = raw.find(SEP);
        if (headerEnd == std::string::npos) return false;

        std::size_t lineEnd = raw.find("\r\n");
        if (lineEnd == std::string::npos) return false;

        const std::string requestLine = raw.substr(0, lineEnd);
        static const std::regex requestLineRe(R"(^([A-Z]+)\s+(\S+)\s+(HTTP\/\d\.\d)$)");
        std::smatch requestMatch;
        if (!std::regex_match(requestLine, requestMatch, requestLineRe)) return false;

        req.method  = requestMatch[1].str();
        req.version = requestMatch[3].str();
        req.parseQuery(requestMatch[2].str());

        const std::string headersBlock = raw.substr(lineEnd + 2, headerEnd - (lineEnd + 2));
        std::istringstream hs(headersBlock);
        std::string headerLine;
        static const std::regex headerRe(R"(^\s*([^:\r\n]+)\s*:\s*(.*?)\s*$)");

        while (std::getline(hs, headerLine)) {
            if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
            if (headerLine.empty()) continue;

            std::smatch hm;
            if (!std::regex_match(headerLine, hm, headerRe)) continue;

            std::string key = hm[1].str();
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            req.headers[key] = hm[2].str();
        }

        req.parseCookies();

        const std::size_t bodyStart = headerEnd + SEP.size();
        req.body.clear();
        auto clIt = req.headers.find("content-length");
        if (clIt != req.headers.end()) {
            std::size_t contentLength = 0;
            auto [ptr, ec] = std::from_chars(
                clIt->second.data(),
                clIt->second.data() + clIt->second.size(),
                contentLength
            );
            if (ec != std::errc{}) return false;
            if (bodyStart + contentLength > raw.size()) return false;
            req.body = raw.substr(bodyStart, contentLength);
        }

        return true;
    }

    static bool isKeepAlive(const HttpRequest& req) {
        auto it = req.headers.find("connection");
        if (it != req.headers.end()) {
            std::string val = it->second;
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            if (val == "close") return false;
            if (val == "keep-alive") return true;
        }
        return req.version == "HTTP/1.1";
    }
};

} // namespace mojoraw::core
