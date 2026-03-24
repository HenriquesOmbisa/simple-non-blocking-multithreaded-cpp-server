# MojoRaw

> Um servidor HTTP non-blocking, multi-threaded em C++20 — feito com curiosidade.

<div align="center">

🌐 **Disponível em:**
[🇬🇧 English](../README.md) &nbsp;|&nbsp;
[🇪🇸 Español](./README.es.md) &nbsp;|&nbsp;
[🇫🇷 Français](./README.fr.md) &nbsp;|&nbsp;
[🇨🇳 中文](./README.zh.md) &nbsp;|&nbsp;
[🇩🇪 Deutsch](./README.de.md)

</div>

---

## Portal de documentação (/doc)

Além deste README, o projeto possui um guia web/local integrado no servidor:

- `/doc`
- `/doc/{lang}`
- `/doc/{lang}/{section}`

Idiomas: `pt`, `es`, `fr`, `de`, `zh`

Seções: `overview`, `setup`, `architecture`, `routes`, `templates`, `production`

Exemplo local: `http://localhost:8080/doc/pt`

---

## A história por trás disso

C e C++ foram as primeiras linguagens que aprendi. Mas por muito tempo ficou aquela inquietação: _"ok, eu sei C++… mas o que eu faço com isso na prática?"_

Na época já trabalhava profissionalmente com PHP, Node.js e até tinha sistemas desktop feitos com JavaFX e Swing. Então um dia tive um pensamento inusitado: **e se eu usar C++ pra fazer um servidor web?**

Foi básico, foi tosco, mas funcionou 😆😆😆

Esse projeto ficou parado no GitHub desde 2024. Recentemente bati uma saudade, dei uma olhada e pensei: _"que tal melhorar isso de verdade?"_. Voltei a pesquisar, reescrevi bastante coisa e aqui está — o que podemos chamar de **v1.2**, agora com um nome decente. O antigo `simple-non-blocking-multithreaded-cpp-server` era comprido demais e genérico demais. **MojoRaw** é mais assim.

O projeto é **open source** e aberto para qualquer entusiasta ou curioso de tecnologia como eu. Pode clonar, melhorar, abrir issue ou mandar PR — vai ser bem-vindo.

---

## O que é o MojoRaw

MojoRaw é um servidor HTTP/1.1 em C++20 focado em performance, simplicidade e controle total do fluxo de rede.

Ele combina:

- Event loop com `epoll` (edge-triggered)
- Multi-thread real com `SO_REUSEPORT`
- Parser HTTP manual e eficiente
- Router com path params e wildcard
- Servidor de arquivos estáticos com cache HTTP e gzip
- Linguagem de template própria (`.mj.html`) para SSR

---

## 1. Visão Geral

### Objetivo

Construir um servidor HTTP moderno em C++ sem depender de frameworks pesados, mantendo:

- alta previsibilidade de runtime
- baixo overhead
- API pequena e clara
- facilidade para evoluir para produção

### Principais recursos

- Non-blocking I/O com `epoll`
- Escalabilidade por thread com `SO_REUSEPORT` (cada worker aceita conexões)
- HTTP/1.1 Keep-alive
- Limite de payload para proteção (`413 Payload Too Large`)
- Timeout de conexão ociosa
- Parsing por regex para request-line, headers, query e cookies
- Router com `:param` e `*` (wildcard)
- ETag + `Cache-Control` + `304 Not Modified`
- Gzip por arquivo precomprimido (`.gz`) e compressão on-the-fly opcional
- Precompressão automática de assets estáticos no build
- Template engine `MojoView` com `if`, `elseif`, `else`, `for`, `include`, `extends`, `block`, filtros e escape HTML

---

## 2. Estrutura do Projeto

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

### Obrigatórios

- Linux (recomendado)
- CMake >= 3.16
- Compilador C++20 (`g++` ou `clang++`)
- `make` (ou gerador equivalente)
- `gzip` (recomendado para precompressão de assets)

### Opcionais

- `zlib` para compressão on-the-fly de respostas em runtime

---

## 4. Build e Execução

```bash
# Build
cmake -S src -B build
cmake --build build

# Rodar
./build/mojoraw_example

# Limpar
rm -rf build
```

Por padrão escuta em `:8080`.

---

## 5. Configuração CMake

### `MOJORAW_PRECOMPRESS_STATIC_GZIP` (default: `ON`)

```bash
cmake -S src -B build -DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF
```

### ZLIB

Se encontrado via `find_package(ZLIB)`, ativa compressão on-the-fly. Sem ZLIB, o servidor continua funcional via fallback `.gz`.

---

## 6. Arquitetura

### 6.1 Event loop por worker

Cada worker cria seu próprio `serverFd`, `epollFd` e mapa de conexões. Resultado: sem lock compartilhado, menor contenção em altas taxas de conexão.

### 6.2 Ciclo de conexão

1. `accept4(..., SOCK_NONBLOCK)`
2. Leitura non-blocking em buffer
3. Detecta request completo
4. Parse do HTTP
5. Dispatch no router
6. Serialização de resposta
7. Escrita non-blocking
8. Keep-alive ou fecha conexão

### 6.3 Proteções

- `MAX_REQUEST_BYTES = 2MB` → retorna `413`
- Conexão ociosa > `IDLE_TIMEOUT_MS` → encerrada

---

## 7. Parser HTTP

**Request line:** `^([A-Z]+)\s+(\S+)\s+(HTTP\/\d\.\d)$`

**Headers:** `^\s*([^:\r\n]+)\s*:\s*(.*?)\s*$` (normalizados para lowercase)

**Query string e Cookies:** Parse por regex com URL decode (`%XX` e `+`).

---

## 8. Router

Suporta roteamento por método (`get`, `post`, `put`, `patch`, `del`), parâmetros nomeados (`:id`) e wildcard (`*`). Rotas compiladas para `std::regex` no registro — não por request.

---

## 9. Rotas do Exemplo

| Rota                  | Descrição                      |
| --------------------- | ------------------------------ |
| `GET /hello`          | Resposta texto simples         |
| `GET /json`           | Resposta JSON                  |
| `POST /echo`          | Retorna o body recebido        |
| `GET /users/:id`      | Path param + cookie            |
| `GET /order?name=...` | Query param                    |
| `GET /old`            | Redirect 301 para `/`          |
| `GET /view`           | Template `page.mj.html`        |
| `GET /home`           | Template com `extends`/`block` |
| `GET /static/*`       | Assets com ETag, cache e gzip  |

---

## 10. Cache HTTP e Gzip

**ETag** baseada em tamanho, mtime e sufixo de encoding. `If-None-Match` → `304 Not Modified`.

**Cache-Control:** asset com hash → `public, max-age=31536000, immutable` | comum → `public, max-age=120`

**Prioridade gzip:** `.gz` pré-gerado → on-the-fly (ZLIB) → sem compressão.

---

## 11. Precompressão Automática no Build

Target `mojoraw_static_gzip` processa recursivamente `src/static`, comprimindo extensões textuais. Só recomprime quando necessário.

Comando interno: `gzip -n -k -f -9 <arquivo>`

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

{# comentário não aparece no HTML #}
```

**Filtros:** `upper`, `lower`, `trim`, `escape`

---

## 13. Testes Rápidos com curl

```bash
curl -i http://localhost:8080/hello
curl -i "http://localhost:8080/order?name=henriques"
curl -i -H "Cookie: sessionId=abc123" http://localhost:8080/users/42
curl -i -X POST http://localhost:8080/echo -H "Content-Type: application/json" -d '{"ok":true}'
curl -i "http://localhost:8080/view?name=Henriques&premium=true&tier=enterprise"
curl -i "http://localhost:8080/home?name=Dev&logged_in=true"
curl -I -H "Accept-Encoding: gzip" http://localhost:8080/static/index.html

etag=$(curl -sI http://localhost:8080/static/index.html | grep -i ETag | cut -d' ' -f2 | tr -d '\r')
curl -i -H "If-None-Match: $etag" http://localhost:8080/static/index.html
```

---

## 14. Segurança e Boas Práticas

**Já implementado:** bloqueio de path traversal, limite de payload, timeout de conexão ociosa, escape HTML automático em `{{ ... }}`.

**Para produção:** reverse proxy (Nginx/Caddy), TLS, rate limiting, logs estruturados, testes de regressão e fuzzing.

---

## 15. Performance

```bash
wrk -t4 -c100 -d10s http://localhost:8080/hello
wrk -t4 -c100 -d10s http://localhost:8080/static/index.html
wrk -t4 -c100 -d10s "http://localhost:8080/view?name=bench&premium=true"
```

---

## 16. Troubleshooting

**ZLIB não encontrado** — sem impacto no fallback `.gz`.

**gzip não encontrado** — instale `gzip` ou desative com `-DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF`.

**Template não encontrado** — execute o binário a partir da raiz do repositório.

---

## 17. Roadmap

- [ ] Testes unitários para parser, router e template engine
- [ ] Logs estruturados com níveis e request-id
- [ ] Métricas de latência e throughput
- [ ] HTTP pipelining/backpressure refinado
- [ ] Suporte a WebSocket
- [ ] Hot reload de templates
- [ ] Cache de template por AST

---

## 18. Contribuindo

Qualquer entusiasta ou curioso de tecnologia é bem-vindo — exatamente o espírito que deu origem a esse projeto.

- Abra uma **issue** para bugs ou sugestões
- Mande um **PR** com melhorias
- Dê uma ⭐ se curtiu a ideia

---

## 19. Licença

MIT License — Copyright (c) 2024 Henriques Ombisa

---

Feito com curiosidade, saudade e C++. ❤️
