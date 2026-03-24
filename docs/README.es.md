# MojoRaw

> Un servidor HTTP non-blocking, multi-threaded en C++20 — construido con curiosidad.

<div align="center">

🌐 **Disponible en:**
[🇬🇧 English](../README.md) &nbsp;|&nbsp;
[🇵🇹 Português](./README.pt.md) &nbsp;|&nbsp;
[🇫🇷 Français](./README.fr.md) &nbsp;|&nbsp;
[🇨🇳 中文](./README.zh.md) &nbsp;|&nbsp;
[🇩🇪 Deutsch](./README.de.md)

</div>

---

## Portal de documentación (/doc)

Además de este README, el proyecto incluye una guía web/local integrada en el servidor:

- `/doc`
- `/doc/{lang}`
- `/doc/{lang}/{section}`

Idiomas: `pt`, `es`, `fr`, `de`, `zh`

Secciones: `overview`, `setup`, `architecture`, `routes`, `templates`, `production`

Ejemplo local: `http://localhost:8080/doc/es`

---

## La historia detrás de esto

C y C++ fueron los primeros lenguajes que aprendí. Pero por mucho tiempo tuve esa inquietud: _"ok, sé C++… ¿pero qué hago con eso en la práctica?"_

En esa época ya trabajaba profesionalmente con PHP, Node.js y hasta tenía sistemas desktop hechos con JavaFX y Swing. Entonces un día tuve un pensamiento inusual: **¿y si uso C++ para hacer un servidor web?**

Fue básico, fue tosco — pero funcionó 😆😆😆

El proyecto estuvo parado en GitHub desde 2024. Recientemente lo revisé con nostalgia y pensé: _"¿qué tal si lo mejoro de verdad?"_. Volví a investigar, reescribí bastante y aquí está — lo que podemos llamar **v1.2**, ahora con un nombre decente. El antiguo `simple-non-blocking-multithreaded-cpp-server` era demasiado largo y genérico. **MojoRaw** es más así.

El proyecto es **open source** y abierto para cualquier entusiasta o curioso de la tecnología como yo. Clónalo, mejóralo, abre un issue o manda un PR — eres bienvenido.

---

## ¿Qué es MojoRaw?

MojoRaw es un servidor HTTP/1.1 en C++20 enfocado en rendimiento, simplicidad y control total del flujo de red.

Combina:

- Event loop con `epoll` (edge-triggered)
- Multi-threading real con `SO_REUSEPORT`
- Parser HTTP manual y eficiente
- Router con path params y wildcard
- Servidor de archivos estáticos con caché HTTP y gzip
- Lenguaje de plantillas propio (`.mj.html`) para SSR

---

## 1. Visión General

### Objetivo

Construir un servidor HTTP moderno en C++ sin depender de frameworks pesados, manteniendo:

- alta previsibilidad de runtime
- bajo overhead
- API pequeña y clara
- facilidad para evolucionar a producción

### Características principales

- I/O non-blocking con `epoll`
- Escalabilidad por hilo con `SO_REUSEPORT`
- HTTP/1.1 Keep-alive
- Límite de payload (`413 Payload Too Large`)
- Timeout de conexión inactiva
- Parsing por regex para request-line, headers, query y cookies
- Router con `:param` y `*` (wildcard)
- ETag + `Cache-Control` + `304 Not Modified`
- Gzip con archivos precomprimidos (`.gz`) y compresión on-the-fly opcional
- Precompresión automática de assets estáticos en el build
- Template engine `MojoView` con `if`, `elseif`, `else`, `for`, `include`, `extends`, `block`, filtros y escape HTML

---

## 2. Estructura del Proyecto

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

## 3. Requisitos

### Obligatorios

- Linux (recomendado)
- CMake >= 3.16
- Compilador C++20 (`g++` o `clang++`)
- `make` (o generador equivalente)
- `gzip` (recomendado para precompresión de assets)

### Opcionales

- `zlib` para compresión on-the-fly en runtime

---

## 4. Build y Ejecución

```bash
# Build
cmake -S src -B build
cmake --build build

# Ejecutar
./build/mojoraw_example

# Limpiar
rm -rf build
```

Escucha en `:8080` por defecto.

---

## 5. Configuración CMake

### `MOJORAW_PRECOMPRESS_STATIC_GZIP` (default: `ON`)

```bash
cmake -S src -B build -DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF
```

### ZLIB

Si se encuentra via `find_package(ZLIB)`, activa compresión on-the-fly. Sin ZLIB, el servidor sigue funcionando con fallback `.gz`.

---

## 6. Arquitectura

### 6.1 Event loop por worker

Cada worker crea su propio `serverFd`, `epollFd` y mapa de conexiones. Resultado: sin lock compartido entre workers, menor contención bajo altas tasas de conexión.

### 6.2 Ciclo de conexión

1. `accept4(..., SOCK_NONBLOCK)`
2. Lectura non-blocking en buffer
3. Detecta request completo
4. Parse del HTTP
5. Dispatch en el router
6. Serialización de respuesta
7. Escritura non-blocking
8. Keep-alive o cierre de conexión

### 6.3 Protecciones

- `MAX_REQUEST_BYTES = 2MB` → retorna `413`
- Conexión inactiva > `IDLE_TIMEOUT_MS` → cerrada

---

## 7. Parser HTTP

**Request line:** `^([A-Z]+)\s+(\S+)\s+(HTTP\/\d\.\d)$`

**Headers:** `^\s*([^:\r\n]+)\s*:\s*(.*?)\s*$` (normalizados a lowercase)

**Query string y Cookies:** Parseados por regex con URL decode (`%XX` y `+`).

---

## 8. Router

Soporta routing por método (`get`, `post`, `put`, `patch`, `del`), parámetros nombrados (`:id`) y wildcard (`*`). Las rutas se compilan a `std::regex` en el registro — no por request.

---

## 9. Rutas de Ejemplo

| Ruta                  | Descripción                    |
| --------------------- | ------------------------------ |
| `GET /hello`          | Respuesta texto simple         |
| `GET /json`           | Respuesta JSON                 |
| `POST /echo`          | Devuelve el body recibido      |
| `GET /users/:id`      | Path param + cookie            |
| `GET /order?name=...` | Query param                    |
| `GET /old`            | Redirect 301 a `/`             |
| `GET /view`           | Renderiza `page.mj.html`       |
| `GET /home`           | Template con `extends`/`block` |
| `GET /static/*`       | Assets con ETag, caché y gzip  |

---

## 10. Caché HTTP y Gzip

**ETag** basada en tamaño, mtime y sufijo de encoding. `If-None-Match` → `304 Not Modified`.

**Cache-Control:** asset con hash → `public, max-age=31536000, immutable` | común → `public, max-age=120`

**Prioridad gzip:** `.gz` pre-generado → on-the-fly (ZLIB) → sin compresión.

---

## 11. Precompresión Automática en el Build

Target `mojoraw_static_gzip` procesa recursivamente `src/static`, comprimiendo extensiones textuales. Solo recomprime cuando es necesario.

Comando interno: `gzip -n -k -f -9 <archivo>`

---

## 12. Template Engine — MojoView (`.mj.html`)

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

{# este comentario no aparece en el HTML #}
```

**Filtros:** `upper`, `lower`, `trim`, `escape`

---

## 13. Tests Rápidos con curl

```bash
curl -i http://localhost:8080/hello
curl -i "http://localhost:8080/order?name=henriques"
curl -i -H "Cookie: sessionId=abc123" http://localhost:8080/users/42
curl -i -X POST http://localhost:8080/echo -H "Content-Type: application/json" -d '{"ok":true}'
curl -i "http://localhost:8080/view?name=Henriques&premium=true&tier=enterprise"
curl -I -H "Accept-Encoding: gzip" http://localhost:8080/static/index.html
```

---

## 14. Seguridad y Buenas Prácticas

**Ya implementado:** bloqueo de path traversal, límite de payload, timeout de conexión inactiva, escape HTML automático en `{{ ... }}`.

**Para producción:** reverse proxy (Nginx/Caddy), TLS, rate limiting, logging estructurado, tests de regresión y fuzzing.

---

## 15. Rendimiento

```bash
wrk -t4 -c100 -d10s http://localhost:8080/hello
wrk -t4 -c100 -d10s http://localhost:8080/static/index.html
wrk -t4 -c100 -d10s "http://localhost:8080/view?name=bench&premium=true"
```

---

## 16. Troubleshooting

**ZLIB no encontrado** — sin impacto en el fallback `.gz`.

**gzip no encontrado** — instala `gzip` o desactiva con `-DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF`.

**Template no encontrado** — ejecuta el binario desde la raíz del repositorio.

---

## 17. Roadmap

- [ ] Tests unitarios para parser, router y template engine
- [ ] Logging estructurado con niveles y request-id
- [ ] Métricas de latencia y throughput
- [ ] HTTP pipelining/backpressure refinado
- [ ] Soporte WebSocket
- [ ] Hot reload de templates
- [ ] Caché de template por AST

---

## 18. Contribuir

Cualquier entusiasta o curioso de la tecnología es bienvenido — exactamente el espíritu que dio origen a este proyecto.

- Abre un **issue** para bugs o sugerencias
- Manda un **PR** con mejoras
- Deja una ⭐ si te gustó la idea

---

## 19. Licencia

MIT License — Copyright (c) 2024 Henriques Ombisa

---

Hecho con curiosidad, nostalgia y C++. ❤️
