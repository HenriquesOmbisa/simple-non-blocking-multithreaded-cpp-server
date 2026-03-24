#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <filesystem>

namespace mojoraw::core {

struct Render {
    static bool file(const std::string& filename, std::string& outContent) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;
        std::stringstream buffer;
        buffer << file.rdbuf();
        outContent = buffer.str();
        return true;
    }

    static std::string mimeType(const std::string& filename) {
        const std::filesystem::path p(filename);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        static const std::unordered_map<std::string, std::string> MIME = {
            {".html", "text/html; charset=utf-8"},
            {".css",  "text/css; charset=utf-8"},
            {".js",   "application/javascript; charset=utf-8"},
            {".json", "application/json; charset=utf-8"},
            {".txt",  "text/plain; charset=utf-8"},
            {".svg",  "image/svg+xml"},
            {".png",  "image/png"},
            {".jpg",  "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif",  "image/gif"},
            {".webp", "image/webp"},
            {".ico",  "image/x-icon"}
        };

        auto it = MIME.find(ext);
        return it != MIME.end() ? it->second : "application/octet-stream";
    }
};

} // namespace mojoraw::core