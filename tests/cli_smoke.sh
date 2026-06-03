#!/usr/bin/env sh
set -eu

cli="$1"

contains() {
    case "$1" in
        *"$2"*) return 0 ;;
        *) echo "expected output to contain '$2', got: $1" >&2; return 1 ;;
    esac
}

quality="$("$cli" quality '{"rating_groups":[[{"mu":25,"sigma":8.333333333333}],[{"mu":25,"sigma":8.333333333333}]]}')"
contains "$quality" '"quality":0.4472135954'

rate_1vs1="$("$cli" rate-1vs1 '{"first_player":{"mu":25,"sigma":8.333333333333},"second_player":{"mu":25,"sigma":8.333333333333}}')"
contains "$rate_1vs1" '"first_player"'
contains "$rate_1vs1" '"second_player"'
contains "$rate_1vs1" '29.395831692'
contains "$rate_1vs1" '20.604168307'

quality_1vs1="$("$cli" quality-1vs1 '{"first_player":{"mu":25,"sigma":8.333333333333},"second_player":{"mu":25,"sigma":8.333333333333}}')"
contains "$quality_1vs1" '"quality":0.4472135954'

exposure="$("$cli" expose '{"rating":{"mu":29.396,"sigma":7.171}}')"
contains "$exposure" '"exposure":7.882'

draw_probability="$("$cli" draw-probability '{"first_player":{"mu":25,"sigma":8.333333333333},"second_player":{"mu":25,"sigma":8.333333333333}}')"
contains "$draw_probability" '"draw_probability":0.0448154975'

team_draw_probability="$("$cli" draw-probability '{"rating_groups":[[{"mu":25,"sigma":8.333333333333},{"mu":27,"sigma":6}],[{"mu":26,"sigma":7}]],"weights":[[1,0.5],[1]]}')"
contains "$team_draw_probability" '"draw_probability":0.0350737324'

if "$cli" draw-probability '{"rating_groups":[[{"mu":25,"sigma":8.333333333333}],[{"mu":25,"sigma":8.333333333333}],[{"mu":25,"sigma":8.333333333333}]]}' >/tmp/skill_rating_cli_draw_error.out 2>/tmp/skill_rating_cli_draw_error.err; then
    echo "expected free-for-all draw probability request to fail" >&2
    exit 1
fi
contains "$(cat /tmp/skill_rating_cli_draw_error.err)" 'free-for-all draw probability is not supported'

rate="$("$cli" rate '{"rating_groups":[[{"mu":32,"sigma":7}],[{"mu":25,"sigma":8.333333333333},{"mu":27,"sigma":6}],[{"mu":20,"sigma":8}]],"ranks":[1,0,2],"weights":[[1],[1,0.5],[1]]}')"
contains "$rate" '"rating_groups"'
contains "$rate" '28.391918161'
contains "$rate" '17.815292649'

stdin_quality="$(printf '%s' '{"rating_groups":[[{"mu":25,"sigma":8.333333333333}],[{"mu":25,"sigma":8.333333333333}]]}' | "$cli" quality -)"
contains "$stdin_quality" '"quality":0.4472135954'

if "$cli" quality '{"rating_groups":[[{"mu":25,"sigma":8.333333333333}]]}' >/tmp/skill_rating_cli_error.out 2>/tmp/skill_rating_cli_error.err; then
    echo "expected invalid CLI request to fail" >&2
    exit 1
fi
contains "$(cat /tmp/skill_rating_cli_error.err)" 'at least two teams are required'
