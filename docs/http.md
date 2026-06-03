# HTTP Service Usage

The HTTP service target is `skill_rating_http`. It uses `cpp-httplib` and exposes
the same operations as the CLI using JSON request and response bodies.

```sh
./build/skill_rating_http --host 127.0.0.1 --port 8080 --workers 8
```

The service is intended for local use, usually behind the application that owns
match storage, queueing, retries, and other orchestration. By default it uses
one HTTP worker per detected CPU core, falling back to 4 workers when the CPU
count is unavailable. Use `--workers N` to override that.

## Routes

- `GET /health`
- `POST /rate`
- `POST /rate-1vs1`
- `POST /quality`
- `POST /quality-1vs1`
- `POST /expose`
- `POST /draw-probability`

Request bodies use the same JSON shapes as the CLI. See [CLI usage](cli.md) for
full examples. Responses are JSON objects. Validation errors return `400` with
an `error` field, unknown routes return `404`, and internal calculation failures
return `500`.

HTTP requests may also include an optional string `request_id`. When present,
the service echoes it in success and error responses so callers can correlate
and reorder concurrent responses without requiring this service to store job
state.

Any POST request may include an optional `environment` object. When provided, it
must include all five fields: `mu`, `sigma`, `beta`, `tau`, and
`draw_probability`.

## Health Check

```sh
curl http://127.0.0.1:8080/health
```

Response:

```json
{"status":"ok"}
```

## Match Quality

`/quality` accepts `rating_groups`, optional `weights`, and optional
`environment`. It supports one-vs-one, team-vs-team, and free-for-all match
quality.

```sh
curl -sS -H 'Content-Type: application/json' \
  -d '{"request_id":"quality-1","rating_groups":[[{"mu":25,"sigma":8.333333333333}],[{"mu":25,"sigma":8.333333333333}]]}' \
  http://127.0.0.1:8080/quality
```

Response:

```json
{"quality":0.4472135954999723,"request_id":"quality-1"}
```

## 1v1 Rating

`/rate-1vs1` accepts `first_player`, `second_player`, optional `drawn`, optional
`min_delta`, and optional `environment`. If `drawn` is omitted or false, the
first player is treated as the winner.

```sh
curl -sS -H 'Content-Type: application/json' \
  -d '{"first_player":{"mu":25,"sigma":8.333333333333},"second_player":{"mu":25,"sigma":8.333333333333}}' \
  http://127.0.0.1:8080/rate-1vs1
```

Response:

```json
{
  "first_player": {"mu": 29.395831693, "sigma": 7.17147580701},
  "second_player": {"mu": 20.604168307, "sigma": 7.17147580701}
}
```

## Draw Probability

`/draw-probability` accepts either `first_player` plus `second_player`, or
exactly two `rating_groups` with optional `weights`. It also accepts optional
`environment` and optional HTTP-only `request_id`.

```sh
curl -sS -H 'Content-Type: application/json' \
  -d '{"request_id":"draw-probability-1","first_player":{"mu":25,"sigma":8.333333333333},"second_player":{"mu":25,"sigma":8.333333333333}}' \
  http://127.0.0.1:8080/draw-probability
```

Response:

```json
{"draw_probability":0.04481549759126091,"request_id":"draw-probability-1"}
```

Team-vs-team request:

```sh
curl -sS -H 'Content-Type: application/json' \
  -d '{"rating_groups":[[{"mu":25,"sigma":8.333333333333},{"mu":27,"sigma":6}],[{"mu":26,"sigma":7}]],"weights":[[1,0.5],[1]]}' \
  http://127.0.0.1:8080/draw-probability
```

Response:

```json
{"draw_probability":0.03507373242772174}
```

The route uses the library's `Environment::draw_probability` method, which
derives the internal draw margin from the environment and combines the players'
means, uncertainties, weights, and beta to estimate the chance that this
specific two-team match ends in a draw.

Free-for-all draw probability is not supported. Rating updates and quality still
support free-for-all matches, but `/draw-probability` is intentionally a
two-team route.

## Custom Environment

This example changes the configured draw probability used to derive the internal
draw margin:

```sh
curl -sS -H 'Content-Type: application/json' \
  -d '{"environment":{"mu":25,"sigma":8.333333333333,"beta":4.166666666667,"tau":0.083333333333,"draw_probability":0.2},"first_player":{"mu":25,"sigma":8.333333333333},"second_player":{"mu":25,"sigma":8.333333333333}}' \
  http://127.0.0.1:8080/draw-probability
```

Response:

```json
{"draw_probability":0.09020749593952493}
```

## Concurrency

- Each request is handled by the `cpp-httplib` thread pool configured through
  `--workers`.
- The rating operations are stateless, so concurrent requests do not share
  mutable rating state inside this service.
- Concurrent responses may complete out of order. Use `request_id` when the
  caller needs correlation, deduplication, or client-side ordering.
- Work queues, background workers, persistence, deduplication, retries, metrics,
  authentication, and TLS termination belong to the application embedding or
  calling this local service.
