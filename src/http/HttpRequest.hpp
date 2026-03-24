#pragma once
#include <string>
#include <unordered_map>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>

namespace mojoraw::core {

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string version;

    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query;
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> cookies;

    std::string body;

    /* ===========================
       HEADER
    =========================== */

    std::string header(const std::string& key) const {
        auto it = headers.find(toLower(key));
        return it != headers.end() ? it->second : "";
    }

    bool hasHeader(const std::string& key) const {
        return headers.contains(toLower(key));
    }

    /* ===========================
       QUERY
    =========================== */

    std::string queryParam(const std::string& key) const {
        auto it = query.find(key);
        return it != query.end() ? it->second : "";
    }

    /* ===========================
       PARAMS (router)
    =========================== */

    std::string param(const std::string& key) const {
        auto it = params.find(key);
        return it != params.end() ? it->second : "";
    }

    /* ===========================
       COOKIES
    =========================== */

    std::string cookie(const std::string& key) const {
        auto it = cookies.find(key);
        return it != cookies.end() ? it->second : "";
    }

    /* ===========================
       AUTH
    =========================== */

    std::string authorization() const {
        return header("authorization");
    }

    bool isJson() const {
        auto ct = toLower(header("content-type"));
        return ct.find("application/json") != std::string::npos;
    }

    bool isAjax() const {
        return toLower(header("x-requested-with")) == "xmlhttprequest";
    }

    /* ===========================
       INTERNAL PARSERS
    =========================== */

    void parseQuery(const std::string& fullPath) {
        query.clear();
        auto pos = fullPath.find('?');
        if (pos == std::string::npos) {
            path = fullPath;
            return;
        }

        path = fullPath.substr(0, pos);
        const std::string q = fullPath.substr(pos + 1);

        static const std::regex queryRe(R"(([^&=]+)(?:=([^&]*))?)");
        for (std::sregex_iterator it(q.begin(), q.end(), queryRe), end; it != end; ++it) {
            const std::string key = urlDecode((*it)[1].str());
            const std::string val = (*it)[2].matched
                ? urlDecode((*it)[2].str())
                : "";
            query[key] = val;
        }
    }

    void parseCookies() {
        cookies.clear();
        const std::string raw = header("cookie");
        if (raw.empty()) return;

        static const std::regex cookieRe(R"(\s*([^=;\s]+)\s*=\s*([^;]*))");
        for (std::sregex_iterator it(raw.begin(), raw.end(), cookieRe), end; it != end; ++it) {
            const std::string key = (*it)[1].str();
            const std::string val = urlDecode((*it)[2].str());
            cookies[key] = val;
        }
    }

private:
    static std::string toLower(const std::string& in) {
        std::string out = in;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return out;
    }

    static std::string urlDecode(const std::string& in) {
        std::string out;
        out.reserve(in.size());

        for (std::size_t i = 0; i < in.size(); ++i) {
            const char c = in[i];
            if (c == '+') {
                out.push_back(' ');
                continue;
            }
            if (c == '%' && i + 2 < in.size()) {
                const auto hexVal = [](char h) -> int {
                    if (h >= '0' && h <= '9') return h - '0';
                    if (h >= 'a' && h <= 'f') return 10 + (h - 'a');
                    if (h >= 'A' && h <= 'F') return 10 + (h - 'A');
                    return -1;
                };
                const int hi = hexVal(in[i + 1]);
                const int lo = hexVal(in[i + 2]);
                if (hi >= 0 && lo >= 0) {
                    out.push_back(static_cast<char>((hi << 4) | lo));
                    i += 2;
                    continue;
                }
            }
            out.push_back(c);
        }
        return out;
    }
};

}
