# Phase 1 Local API

The service listens on `http://127.0.0.1:8787` by default. All JSON responses use
`Content-Type: application/json; charset=utf-8`. Errors have this shape:

```json
{"error":"human-readable message"}
```

## HTTP Routes

### Health

`GET /api/health`

Returns service version, local-only status, and imported-game count.

### Import

`POST /api/import`

Accepts exactly one import source:

```json
{"url":"https://www.chess.com/game/live/123"}
```

```json
{"pgn":"[Event \"...\"]\n\n1. e4 ..."}
```

Returns HTTP 202 for a new game or 200 for a duplicate. The response contains
`duplicate`, `game_id`, and the queued or existing analysis `job`.

### Games

- `GET /api/games`: list imported games and their analysis status.
- `GET /api/games/{id}`: retrieve tags, all plies/FENs, PGN, and completed analysis.
- `GET /api/games/{id}/moves/{ply}`: retrieve one SAN/UCI move and its before/after FEN.
- `GET /api/games/{id}/analysis`: retrieve analysis or HTTP 202 with `pending`.
- `POST /api/games/{id}/analysis`: enqueue analysis or return the existing job.

### Analysis Views

- `GET /api/mistakes`: return all stored top mistakes across analyzed games.
- `GET /api/settings`: return effective Phase 1 analysis and local binding settings.

### Jobs

- `GET /api/jobs`: list in-memory jobs.
- `GET /api/jobs/{id}`: retrieve one job.
- `DELETE /api/jobs/{id}`: cancel queued or running analysis.

A job has `queued`, `running`, `complete`, `failed`, or `cancelled` status. Progress
has a stage (`parsing`, `shallow_scan`, `deep_analysis`, or `complete`), completed
unit count, total unit count, and message.

## WebSocket

Connect to `ws://127.0.0.1:8787/ws`.

The server first sends a reconnect-safe snapshot:

```json
{"type":"jobs_snapshot","jobs":[]}
```

It then publishes changes:

```json
{"type":"job_update","job":{"id":1,"game_id":"...","status":"running","progress":{}}}
```

Clients should reload `GET /api/games/{id}` after a job reaches `complete` because
the event-log projection is authoritative.

## Status Codes

| Code | Meaning |
| --- | --- |
| 200 | Read succeeded, duplicate import, or cancellation accepted |
| 202 | New import/analysis accepted or requested analysis remains pending |
| 400 | Invalid JSON, URL, PGN, move index, or request shape |
| 404 | Unknown route, game, move, or job |
| 413 | Request exceeds transport limits |
| 500 | Local storage or engine failure |
| 502 | Chess.com retrieval failure |
| 504 | Retrieval or engine timeout |
