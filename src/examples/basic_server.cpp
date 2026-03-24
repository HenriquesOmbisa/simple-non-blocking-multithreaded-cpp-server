#include "http/Router.hpp"
#include "core/EventLoop.hpp"
#include "http/Render.hpp"
#include "http/TemplateEngine.hpp"

#include <filesystem>
#include <regex>
#include <vector>
#ifdef MOJORAW_HAS_ZLIB
#include <zlib.h>
#endif

using namespace mojoraw::core;

namespace {

#ifdef MOJORAW_HAS_ZLIB
bool isCompressibleMime(const std::string& mime) {
    return mime.rfind("text/", 0) == 0
        || mime.find("json") != std::string::npos
        || mime.find("javascript") != std::string::npos
        || mime.find("svg") != std::string::npos;
}

bool acceptsGzip(const HttpRequest& req) {
    return req.header("accept-encoding").find("gzip") != std::string::npos;
}
#endif

bool isVersionedAsset(const std::string& path) {
    static const std::regex hashedAsset(R"(\.[A-Fa-f0-9]{8,}\.)");
    return std::regex_search(path, hashedAsset);
}

#ifdef MOJORAW_HAS_ZLIB
bool gzipCompress(const std::string& input, std::string& out) {
    if (input.empty()) {
        out.clear();
        return true;
    }

    z_stream zs{};
    if (deflateInit2(&zs,
                     Z_BEST_SPEED,
                     Z_DEFLATED,
                     15 + 16,
                     8,
                     Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    zs.avail_in = static_cast<uInt>(input.size());

    std::vector<char> buffer(16384);
    out.clear();

    int ret = Z_OK;
    while (ret == Z_OK) {
        zs.next_out = reinterpret_cast<Bytef*>(buffer.data());
        zs.avail_out = static_cast<uInt>(buffer.size());

        ret = deflate(&zs, Z_FINISH);
        const std::size_t produced = buffer.size() - zs.avail_out;
        if (produced > 0) out.append(buffer.data(), produced);
    }

    deflateEnd(&zs);
    return ret == Z_STREAM_END;
}
#endif

} // namespace

int main() {

    Router router;

    // ---- Route with path parameter ----
    router.get("/users/:id", [](HttpRequest& req) {
        std::string id      = req.param("id");
        std::string token   = req.authorization();
        std::string session = req.cookie("sessionId");

        HttpResponse res;
        return res.json(
            R"({"id":")" + id + R"(","session":")" + session + R"("})"
        );
    });

    // ---- Route with query parameters ----
    router.get("/order", [](HttpRequest& req) {
        std::string name = req.queryParam("name");
        HttpResponse res;
        return res.json(R"({"name":")" + name + R"("})");
    });

    // ---- POST route with request body ----
    router.post("/echo", [](HttpRequest& req) {
        HttpResponse res;
        return res.status(200).json(req.body);
    });

    // ---- Basic demo routes ----
    router.get("/hello", [](const HttpRequest&) {
        HttpResponse res;
        return res.send("Hello from MojoRaw!");
    });

    router.get("/", [](const HttpRequest&) {
        HttpResponse res;
        return res.render("<h1>MojoRaw Server</h1><p>Non-blocking, multi-threaded C++20</p>");
    });

    router.get("/json", [](const HttpRequest&) {
        HttpResponse res;
        return res.json(R"({"name":"MojoRaw","version":"0.2.0","threaded":true})");
    });

    // ---- MojoView template route (if/elseif/else/for/include) ----
    router.get("/view", [](HttpRequest& req) {
        HttpResponse res;

        TemplateEngine::Context ctx;
        ctx["name"]         = req.queryParam("name").empty()    ? "Developer" : req.queryParam("name");
        ctx["premium"]      = req.queryParam("premium").empty() ? "false"     : req.queryParam("premium");
        ctx["tier"]         = req.queryParam("tier").empty()    ? "pro"       : req.queryParam("tier");
        ctx["message"]      = req.queryParam("message").empty()
                                ? "<script>alert('xss')</script>"
                                : req.queryParam("message");
        ctx["message_raw"]  = "<em>Render HTML raw controlado no servidor</em>";
        ctx["footer_note"]  = "MojoView .mj.html template engine";

        // Rich list context: vector<RowContext> — use {{ item.name }}, {{ item.price }}
        ctx["plans"] = std::vector<TemplateEngine::RowContext>{
            {{"name", "Starter"},    {"price", "Grátis"},  {"badge", "basic"}},
            {{"name", "Pro"},        {"price", "$29/mês"}, {"badge", "pro"}},
            {{"name", "Enterprise"}, {"price", "Custom"},  {"badge", "enterprise"}},
        };

        const std::filesystem::path templatePath =
            std::filesystem::current_path() / "src/static/templates/page.mj.html";

        std::string html;
        if (!TemplateEngine::renderFile(templatePath.string(), ctx, html)) {
            return res.status(500).send("Template not found");
        }

        return res.render(html);
    });

    // ---- MojoView layout inheritance route (/home extends layout.mj.html) ----
    router.get("/home", [](HttpRequest& req) {
        HttpResponse res;

        TemplateEngine::Context ctx;
        ctx["name"]       = req.queryParam("name").empty() ? "Visitante" : req.queryParam("name");
        ctx["logged_in"]  = req.queryParam("logged_in");
        ctx["footer_note"] = "MojoRaw Server";

        ctx["features"] = std::vector<TemplateEngine::RowContext>{
            {{"title", "Non-blocking I/O"}, {"desc", "epoll edge-triggered, zero locks between threads."}},
            {{"title", "SO_REUSEPORT"},     {"desc", "Each thread owns its own socket — true parallel accept."}},
            {{"title", "MojoView"},         {"desc", "Template engine in .mj.html: if/for/include/extends/block."}},
            {{"title", "Gzip + Cache"},     {"desc", "ETag, Cache-Control, precompressed .gz fallback."}},
        };

        const std::filesystem::path templatePath =
            std::filesystem::current_path() / "src/static/templates/home.mj.html";

        std::string html;
        if (!TemplateEngine::renderFile(templatePath.string(), ctx, html)) {
            return res.status(500).send("Template not found");
        }

        return res.render(html);
    });

    // ---- Redirect ----
    router.get("/old", [](const HttpRequest&) {
        HttpResponse res;
        return res.redirect("/", 301);
    });

    // ---- Documentation portal routes (/doc, /doc/{lang}, /doc/{lang}/{section}) ----
    const std::filesystem::path docsRoot =
        std::filesystem::weakly_canonical(std::filesystem::current_path() / "src/static/doc");

    auto serveDoc = [docsRoot](HttpRequest& req) {
        HttpResponse res;

        std::string rel = req.param("wildcard");
        while (!rel.empty() && rel.front() == '/') rel.erase(rel.begin());

        std::filesystem::path requested = rel.empty()
            ? docsRoot / "index.html"
            : docsRoot / rel;

        if (requested.extension().empty()) {
            if (std::filesystem::exists(requested) && std::filesystem::is_directory(requested)) {
                requested /= "index.html";
            } else {
                requested += ".html";
            }
        }

        std::filesystem::path resolved = std::filesystem::weakly_canonical(requested);

        const std::string rootStr = docsRoot.string();
        const std::string resolvedStr = resolved.string();
        if (resolvedStr.rfind(rootStr, 0) != 0) {
            return res.status(403).send("Forbidden");
        }

        if (!std::filesystem::exists(resolved) || std::filesystem::is_directory(resolved)) {
            return res.status(404).send("Doc page not found");
        }

        std::string content;
        if (!Render::file(resolved.string(), content)) {
            return res.status(500).send("Failed to read doc page");
        }

        return res.status(200).sendFile(content, Render::mimeType(resolved.string()));
    };

    router.get("/doc", [serveDoc](HttpRequest& req) mutable {
        return serveDoc(req);
    });

    router.get("/doc/*", [serveDoc](HttpRequest& req) mutable {
        return serveDoc(req);
    });

    // ---- Static files with HTTP caching ----
    const std::filesystem::path staticRoot =
        std::filesystem::weakly_canonical(std::filesystem::current_path() / "src/static");

    router.get("/static/*", [staticRoot](HttpRequest& req) {
        HttpResponse res;

        std::string rel = req.param("wildcard");
        if (rel.empty()) {
            return res.status(400).send("Bad static path");
        }

        std::filesystem::path requested = staticRoot / rel;
        std::filesystem::path resolved = std::filesystem::weakly_canonical(requested);

        // Block path traversal outside the static root.
        const std::string rootStr = staticRoot.string();
        const std::string resolvedStr = resolved.string();
        if (resolvedStr.rfind(rootStr, 0) != 0) {
            return res.status(403).send("Forbidden");
        }

        if (!std::filesystem::exists(resolved) || std::filesystem::is_directory(resolved)) {
            return res.status(404).send("Static file not found");
        }

        const std::string mime = Render::mimeType(resolved.string());
        const bool clientAcceptsGzip = req.header("accept-encoding").find("gzip") != std::string::npos;

        const std::filesystem::path precompressed = resolved.string() + ".gz";
        const bool hasPrecompressed =
            clientAcceptsGzip
            && std::filesystem::exists(precompressed)
            && !std::filesystem::is_directory(precompressed);

        bool useGzip = hasPrecompressed;
    #ifdef MOJORAW_HAS_ZLIB
        useGzip = hasPrecompressed || (acceptsGzip(req) && isCompressibleMime(mime));
    #endif
        const std::string cacheControl = isVersionedAsset(rel)
            ? "public, max-age=31536000, immutable"
            : "public, max-age=120";

        // Build a weak ETag from file size + mtime (+ encoding suffix).
        const std::filesystem::path etagPath = hasPrecompressed ? precompressed : resolved;
        const auto fileSize = std::filesystem::file_size(etagPath);
        const auto mtime = std::filesystem::last_write_time(etagPath).time_since_epoch().count();
        const std::string etag = "W/\""
            + std::to_string(fileSize)
            + "-"
            + std::to_string(mtime)
            + (useGzip ? "-gz" : "-id")
            + "\"";

        if (req.header("if-none-match") == etag) {
            return res.status(304)
                .setHeader("ETag", etag)
                .setHeader("Cache-Control", cacheControl)
                .setHeader("Vary", "Accept-Encoding");
        }

        std::string content;
        const std::string sourcePath = hasPrecompressed ? precompressed.string() : resolved.string();
        if (!Render::file(sourcePath, content)) {
            return res.status(500).send("Failed to read static file");
        }

        std::string payload = content;
#ifdef MOJORAW_HAS_ZLIB
        if (!hasPrecompressed && useGzip) {
            std::string compressed;
            if (gzipCompress(content, compressed) && compressed.size() < content.size()) {
                payload = std::move(compressed);
            }
        }
#endif

        const bool encoded = hasPrecompressed || (payload.size() != content.size());

        HttpResponse response = res
            .status(200)
            .setHeader("ETag", etag)
            .setHeader("Cache-Control", cacheControl)
            .setHeader("Vary", "Accept-Encoding")
            .sendFile(payload, mime);

        if (encoded) response.setHeader("Content-Encoding", "gzip");
        return response;
    });

    // Start with 0 threads to auto-select all available CPU cores.
    EventLoop loop(router);
    loop.init(8080);
    loop.run();

    return 0;
}
