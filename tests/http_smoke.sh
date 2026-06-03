#!/usr/bin/env sh
set -eu

server="$1"
port="${2:-18080}"

contains() {
    case "$1" in
        *"$2"*) return 0 ;;
        *) echo "expected output to contain '$2', got: $1" >&2; return 1 ;;
    esac
}

"$server" --host 127.0.0.1 --port "$port" --workers 2 >/tmp/skill_rating_http_test.log 2>&1 &
pid="$!"
trap 'kill "$pid" 2>/dev/null || true' EXIT

for _ in 1 2 3 4 5 6 7 8 9 10; do
    if curl -fsS "http://127.0.0.1:$port/health" >/tmp/skill_rating_http_health.json 2>/dev/null; then
        break
    fi
    sleep 0.1
done

health="$(curl -fsS "http://127.0.0.1:$port/health")"
test "$health" = '{"status":"ok"}'

quality="$(curl -fsS \
    -H 'Content-Type: application/json' \
    -d '{"request_id":"quality-1","rating_groups":[[{"mu":25,"sigma":8.333333333333}],[{"mu":25,"sigma":8.333333333333}]]}' \
    "http://127.0.0.1:$port/quality")"
contains "$quality" '"quality":0.4472135954'
contains "$quality" '"request_id":"quality-1"'

rate_1vs1="$(curl -fsS \
    -H 'Content-Type: application/json' \
    -d '{"first_player":{"mu":25,"sigma":8.333333333333},"second_player":{"mu":25,"sigma":8.333333333333}}' \
    "http://127.0.0.1:$port/rate-1vs1")"
contains "$rate_1vs1" '"first_player"'
contains "$rate_1vs1" '29.395831692'
contains "$rate_1vs1" '20.604168307'

quality_1vs1="$(curl -fsS \
    -H 'Content-Type: application/json' \
    -d '{"first_player":{"mu":25,"sigma":8.333333333333},"second_player":{"mu":25,"sigma":8.333333333333}}' \
    "http://127.0.0.1:$port/quality-1vs1")"
contains "$quality_1vs1" '"quality":0.4472135954'

exposure="$(curl -fsS \
    -H 'Content-Type: application/json' \
    -d '{"rating":{"mu":29.396,"sigma":7.171}}' \
    "http://127.0.0.1:$port/expose")"
contains "$exposure" '"exposure":7.882'

draw_probability="$(curl -fsS \
    -H 'Content-Type: application/json' \
    -d '{"request_id":"draw-probability-1","first_player":{"mu":25,"sigma":8.333333333333},"second_player":{"mu":25,"sigma":8.333333333333}}' \
    "http://127.0.0.1:$port/draw-probability")"
contains "$draw_probability" '"draw_probability":0.0448154975'
contains "$draw_probability" '"request_id":"draw-probability-1"'

rate="$(curl -fsS \
    -H 'Content-Type: application/json' \
    -d '{"rating_groups":[[{"mu":32,"sigma":7}],[{"mu":25,"sigma":8.333333333333},{"mu":27,"sigma":6}],[{"mu":20,"sigma":8}]],"ranks":[1,0,2],"weights":[[1],[1,0.5],[1]]}' \
    "http://127.0.0.1:$port/rate")"
contains "$rate" '"rating_groups"'
contains "$rate" '28.391918161'
contains "$rate" '17.815292649'

bad_status="$(curl -sS -o /tmp/skill_rating_http_bad.json -w '%{http_code}' \
    -H 'Content-Type: application/json' \
    -d '{"request_id":"bad-quality","rating_groups":[[{"mu":25,"sigma":8.333333333333}]]}' \
    "http://127.0.0.1:$port/quality")"
test "$bad_status" = "400"
contains "$(cat /tmp/skill_rating_http_bad.json)" 'at least two teams are required'
contains "$(cat /tmp/skill_rating_http_bad.json)" '"request_id":"bad-quality"'

missing_status="$(curl -sS -o /tmp/skill_rating_http_missing.json -w '%{http_code}' \
    "http://127.0.0.1:$port/missing")"
test "$missing_status" = "404"
contains "$(cat /tmp/skill_rating_http_missing.json)" 'unknown route'
