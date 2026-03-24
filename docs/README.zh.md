# MojoRaw

> 一个用 C++20 编写的非阻塞、多线程 HTTP 服务器——以好奇心为驱动。

<div align="center">

🌐 **其他语言版本：**
[🇬🇧 English](../README.md) &nbsp;|&nbsp;
[🇵🇹 Português](./README.pt.md) &nbsp;|&nbsp;
[🇪🇸 Español](./README.es.md) &nbsp;|&nbsp;
[🇫🇷 Français](./README.fr.md) &nbsp;|&nbsp;
[🇩🇪 Deutsch](./README.de.md)

</div>

---

## 文档门户 (/doc)

除了这个 README，项目还提供了由服务器直接提供的本地/网页文档门户：

- `/doc`
- `/doc/{lang}`
- `/doc/{lang}/{section}`

语言：`pt`, `es`, `fr`, `de`, `zh`

章节：`overview`, `setup`, `architecture`, `routes`, `templates`, `production`

本地示例：`http://localhost:8080/doc/zh`

---

## 项目背后的故事

C 和 C++ 是我学习的第一批编程语言。但很长一段时间里，我心中始终有一个困惑：_"好吧，我会 C++……但我能用它做什么呢？"_

那时我已经在专业工作中使用 PHP、Node.js，甚至用 JavaFX 和 Swing 开发过桌面应用系统。某天，我突发奇想：**如果用 C++ 来做一个 Web 服务器会怎样？**

很基础，很粗糙——但它跑起来了 😆😆😆

这个项目从 2024 年起就搁置在 GitHub 上。最近，带着一丝怀旧之情，我重新打开了它，心想：_"干脆认真改进一下吧？"_。于是重新研究、大量重写，这就是现在的版本——我们姑且称之为 **v1.2**，终于有了一个像样的名字。旧名字 `simple-non-blocking-multithreaded-cpp-server` 太长也太泛。**MojoRaw** 更贴切。

本项目是**开源**的，欢迎所有对技术充满热情和好奇的朋友参与。你可以克隆、改进、提 issue 或发 PR——非常欢迎。

---

## MojoRaw 是什么？

MojoRaw 是一个基于 C++20 的 HTTP/1.1 服务器，专注于性能、简洁性以及对网络流的完全掌控。

它结合了：

- 使用 `epoll`（边缘触发）的事件循环
- 基于 `SO_REUSEPORT` 的真正多线程
- 手动实现的高效 HTTP 解析器
- 支持路径参数和通配符的路由器
- 带 HTTP 缓存和 gzip 的静态文件服务器
- 自研模板语言（`.mj.html`）用于服务端渲染（SSR）

---

## 1. 概览

### 目标

在不依赖重型框架的前提下，构建一个现代 C++ HTTP 服务器，保持：

- 高度可预测的运行时行为
- 低开销
- 小而清晰的 API
- 易于演进至生产环境

### 主要特性

- 使用 `epoll` 的非阻塞 I/O
- 使用 `SO_REUSEPORT` 的每线程可扩展性
- HTTP/1.1 Keep-alive
- 负载限制保护（`413 Payload Too Large`）
- 空闲连接超时
- 请求行、请求头、查询参数、Cookie 的正则解析
- 支持 `:param` 和 `*`（通配符）的路由器
- ETag + `Cache-Control` + `304 Not Modified`
- 基于预压缩文件（`.gz`）的 Gzip 及可选的即时压缩
- 构建时自动预压缩静态资源
- `MojoView` 模板引擎，支持 `if`、`elseif`、`else`、`for`、`include`、`extends`、`block`、过滤器和 HTML 转义

---

## 2. 项目结构

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

## 3. 环境要求

### 必须

- Linux（推荐）
- CMake >= 3.16
- C++20 编译器（`g++` 或 `clang++`）
- `make`（或等效构建工具）
- `gzip`（推荐，用于资源预压缩）

### 可选

- `zlib`，用于运行时即时压缩响应

---

## 4. 构建与运行

```bash
# 构建
cmake -S src -B build
cmake --build build

# 运行
./build/mojoraw_example

# 清理
rm -rf build
```

默认监听 `:8080`。

---

## 5. CMake 配置

### `MOJORAW_PRECOMPRESS_STATIC_GZIP`（默认：`ON`）

```bash
cmake -S src -B build -DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF
```

### ZLIB

通过 `find_package(ZLIB)` 找到时，启用即时压缩。没有 ZLIB，服务器仍可通过预压缩 `.gz` 文件正常运行。

---

## 6. 架构

### 6.1 每个 Worker 的事件循环

每个 worker 拥有独立的 `serverFd`、`epollFd` 和连接映射表。结果：worker 间无共享锁，在高连接速率下竞争更少。

### 6.2 连接生命周期

1. `accept4(..., SOCK_NONBLOCK)`
2. 非阻塞缓冲区读取
3. 检测完整请求
4. HTTP 解析
5. 路由分发
6. 响应序列化
7. 非阻塞写入
8. Keep-alive 或关闭连接

### 6.3 保护机制

- `MAX_REQUEST_BYTES = 2MB` → 返回 `413`
- 空闲连接超过 `IDLE_TIMEOUT_MS` → 关闭

---

## 7. HTTP 解析器

**请求行：** `^([A-Z]+)\s+(\S+)\s+(HTTP\/\d\.\d)$`

**请求头：** `^\s*([^:\r\n]+)\s*:\s*(.*?)\s*$`（键名规范化为小写）

**查询参数与 Cookie：** 正则解析，支持 URL 解码（`%XX` 和 `+`）。

---

## 8. 路由器

支持按方法路由（`get`、`post`、`put`、`patch`、`del`）、命名参数（`:id`）和通配符（`*`）。路由在注册时编译为 `std::regex`，而非每次请求时编译。

---

## 9. 示例路由

| 路由                  | 描述                            |
| --------------------- | ------------------------------- |
| `GET /hello`          | 纯文本响应                      |
| `GET /json`           | JSON 响应                       |
| `POST /echo`          | 返回接收到的 body               |
| `GET /users/:id`      | 路径参数 + Cookie               |
| `GET /order?name=...` | 查询参数                        |
| `GET /old`            | 301 跳转到 `/`                  |
| `GET /view`           | 渲染 `page.mj.html`             |
| `GET /home`           | 使用 `extends`/`block` 的模板   |
| `GET /static/*`       | 带 ETag、缓存和 gzip 的静态资源 |

---

## 10. HTTP 缓存与 Gzip

**ETag** 基于文件大小、mtime 和编码后缀。`If-None-Match` 匹配 → `304 Not Modified`。

**Cache-Control：** 含哈希的资源 → `public, max-age=31536000, immutable` | 普通 → `public, max-age=120`

**Gzip 优先级：** 预压缩 `.gz` → 即时压缩（ZLIB）→ 不压缩。

---

## 11. 构建时自动预压缩

`mojoraw_static_gzip` 目标递归处理 `src/static`，压缩文本类型扩展名。仅在必要时重新压缩。

内部命令：`gzip -n -k -f -9 <文件>`

---

## 12. 模板引擎 — MojoView（`.mj.html`）

```mjhtml
{{ name }}
{{ name | upper | trim }}
{{{ trusted_html }}}

{% if premium == "true" %}...{% elseif tier == "enterprise" %}...{% else %}...{% endif %}

{% for item in plans %}
    {{ item.name }} - {{ item.price }}
{% endfor %}

{% include "partials/plan-item.mj.html" %}
{% extends "layout.mj.html" %}
{% block content %}...{% endblock %}

{# 此注释不会出现在 HTML 中 #}
```

**过滤器：** `upper`、`lower`、`trim`、`escape`

---

## 13. 使用 curl 快速测试

```bash
curl -i http://localhost:8080/hello
curl -i "http://localhost:8080/order?name=henriques"
curl -i -H "Cookie: sessionId=abc123" http://localhost:8080/users/42
curl -i -X POST http://localhost:8080/echo -H "Content-Type: application/json" -d '{"ok":true}'
curl -i "http://localhost:8080/view?name=Henriques&premium=true&tier=enterprise"
curl -I -H "Accept-Encoding: gzip" http://localhost:8080/static/index.html
```

---

## 14. 安全与最佳实践

**已实现：** 路径遍历拦截、payload 限制、空闲连接超时、`{{ ... }}` 中的自动 HTML 转义。

**生产环境建议：** 反向代理（Nginx/Caddy）、TLS、限流、结构化日志、回归测试与模糊测试。

---

## 15. 性能测试

```bash
wrk -t4 -c100 -d10s http://localhost:8080/hello
wrk -t4 -c100 -d10s http://localhost:8080/static/index.html
wrk -t4 -c100 -d10s "http://localhost:8080/view?name=bench&premium=true"
```

---

## 16. 故障排查

**找不到 ZLIB** — 不影响 `.gz` 文件的 fallback 功能。

**找不到 gzip** — 安装 `gzip`，或使用 `-DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF` 禁用。

**找不到模板** — 从仓库根目录运行二进制文件。

---

## 17. 路线图

- [ ] 为解析器、路由器和模板引擎编写单元测试
- [ ] 带级别和 request-id 的结构化日志
- [ ] 延迟和吞吐量指标
- [ ] 完善 HTTP pipelining/backpressure
- [ ] WebSocket 支持
- [ ] 模板热重载
- [ ] AST 级别的模板缓存

---

## 18. 参与贡献

欢迎所有技术爱好者和好奇者——这正是这个项目诞生的精神所在。

- 提 **issue** 反馈 bug 或建议
- 发 **PR** 带来改进
- 如果你喜欢这个想法，点个 ⭐

---

## 19. 许可证

MIT License — Copyright (c) 2024 Henriques Ombisa

---

用好奇心、怀旧与 C++ 构建。❤️
