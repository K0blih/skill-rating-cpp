# Implementation Decisions

## Compiled Library Instead Of Header-Only

The project is built as a compiled CMake library. This keeps compile times lower
for consumers and gives us a clean target that later CLI and HTTP binaries can
link against.

Header-only would be simpler for a tiny library, but the factor graph and math
implementation are large enough that hiding them in `.cpp` files is cleaner.

## Public API Separate From Internals

Public API lives in `include/skill_rating/skill_rating.hpp`. Internal implementation
lives in `src/detail/`.

This keeps users on stable concepts:

- ratings
- teams
- ranks
- weights
- environments

It avoids exposing implementation details like Gaussian precision math and
message passing.

## `include/skill_rating/` Folder

The nested include folder lets users write:

```cpp
#include <skill_rating/skill_rating.hpp>
```

This avoids collisions with other libraries that might also have a file named
`rating.hpp`, `environment.hpp`, or `skill_rating.hpp` once installed.

## Dependency-Light Core

The core rating library uses only the C++ standard library.

The CLI and HTTP service use `nlohmann/json` for JSON and `cpp-httplib` for the
HTTP server. They are kept outside the core `skill_rating` target so library users
do not inherit frontend dependencies unless they build those targets.

The install/export package only installs the core library target and public
headers. CLI and HTTP frontends are build targets, not part of the core package
contract.

## Factor Graph Style

The rating update follows the classic Bayesian skill rating factor-graph approach instead of
using only closed-form 1v1 formulas.

That choice matters because the library needs to support:

- teams
- more than two teams
- free-for-all matches
- draws
- partial-play weights

A factor graph handles those cases with one general update path.

## Plain Test Runner

The tests do not use a third-party framework. The custom runner is enough for
the 1.0 project size and keeps setup simple.

If test coverage grows substantially, moving to Catch2, doctest, or GoogleTest
would be reasonable.
