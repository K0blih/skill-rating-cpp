# CLI Usage

The CLI target is `skill_rating_cli`. It uses `nlohmann/json` and reads JSON from a
file, a JSON command argument, or stdin.

```sh
./build/skill_rating_cli <command> [input.json|-|json]
```

Commands:

- `rate`
- `rate-1vs1`
- `quality`
- `quality-1vs1`
- `expose`
- `draw-probability`

## `rate-1vs1`

Input:

```json
{
  "first_player": {"mu": 25.0, "sigma": 8.333333333333},
  "second_player": {"mu": 25.0, "sigma": 8.333333333333},
  "drawn": false
}
```

Command:

```sh
./build/skill_rating_cli rate-1vs1 input.json
```

Output:

```json
{
  "first_player": {"mu": 29.395831693, "sigma": 7.17147580701},
  "second_player": {"mu": 20.604168307, "sigma": 7.17147580701}
}
```

Without `"drawn": true`, the first player is treated as the winner.

## `rate`

Input:

```json
{
  "rating_groups": [
    [{"mu": 32.0, "sigma": 7.0}],
    [{"mu": 25.0, "sigma": 8.333333333333}, {"mu": 27.0, "sigma": 6.0}],
    [{"mu": 20.0, "sigma": 8.0}]
  ],
  "ranks": [1, 0, 2],
  "weights": [
    [1.0],
    [1.0, 0.5],
    [1.0]
  ]
}
```

Command:

```sh
./build/skill_rating_cli rate input.json
```

Output:

```json
{
  "rating_groups": [
    [{"mu": 31.2792902195, "sigma": 5.98723455705}],
    [{"mu": 28.3919181613, "sigma": 7.19292639796}, {"mu": 27.8792668566, "sigma": 5.90056509681}],
    [{"mu": 17.8152926491, "sigma": 7.08576695558}]
  ]
}
```

Lower rank is better. Equal ranks are draws. Output keeps the same team/player
ordering as input.

Weights are optional. If present, they must mirror the `rating_groups` shape.
Weight `0.0` means the player did not participate and is returned unchanged.
Each team must have at least one positive weight.

## `quality`

Input:

```json
{
  "rating_groups": [
    [{"mu": 25.0, "sigma": 8.333333333333}],
    [{"mu": 25.0, "sigma": 8.333333333333}]
  ]
}
```

Output:

```json
{"quality": 0.4472135954999723}
```

## `quality-1vs1`

Input:

```json
{
  "first_player": {"mu": 25.0, "sigma": 8.333333333333},
  "second_player": {"mu": 25.0, "sigma": 8.333333333333}
}
```

Output:

```json
{"quality": 0.4472135954999723}
```

## `expose`

Input:

```json
{
  "rating": {"mu": 29.396, "sigma": 7.171}
}
```

Output:

```json
{"exposure": 7.883}
```

## `draw-probability`

1v1 input:

```json
{
  "first_player": {"mu": 25.0, "sigma": 8.333333333333},
  "second_player": {"mu": 25.0, "sigma": 8.333333333333}
}
```

Command:

```sh
./build/skill_rating_cli draw-probability input.json
```

Output:

```json
{"draw_probability": 0.04481549759126091}
```

Team-vs-team input:

```json
{
  "rating_groups": [
    [{"mu": 25.0, "sigma": 8.333333333333}, {"mu": 27.0, "sigma": 6.0}],
    [{"mu": 26.0, "sigma": 7.0}]
  ],
  "weights": [
    [1.0, 0.5],
    [1.0]
  ]
}
```

Output:

```json
{"draw_probability": 0.03507373242772174}
```

The command uses the library's `Environment::draw_probability` method, which
derives the internal draw margin from the environment and combines the players'
means, uncertainties, weights, and beta to estimate the chance that this
specific two-team match ends in a draw.

Free-for-all draw probability is not supported. Rating updates and quality still
support free-for-all matches, but `draw-probability` is intentionally a two-team
command.

## Custom Environment

Every command accepts an optional `environment` object. When provided, it must
include all five fields: `mu`, `sigma`, `beta`, `tau`, and `draw_probability`.

Example input:

```json
{
  "environment": {
    "mu": 25.0,
    "sigma": 8.333333333333,
    "beta": 4.166666666667,
    "tau": 0.083333333333,
    "draw_probability": 0.10
  },
  "rating": {"mu": 29.396, "sigma": 7.171}
}
```

Command:

```sh
./build/skill_rating_cli expose input.json
```

Output:

```json
{"exposure": 7.883}
```

The same environment object can be included in rating, quality, and
draw-probability requests:

```sh
./build/skill_rating_cli draw-probability '{"environment":{"mu":25,"sigma":8.333333333333,"beta":4.166666666667,"tau":0.083333333333,"draw_probability":0.2},"first_player":{"mu":25,"sigma":8.333333333333},"second_player":{"mu":25,"sigma":8.333333333333}}'
```
