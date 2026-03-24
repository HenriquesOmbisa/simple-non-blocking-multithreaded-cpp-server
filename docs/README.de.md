# MojoRaw

> Ein non-blocking, multi-threaded HTTP-Server in C++20 — gebaut mit Neugier.

<div align="center">

🌐 **Verfügbar in:**
[🇬🇧 English](../README.md) &nbsp;|&nbsp;
[🇵🇹 Português](./README.pt.md) &nbsp;|&nbsp;
[🇪🇸 Español](./README.es.md) &nbsp;|&nbsp;
[🇫🇷 Français](./README.fr.md) &nbsp;|&nbsp;
[🇨🇳 中文](./README.zh.md)

</div>

---

## Dokumentationsportal (/doc)

Zusätzlich zu diesem README enthält das Projekt ein integriertes Web/Lokal-Guide-Portal im Server:

- `/doc`
- `/doc/{lang}`
- `/doc/{lang}/{section}`

Sprachen: `pt`, `es`, `fr`, `de`, `zh`

Abschnitte: `overview`, `setup`, `architecture`, `routes`, `templates`, `production`

Lokales Beispiel: `http://localhost:8080/doc/de`

---

## Die Geschichte dahinter

C und C++ waren die ersten Sprachen, die ich gelernt habe. Aber lange Zeit hatte ich dieses nagende Gefühl: _„ok, ich kann C++… aber was mache ich damit eigentlich?"_

Damals arbeitete ich bereits professionell mit PHP, Node.js und hatte sogar Desktop-Systeme mit JavaFX und Swing gebaut. Dann hatte ich eines Tages einen ungewöhnlichen Gedanken: **Was, wenn ich C++ für einen Webserver nutze?**

Es war einfach, es war roh — aber es hat funktioniert 😆😆😆

Das Projekt schlummerte seit 2024 auf GitHub. Neulich bekam ich Nostalgie, schaute es mir wieder an und dachte: _„Was, wenn ich das wirklich verbessere?"_. Ich habe wieder recherchiert, vieles neu geschrieben — und hier ist es: was wir **v1.2** nennen können, jetzt mit einem ordentlichen Namen. Das alte `simple-non-blocking-multithreaded-cpp-server` war zu lang und zu generisch. **MojoRaw** passt besser.

Das Projekt ist **open source** und offen für jeden Technikbegeisterten oder Neugierigen wie mich. Klone es, verbessere es, öffne ein Issue oder schicke einen PR — du bist herzlich willkommen.

---

## Was ist MojoRaw?

MojoRaw ist ein HTTP/1.1-Server in C++20 mit Fokus auf Performance, Einfachheit und vollständige Kontrolle über den Netzwerkfluss.

Es kombiniert:

- Event loop mit `epoll` (edge-triggered)
- Echtes Multi-Threading mit `SO_REUSEPORT`
- Manuellen und effizienten HTTP-Parser
- Router mit Path-Params und Wildcard
- Statischer Dateiserver mit HTTP-Cache und Gzip
- Eigene Template-Sprache (`.mj.html`) für SSR

---

## 1. Überblick

### Ziel

Einen modernen HTTP-Server in C++ ohne schwere Frameworks bauen, dabei:

- hohe Runtime-Vorhersagbarkeit
- geringen Overhead
- kleine, klare API
- einfache Weiterentwicklung Richtung Produktion

### Hauptfunktionen

- Non-blocking I/O mit `epoll`
- Thread-Skalierbarkeit mit `SO_REUSEPORT`
- HTTP/1.1 Keep-alive
- Payload-Limit-Schutz (`413 Payload Too Large`)
- Idle-Connection-Timeout
- Regex-Parsing für Request-Line, Headers, Query und Cookies
- Router mit `:param` und `*` (Wildcard)
- ETag + `Cache-Control` + `304 Not Modified`
- Gzip über vorkomprimierte Dateien (`.gz`) und optionale On-the-fly-Kompression
- Automatische Vorkompression statischer Assets beim Build
- `MojoView` Template-Engine mit `if`, `elseif`, `else`, `for`, `include`, `extends`, `block`, Filtern und HTML-Escaping

---

## 2. Projektstruktur

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

## 3. Voraussetzungen

### Erforderlich

- Linux (empfohlen)
- CMake >= 3.16
- C++20-Compiler (`g++` oder `clang++`)
- `make` (oder gleichwertiger Generator)
- `gzip` (empfohlen für Asset-Vorkompression)

### Optional

- `zlib` für On-the-fly-Kompression der Responses zur Laufzeit

---

## 4. Build und Ausführung

```bash
# Build
cmake -S src -B build
cmake --build build

# Starten
./build/mojoraw_example

# Aufräumen
rm -rf build
```

Hört standardmäßig auf `:8080`.

---

## 5. CMake-Konfiguration

### `MOJORAW_PRECOMPRESS_STATIC_GZIP` (Standard: `ON`)

```bash
cmake -S src -B build -DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF
```

### ZLIB

Bei Fund via `find_package(ZLIB)` wird On-the-fly-Kompression aktiviert. Ohne ZLIB funktioniert der Server weiterhin mit `.gz`-Fallback.

---

## 6. Architektur

### 6.1 Event loop pro Worker

Jeder Worker erstellt seinen eigenen `serverFd`, `epollFd` und eine eigene Connection-Map. Ergebnis: kein geteiltes Lock zwischen Workern, geringere Contention bei hohen Verbindungsraten.

### 6.2 Verbindungszyklus

1. `accept4(..., SOCK_NONBLOCK)`
2. Non-blocking Puffer-Lesen
3. Vollständige Anfrage erkennen
4. HTTP-Parsing
5. Router-Dispatch
6. Response-Serialisierung
7. Non-blocking Schreiben
8. Keep-alive oder Verbindung schließen

### 6.3 Schutzmaßnahmen

- `MAX_REQUEST_BYTES = 2MB` → gibt `413` zurück
- Idle-Verbindung > `IDLE_TIMEOUT_MS` → wird geschlossen

---

## 7. HTTP-Parser

**Request line:** `^([A-Z]+)\s+(\S+)\s+(HTTP\/\d\.\d)$`

**Headers:** `^\s*([^:\r\n]+)\s*:\s*(.*?)\s*$` (normalisiert auf lowercase)

**Query string und Cookies:** Regex-Parsing mit URL-Decode-Unterstützung (`%XX` und `+`).

---

## 8. Router

Unterstützt methodenbasiertes Routing (`get`, `post`, `put`, `patch`, `del`), benannte Parameter (`:id`) und Wildcards (`*`). Routen werden bei der Registrierung zu `std::regex` kompiliert — nicht pro Request.

---

## 9. Beispielrouten

| Route                 | Beschreibung                              |
| --------------------- | ----------------------------------------- |
| `GET /hello`          | Einfache Textantwort                      |
| `GET /json`           | JSON-Antwort                              |
| `POST /echo`          | Gibt den empfangenen Body zurück          |
| `GET /users/:id`      | Path-Param + Cookie                       |
| `GET /order?name=...` | Query-Param                               |
| `GET /old`            | 301-Redirect zu `/`                       |
| `GET /view`           | Rendert `page.mj.html`                    |
| `GET /home`           | Template mit `extends`/`block`            |
| `GET /static/*`       | Statische Assets mit ETag, Cache und Gzip |

---

## 10. HTTP-Cache und Gzip

**ETag** basierend auf Dateigröße, mtime und Encoding-Suffix. `If-None-Match` → `304 Not Modified`.

**Cache-Control:** Asset mit Hash → `public, max-age=31536000, immutable` | normal → `public, max-age=120`

**Gzip-Priorität:** Vorkomprimiertes `.gz` → On-the-fly (ZLIB) → keine Kompression.

---

## 11. Automatische Vorkompression beim Build

Das Target `mojoraw_static_gzip` verarbeitet `src/static` rekursiv und komprimiert Text-Erweiterungen. Nur bei Bedarf neu komprimiert.

Interner Befehl: `gzip -n -k -f -9 <Datei>`

---

## 12. Template-Engine — MojoView (`.mj.html`)

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

{# dieser Kommentar erscheint nicht im HTML #}
```

**Filter:** `upper`, `lower`, `trim`, `escape`

---

## 13. Schnelltests mit curl

```bash
curl -i http://localhost:8080/hello
curl -i "http://localhost:8080/order?name=henriques"
curl -i -H "Cookie: sessionId=abc123" http://localhost:8080/users/42
curl -i -X POST http://localhost:8080/echo -H "Content-Type: application/json" -d '{"ok":true}'
curl -i "http://localhost:8080/view?name=Henriques&premium=true&tier=enterprise"
curl -I -H "Accept-Encoding: gzip" http://localhost:8080/static/index.html
```

---

## 14. Sicherheit und Best Practices

**Bereits implementiert:** Path-Traversal-Blockierung, Payload-Limit, Idle-Connection-Timeout, automatisches HTML-Escaping in `{{ ... }}`.

**Für Produktion:** Reverse Proxy (Nginx/Caddy), TLS, Rate Limiting, strukturiertes Logging, Regressions- und Fuzzing-Tests.

---

## 15. Performance

```bash
wrk -t4 -c100 -d10s http://localhost:8080/hello
wrk -t4 -c100 -d10s http://localhost:8080/static/index.html
wrk -t4 -c100 -d10s "http://localhost:8080/view?name=bench&premium=true"
```

---

## 16. Troubleshooting

**ZLIB nicht gefunden** — kein Einfluss auf den `.gz`-Fallback.

**gzip nicht gefunden** — installiere `gzip` oder deaktiviere mit `-DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF`.

**Template nicht gefunden** — Binary vom Repository-Root aus starten.

---

## 17. Roadmap

- [ ] Unit-Tests für Parser, Router und Template-Engine
- [ ] Strukturiertes Logging mit Levels und Request-ID
- [ ] Latenz- und Throughput-Metriken
- [ ] Verfeinertes HTTP-Pipelining/Backpressure
- [ ] WebSocket-Unterstützung
- [ ] Template Hot-Reload
- [ ] AST-basierter Template-Cache

---

## 18. Mitmachen

Jeder Technikbegeisterte oder Neugierige ist willkommen — genau der Geist, aus dem dieses Projekt entstanden ist.

- Öffne ein **Issue** für Bugs oder Vorschläge
- Schicke einen **PR** mit Verbesserungen
- Gib einen ⭐ wenn dir die Idee gefällt

---

## 19. Lizenz

MIT License — Copyright (c) 2024 Henriques Ombisa

---

Gebaut mit Neugier, Nostalgie und C++. ❤️
