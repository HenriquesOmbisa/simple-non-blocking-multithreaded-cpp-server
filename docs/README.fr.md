# MojoRaw

> Un serveur HTTP non-bloquant, multi-threadé en C++20 — construit avec curiosité.

<div align="center">

🌐 **Disponible en :**
[🇬🇧 English](../README.md) &nbsp;|&nbsp;
[🇵🇹 Português](./README.pt.md) &nbsp;|&nbsp;
[🇪🇸 Español](./README.es.md) &nbsp;|&nbsp;
[🇨🇳 中文](./README.zh.md) &nbsp;|&nbsp;
[🇩🇪 Deutsch](./README.de.md)

</div>

---

## Portail de documentation (/doc)

En plus de ce README, le projet inclut un guide web/local directement servi par le serveur :

- `/doc`
- `/doc/{lang}`
- `/doc/{lang}/{section}`

Langues : `pt`, `es`, `fr`, `de`, `zh`

Sections : `overview`, `setup`, `architecture`, `routes`, `templates`, `production`

Exemple local : `http://localhost:8080/doc/fr`

---

## L'histoire derrière ce projet

C et C++ ont été les premiers langages que j'ai appris. Mais pendant longtemps j'avais cette inquiétude : _"ok, je connais C++… mais qu'est-ce que j'en fais concrètement ?"_

À l'époque, je travaillais déjà professionnellement avec PHP, Node.js et j'avais même développé des systèmes desktop avec JavaFX et Swing. Puis un jour j'ai eu une idée inhabituelle : **et si j'utilisais C++ pour faire un serveur web ?**

C'était basique, c'était brut — mais ça a marché 😆😆😆

Le projet est resté sur GitHub depuis 2024. Récemment, pris d'une certaine nostalgie, j'y ai jeté un œil et pensé : _"et si je l'améliorais vraiment ?"_. Je suis retourné à la recherche, j'ai beaucoup réécrit, et voilà — ce qu'on peut appeler la **v1.2**, avec un vrai nom cette fois. L'ancien `simple-non-blocking-multithreaded-cpp-server` était trop long et trop générique. **MojoRaw**, c'est mieux comme ça.

Le projet est **open source** et ouvert à tout passionné ou curieux de technologie comme moi. Clone-le, améliore-le, ouvre une issue ou envoie une PR — tu es le bienvenu.

---

## Qu'est-ce que MojoRaw ?

MojoRaw est un serveur HTTP/1.1 en C++20 axé sur la performance, la simplicité et le contrôle total du flux réseau.

Il combine :

- Event loop avec `epoll` (edge-triggered)
- Multi-threading réel avec `SO_REUSEPORT`
- Parser HTTP manuel et efficace
- Router avec path params et wildcard
- Serveur de fichiers statiques avec cache HTTP et gzip
- Langage de template maison (`.mj.html`) pour le SSR

---

## 1. Vue d'ensemble

### Objectif

Construire un serveur HTTP moderne en C++ sans dépendre de frameworks lourds, tout en maintenant :

- une haute prévisibilité du runtime
- un faible overhead
- une API petite et claire
- une évolution facile vers la production

### Fonctionnalités principales

- I/O non-bloquant avec `epoll`
- Scalabilité par thread avec `SO_REUSEPORT`
- HTTP/1.1 Keep-alive
- Limite de payload (`413 Payload Too Large`)
- Timeout de connexion inactive
- Parsing par regex pour request-line, headers, query et cookies
- Router avec `:param` et `*` (wildcard)
- ETag + `Cache-Control` + `304 Not Modified`
- Gzip via fichiers pré-compressés (`.gz`) et compression on-the-fly optionnelle
- Pré-compression automatique des assets statiques au build
- Template engine `MojoView` avec `if`, `elseif`, `else`, `for`, `include`, `extends`, `block`, filtres et échappement HTML

---

## 2. Structure du Projet

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

## 3. Prérequis

### Obligatoires

- Linux (recommandé)
- CMake >= 3.16
- Compilateur C++20 (`g++` ou `clang++`)
- `make` (ou générateur équivalent)
- `gzip` (recommandé pour la pré-compression des assets)

### Optionnels

- `zlib` pour la compression on-the-fly des réponses en runtime

---

## 4. Build et Exécution

```bash
# Build
cmake -S src -B build
cmake --build build

# Lancer
./build/mojoraw_example

# Nettoyer
rm -rf build
```

Écoute sur `:8080` par défaut.

---

## 5. Configuration CMake

### `MOJORAW_PRECOMPRESS_STATIC_GZIP` (défaut : `ON`)

```bash
cmake -S src -B build -DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF
```

### ZLIB

Si trouvé via `find_package(ZLIB)`, active la compression on-the-fly. Sans ZLIB, le serveur fonctionne avec le fallback `.gz`.

---

## 6. Architecture

### 6.1 Event loop par worker

Chaque worker crée son propre `serverFd`, `epollFd` et map de connexions. Résultat : pas de lock partagé entre workers, moindre contention sous forte charge.

### 6.2 Cycle de connexion

1. `accept4(..., SOCK_NONBLOCK)`
2. Lecture non-bloquante en buffer
3. Détection de la requête complète
4. Parse HTTP
5. Dispatch dans le router
6. Sérialisation de la réponse
7. Écriture non-bloquante
8. Keep-alive ou fermeture de connexion

### 6.3 Protections

- `MAX_REQUEST_BYTES = 2MB` → retourne `413`
- Connexion inactive > `IDLE_TIMEOUT_MS` → fermée

---

## 7. Parser HTTP

**Request line :** `^([A-Z]+)\s+(\S+)\s+(HTTP\/\d\.\d)$`

**Headers :** `^\s*([^:\r\n]+)\s*:\s*(.*?)\s*$` (normalisés en lowercase)

**Query string et Cookies :** Parsés par regex avec URL decode (`%XX` et `+`).

---

## 8. Router

Supporte le routing par méthode (`get`, `post`, `put`, `patch`, `del`), les paramètres nommés (`:id`) et le wildcard (`*`). Les routes sont compilées en `std::regex` à l'enregistrement — pas par requête.

---

## 9. Routes de l'Exemple

| Route                 | Description                     |
| --------------------- | ------------------------------- |
| `GET /hello`          | Réponse texte simple            |
| `GET /json`           | Réponse JSON                    |
| `POST /echo`          | Retourne le body reçu           |
| `GET /users/:id`      | Path param + cookie             |
| `GET /order?name=...` | Query param                     |
| `GET /old`            | Redirect 301 vers `/`           |
| `GET /view`           | Rendu `page.mj.html`            |
| `GET /home`           | Template avec `extends`/`block` |
| `GET /static/*`       | Assets avec ETag, cache et gzip |

---

## 10. Cache HTTP et Gzip

**ETag** basée sur taille, mtime et suffixe d'encodage. `If-None-Match` → `304 Not Modified`.

**Cache-Control :** asset avec hash → `public, max-age=31536000, immutable` | commun → `public, max-age=120`

**Priorité gzip :** `.gz` pré-généré → on-the-fly (ZLIB) → sans compression.

---

## 11. Pré-compression Automatique au Build

La cible `mojoraw_static_gzip` parcourt récursivement `src/static`, comprime les extensions textuelles. Ne recomprime que si nécessaire.

Commande interne : `gzip -n -k -f -9 <fichier>`

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

{# ce commentaire n'apparaît pas dans le HTML #}
```

**Filtres :** `upper`, `lower`, `trim`, `escape`

---

## 13. Tests Rapides avec curl

```bash
curl -i http://localhost:8080/hello
curl -i "http://localhost:8080/order?name=henriques"
curl -i -H "Cookie: sessionId=abc123" http://localhost:8080/users/42
curl -i -X POST http://localhost:8080/echo -H "Content-Type: application/json" -d '{"ok":true}'
curl -i "http://localhost:8080/view?name=Henriques&premium=true&tier=enterprise"
curl -I -H "Accept-Encoding: gzip" http://localhost:8080/static/index.html
```

---

## 14. Sécurité et Bonnes Pratiques

**Déjà implémenté :** blocage du path traversal, limite de payload, timeout de connexion inactive, échappement HTML automatique dans `{{ ... }}`.

**Pour la production :** reverse proxy (Nginx/Caddy), TLS, rate limiting, logging structuré, tests de régression et fuzzing.

---

## 15. Performance

```bash
wrk -t4 -c100 -d10s http://localhost:8080/hello
wrk -t4 -c100 -d10s http://localhost:8080/static/index.html
wrk -t4 -c100 -d10s "http://localhost:8080/view?name=bench&premium=true"
```

---

## 16. Dépannage

**ZLIB non trouvé** — sans impact sur le fallback `.gz`.

**gzip non trouvé** — installe `gzip` ou désactive avec `-DMOJORAW_PRECOMPRESS_STATIC_GZIP=OFF`.

**Template non trouvé** — lance le binaire depuis la racine du dépôt.

---

## 17. Feuille de Route

- [ ] Tests unitaires pour parser, router et template engine
- [ ] Logging structuré avec niveaux et request-id
- [ ] Métriques de latence et throughput
- [ ] HTTP pipelining/backpressure affiné
- [ ] Support WebSocket
- [ ] Hot reload de templates
- [ ] Cache de template par AST

---

## 18. Contribuer

Tout passionné ou curieux de technologie est le bienvenu — exactement l'esprit qui a donné naissance à ce projet.

- Ouvre une **issue** pour des bugs ou suggestions
- Envoie une **PR** avec des améliorations
- Laisse une ⭐ si l'idée te plaît

---

## 19. Licence

MIT License — Copyright (c) 2024 Henriques Ombisa

---

Fait avec curiosité, nostalgie et C++. ❤️
