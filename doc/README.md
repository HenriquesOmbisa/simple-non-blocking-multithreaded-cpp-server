# MojoRaw Docs Local Guide

This folder is the local authoring entrypoint for the project guide.

## Web guide routes

The web guide is served from `src/static/doc` through HTTP routes:

- `/doc`
- `/doc/{lang}`
- `/doc/{lang}/{section}`

Supported languages:

- `pt`
- `es`
- `fr`
- `de`
- `zh`

Supported sections:

- `overview`
- `setup`
- `architecture`
- `routes`
- `templates`
- `production`

## Source of truth

Main docs source files in markdown:

- `README.md`
- `docs/README.pt.md`
- `docs/README.es.md`
- `docs/README.fr.md`
- `docs/README.de.md`
- `docs/README.zh.md`

## Regeneration note

When README or language docs change, update the web pages under `src/static/doc`.
