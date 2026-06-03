# Source Internals

The public header is intentionally small. Most implementation details live under
`src/`.

## `src/skill_rating.cpp`

This file connects the public API to the algorithm internals.

Responsibilities:

- Validate ratings, teams, ranks, weights, and environment parameters.
- Normalize omitted ranks and weights.
- Skip zero-weight players during graph construction and return them unchanged.
- Build the factor graph for a match.
- Run the iterative rating update.
- Compute match quality.
- Preserve input team ordering in returned results.

## `src/detail/math.*`

This module contains numeric helpers that are not specific to the public API.

Important pieces:

- `normal_pdf`
- `normal_cdf`
- `inverse_normal_cdf`
- `determinant`
- `solve`
- validation helpers such as `require_probability`

The inverse CDF is implemented locally to keep the library dependency-free.

## `src/detail/factor_graph.*`

This module contains the Bayesian skill rating message-passing primitives.

Important pieces:

- `Gaussian`: internal precision/precision-mean representation.
- `Variable`: graph variable holding a Gaussian value.
- `SumLayer`: helper for sum factors such as team performance and performance differences.
- `update_prior`
- `update_likelihood_value`
- `update_likelihood_mean`
- `update_sum`
- `update_trunc`

These types are internal on purpose. Users should work with `Rating` and
`Environment`, not factor graph primitives.

## `src/app/*`

This layer is shared by the CLI and HTTP service.

Responsibilities:

- Parse JSON request bodies with `nlohmann/json`.
- Convert JSON into `Rating`, `RatingGroups`, ranks, weights, and environment values.
- Dispatch commands such as `rate`, `rate-1vs1`, `quality`, `quality-1vs1`, and `expose`.
- Print JSON responses.

Keeping this layer separate means the core `skill_rating` library does not depend
on frontend JSON or HTTP dependencies.

## `src/cli/main.cpp`

This file implements the JSON CLI.

Responsibilities:

- Read JSON from a file, stdin, or a command argument.
- Call the shared app command handler.
- Print the JSON response or a readable error.

## `src/http/main.cpp`

This file implements the local JSON HTTP service.

Responsibilities:

- Serve HTTP routes with `cpp-httplib`.
- Route `POST` requests to the shared app command handler.
- Return JSON responses and JSON errors.
- Provide `GET /health` for smoke tests.

The service is intentionally local-first: TLS, authentication, metrics, and
deployment packaging are responsibilities of the embedding or calling
application.

## `tests/skill_rating_tests.cpp`

The tests use a small custom runner. It prints named pass/fail lines when run
directly:

```sh
./build/skill_rating_tests
```

The tests cover default values, 1v1 quality, wins, draws, team
matches, ranks, partial-play weights, zero-weight players, custom environments,
draw helper round trips, multi-team draws, and validation errors.

## `tests/http_smoke.sh`

This script starts the HTTP service on loopback, checks `/health`, calls
every JSON route, checks common error status codes, and then stops the service.

## `tests/cli_smoke.sh`

This script exercises every CLI command, stdin input, and a validation failure.

## `tests/package_install_smoke.sh`

This script installs the core library into a temporary prefix, configures a
separate consumer project with `find_package(skill_rating_cpp REQUIRED)`, builds it,
and runs it.
