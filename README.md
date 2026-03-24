# MojoRaw

> A non-blocking, multi-threaded HTTP server in C++20 — built with curiosity.

<div align="center">

🌐 **Available in:**
[🇵🇹 Português](./docs/README.pt.md) &nbsp;|&nbsp;
[🇪🇸 Español](./docs/README.es.md) &nbsp;|&nbsp;
[🇫🇷 Français](./docs/README.fr.md) &nbsp;|&nbsp;
[🇨🇳 中文](./docs/README.zh.md) &nbsp;|&nbsp;
[🇩🇪 Deutsch](./docs/README.de.md)

</div>

---

## Docs Portal (/doc)

The project now includes a built-in local/web docs portal served by the server itself.

Base routes:

- `/doc`
- `/doc/{lang}`
- `/doc/{lang}/{section}`

Languages:

- `pt`, `es`, `fr`, `de`, `zh`

Sections:

- `overview`, `setup`, `architecture`, `routes`, `templates`, `production`

Example URLs:

- `/doc/pt/overview`
- `/doc/es/setup`
- `/doc/fr/templates`
- `/doc/de/production`

Docs files are under `src/static/doc`, and a local authoring note lives in `doc/README.md`.

---

## The Story Behind This

C and C++ were the first languages I learned. But for a long time I had this nagging feeling: _"ok, I know C++… but what do I actually do with it?"_

Back then I was already working professionally with PHP, Node.js, and had even built desktop systems with JavaFX and Swing. Then one day I had an unusual thought: **what if I use C++ to build a web server?**

It was basic, it was rough — but it worked 😆😆😆

The project sat on GitHub since 2024. Recently I got nostalgic, took a look at it and thought: _"what if I actually improve this?"_. I went back to research, rewrote a lot of it, and here it is — what we can call **v1.2**, now with a proper name. The old `simple-non-blocking-multithreaded-cpp-server` was too long and too generic. **MojoRaw** feels right.

This project is **open source** and open to any tech enthusiast or curious mind like me. Clone it, improve it, open an issue or send a PR — you're welcome here.

---

## What is MojoRaw

MojoRaw is an HTTP/1.1 server in C++20 focused on performance, simplicity, and full control over the network flow.

It combines:

- Event loop with `epoll` (edge-triggered)
- Real multi-threading with `SO_REUSEPORT`
- Manual and efficient HTTP parser
- Router with path params and wildcard
- Static file server with HTTP cache and gzip
- Custom template language (`.mj.html`) for SSR

---

## 1. Overview

### Goal

Build a modern HTTP server in C++ without relying on heavy frameworks, while keeping:

- high runtime predictability
- low overhead
- small and clear API
- easy path to production

### Key Features

- Non-blocking I/O with `epoll`
- Per-thread scalability with `SO_REUSEPORT` (each worker accepts connections)
- HTTP/1.1 Keep-alive
- Payload limit for protection (`413 Payload Too Large`)
- Idle connection timeout
- Regex parsing for request-line, headers, query and cookies
- Router with `:param` and `*` (wildcard)
- ETag + `Cache-Control` + `304 Not Modified`
- Gzip via pre-compressed files (`.gz`) and optional on-the-fly compression
- Automatic static asset pre-compression at build time
- `MojoView` template engine with `if`, `elseif`, `else`, `for`, `include`, `extends`, `block`, filters and HTML escaping

---

## 2. Project Structure

```
src/
    CMakeLists.txt
    core/
        EventLoop.hpp / EventLoop.cpp
        Socket.hpp / Socket.cpp
        Connection.hpp
    http/
        HttpParser.hpp
        HttpRequest.hpp
        HttpResponse.hpp
        Router.hpp
        Render.hpp
        TemplateEngine.hpp
    examples/
        basic_server.cpp
    static/
        index.html
        style.css
        templates/
            page.mj.html
            layout.mj.html
            home.mj.html
            partials/
                plan-item.mj.html
    cmake/
        PrecompressStatic.cmake
```

---

## 3. Requirements

### Required

- Linux (recommended)
- CMake >= 3.16
- C++20 compiler (`g++` or `clang++`)
- `make` (or equivalent generator)
- `gzip` (recommended for asset pre-compression)

### Optional

- `zlib` for on-the-fly response compression at runtime

---

## 4. Build & Run

```bash
# Build
cmake -S src -B build
cmake --build build

# Run
./build/mojoraw_example

# Clean
rm -rf build
```

Listens on `:8080` by default.

---

## 5. CMake Configuration

### `MOJORAW_PRECOMPRESS_STATIC_GZIP` (default: `ON`)

When active, the build automatically generates `.gz` files for text assets in `src/static`.

```bash
cmake -S src -B build -DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF
```

### ZLIB

If found via `find_package(ZLIB)`, enables `MOJORAW_HAS_ZLIB=1` for on-the-fly gzip. Without ZLIB, the server still works using pre-compressed `.gz` fallback.

---

## 6. Architecture

### 6.1 Per-worker event loop

Each worker creates its own `serverFd`, `epollFd`, and connection map (`unordered_map<int, Connection>`). Result: no shared lock between workers, lower contention under high connection rates.

### 6.2 Connection cycle

1. `accept4(..., SOCK_NONBLOCK)`
2. Non-blocking buffer read
3. Detect complete request (`\r\n\r\n` + `Content-Length` when applicable)
4. HTTP parse
5. Router dispatch
6. Response serialization
7. Non-blocking write
8. Back to `EPOLLIN` if keep-alive, or close connection

### 6.3 Protections

- `MAX_REQUEST_BYTES = 2MB` → returns `413`
- Idle connection > `IDLE_TIMEOUT_MS` → closed

---

## 7. HTTP Parser

**Request line:** `^([A-Z]+)\s+(\S+)\s+(HTTP\/\d\.\d)$`

**Headers:** `^\s*([^:\r\n]+)\s*:\s*(.*?)\s*$` (normalized to lowercase)

**Query string & Cookies:** Regex-parsed with URL decode support (`%XX` and `+`).

---

## 8. Router

Supports method-based routing (`get`, `post`, `put`, `patch`, `del`), named params (`:id`), and wildcards (`*`). Routes are compiled to `std::regex` at registration time — not per request.

---

## 9. Example Routes

| Route                 | Description                             |
| --------------------- | --------------------------------------- |
| `GET /hello`          | Plain text response                     |
| `GET /json`           | JSON response                           |
| `POST /echo`          | Returns received body                   |
| `GET /users/:id`      | Path param + cookie                     |
| `GET /order?name=...` | Query param                             |
| `GET /old`            | 301 redirect to `/`                     |
| `GET /view`           | Renders `page.mj.html`                  |
| `GET /home`           | Template with `extends`/`block`         |
| `GET /static/*`       | Static assets with ETag, cache and gzip |

---

## 10. HTTP Cache & Gzip

**ETag** based on file size, mtime and encoding suffix. `If-None-Match` match → `304 Not Modified`.

**Cache-Control:** hashed assets → `public, max-age=31536000, immutable` | common → `public, max-age=120`

**Gzip priority:** pre-compressed `.gz` → on-the-fly (ZLIB) → no compression.

---

## 11. Auto Pre-compression at Build

During `cmake --build build`, target `mojoraw_static_gzip` recursively processes `src/static`, compressing text extensions (`.html`, `.css`, `.js`, `.json`, `.svg`, `.txt`, `.xml`, `.csv`). Only recompresses when needed.

Internal command: `gzip -n -k -f -9 <file>`

---

## 12. Template Engine — MojoView (`.mj.html`)

```mjhtml
{{ name }}                          {{# escaped #}}
{{ name | upper | trim }}           {{# with filters #}}
{{{ trusted_html }}}                {{# raw output #}}

{% if premium == "true" %}...{% elseif tier == "enterprise" %}...{% else %}...{% endif %}

{% for item in plans %}
    {{ item.name }} - {{ item.price }}
{% endfor %}

{% include "partials/plan-item.mj.html" %}

{% extends "layout.mj.html" %}
{% block content %}...{% endblock %}

{# this comment won't appear in the HTML #}
```

**Filters:** `upper`, `lower`, `trim`, `escape`

Template content is cached per file with `mtime`-based invalidation, protected by mutex.

---

## 13. Quick Tests with curl

```bash
curl -i http://localhost:8080/hello
curl -i "http://localhost:8080/order?name=henriques"
curl -i -H "Cookie: sessionId=abc123" http://localhost:8080/users/42
curl -i -X POST http://localhost:8080/echo -H "Content-Type: application/json" -d '{"ok":true}'
curl -i "http://localhost:8080/view?name=Henriques&premium=true&tier=enterprise"
curl -i "http://localhost:8080/home?name=Dev&logged_in=true"
curl -I -H "Accept-Encoding: gzip" http://localhost:8080/static/index.html

# 304 test
etag=$(curl -sI http://localhost:8080/static/index.html | grep -i ETag | cut -d' ' -f2 | tr -d '\r')
curl -i -H "If-None-Match: $etag" http://localhost:8080/static/index.html
```

---

## 14. Security & Best Practices

**Already implemented:** path traversal blocking, payload limit, idle connection timeout, automatic HTML escaping in `{{ ... }}`.

**Recommended for production:** reverse proxy (Nginx/Caddy), TLS at the proxy, rate limiting, structured logging, regression and fuzzing tests on the parser.

---

## 15. Performance

```bash
wrk -t4 -c100 -d10s http://localhost:8080/hello
wrk -t4 -c100 -d10s http://localhost:8080/static/index.html
wrk -t4 -c100 -d10s "http://localhost:8080/view?name=bench&premium=true"
```

---

## 16. Troubleshooting

**ZLIB not found** — `Could NOT find ZLIB`: no impact on pre-compressed `.gz` fallback.

**gzip not found** — `gzip not found; static .gz precompression disabled`: install `gzip` or disable with `-DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF`.

**Template not found** — Run the binary from the repository root where `src/static/templates` exists.

---

## 17. Roadmap

- [ ] Unit tests for parser, router and template engine
- [ ] Structured logging with levels and request-id
- [ ] Latency and throughput metrics
- [ ] Refined HTTP pipelining/backpressure
- [ ] WebSocket support
- [ ] Template hot reload
- [ ] AST-level template cache

---

## 18. Contributing

Any tech enthusiast or curious mind is welcome — exactly the spirit that created this project.

- Open an **issue** for bugs or suggestions
- Send a **PR** with improvements
- Drop a ⭐ if you like the idea

---

## 19. License

MIT License

Copyright (c) 2024 Henriques Ombisa

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 20. Quick Summary

MojoRaw delivers a real foundation for high-performance C++ backend:

- Non-blocking multi-threaded HTTP server
- Efficient static serving with cache and gzip
- Custom SSR template engine
- Build pipeline with automatic pre-compression, production-ready

Built with curiosity, nostalgia, and C++. ❤️
