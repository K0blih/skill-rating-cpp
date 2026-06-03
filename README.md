# skill-rating-cpp
C++ implementation of a Bayesian skill rating algorithm

## Status

This repository currently provides the core C++20 library, a basic JSON CLI, and
a basic JSON HTTP service.

The HTTP service is intended for local development and controlled internal use.
It handles concurrent local requests with a configurable worker pool. Public
exposure, TLS termination, authentication, persistence, queueing, retries,
logging, and monitoring are responsibilities of the application that embeds or
calls it.

## Current capabilities

- Create ratings with standard Bayesian skill rating defaults or custom environment values.
- Update ratings for one-vs-one matches, team matches, free-for-all matches, and draws.
- Use explicit ranks where lower rank means a better result.
- Use partial-play weights for players who contributed less than a full match.
- Use zero weights for players who did not participate; they are returned unchanged.
- Compute match quality for one-vs-one and multi-team matches.
- Compute draw probability for one-vs-one and two-team matches.
- Compute conservative exposure as `mu - 3 * sigma`.
- Convert between draw probability and draw margin.

## Planned additions

- Richer CLI ergonomics, examples, and shell-completion support.
- Richer HTTP service ergonomics, observability, and deployment examples.
- Serialization helpers for common request/response formats.

## Build And Test

The core library has no third-party runtime dependency. Building the CLI or HTTP
service fetches `nlohmann/json` and `cpp-httplib` with CMake `FetchContent`.

## Third-Party Dependencies

The core library target uses only the C++ standard library.

Optional frontend targets fetch these MIT-licensed dependencies at configure
time:

- `nlohmann/json`: https://github.com/nlohmann/json
- `cpp-httplib`: https://github.com/yhirose/cpp-httplib

### Installable Package

Install the core library and CMake package files:

```sh
mkdir -p build
cd build
cmake .. -DSKILL_RATING_BUILD_TESTS=OFF -DSKILL_RATING_BUILD_CLI=OFF -DSKILL_RATING_BUILD_HTTP=OFF
make -j
cmake --install . --prefix /path/to/install
```

Use it from another CMake project:

```cmake
find_package(skill_rating_cpp REQUIRED)
target_link_libraries(my_app PRIVATE skill_rating::skill_rating)
```

### Library

Build only the core library:

```sh
mkdir -p build
cd build
cmake .. -DSKILL_RATING_BUILD_TESTS=OFF -DSKILL_RATING_BUILD_CLI=OFF -DSKILL_RATING_BUILD_HTTP=OFF
make -j
```

Build the library with tests:

```sh
mkdir -p build
cd build
cmake .. -DSKILL_RATING_BUILD_TESTS=ON -DSKILL_RATING_BUILD_CLI=OFF -DSKILL_RATING_BUILD_HTTP=OFF
make -j
ctest --output-on-failure
./skill_rating_tests
```

### CLI

Build the CLI with the library and `nlohmann/json`:

```sh
mkdir -p build
cd build
cmake .. -DSKILL_RATING_BUILD_CLI=ON
make -j
./skill_rating_cli --help
```

Run a direct CLI smoke check:

```sh
./skill_rating_cli quality '{"rating_groups":[[{"mu":25,"sigma":8.333333333333}],[{"mu":25,"sigma":8.333333333333}]]}'
```

When tests are enabled, CTest also runs `tests/cli_smoke.sh`, which covers all
CLI commands and an error path.

### HTTP Service

Build the HTTP service with the library, `nlohmann/json`, and `cpp-httplib`:

```sh
mkdir -p build
cd build
cmake .. -DSKILL_RATING_BUILD_HTTP=ON
make -j
./skill_rating_http --host 127.0.0.1 --port 8080 --workers 8
```

In another shell:

```sh
curl http://127.0.0.1:8080/health
curl -sS -H 'Content-Type: application/json' \
  -d '{"rating_groups":[[{"mu":25,"sigma":8.333333333333}],[{"mu":25,"sigma":8.333333333333}]]}' \
  http://127.0.0.1:8080/quality
```

Run all enabled tests, including CLI and HTTP smoke tests:

```sh
mkdir -p build
cd build
cmake .. -DSKILL_RATING_BUILD_TESTS=ON -DSKILL_RATING_BUILD_CLI=ON -DSKILL_RATING_BUILD_HTTP=ON
make -j
ctest --output-on-failure
```

The HTTP smoke test starts the service on loopback and checks every route plus
basic error responses.

## Documentation

- [Usage example](docs/example.md)
- [CLI usage](docs/cli.md)
- [HTTP service usage](docs/http.md)
- [Public API overview](docs/api.md)
- [Source internals](docs/internals.md)
- [Implementation decisions](docs/design.md)
