# kotatsu

A C++20/23 coroutine wrapper for libuv. Library modules live under
`include/kota/<module>/` (`async`, `codec`, `deco`, `http`, `ipc`, `meta`,
`support`, `zest`).

## Build system

**CMake is the primary build system.** `xmake.lua` is an auxiliary build
description for downstream xmake users, kept working by the small `xmake.yml`
CI workflow — never use xmake for local development.

- Local dev uses pre-configured cmake+ninja trees at the repo root: `build`
  (Debug, day-to-day), plus variants like `build-asan`, `build-tsan`,
  `build-cov`. Build with `cmake --build <tree>`; don't reconfigure trees on
  your own.
- The main CI matrix is `cmake.yml` (all toolchains, sanitizers,
  no-exceptions/no-rtti variants), driving cmake/ctest directly.

## Testing

- Unit tests — always from the repo root, always with the snapshot dir:
  `./build/unit_tests --snapshot-dir=tests/snapshots`. Omitting the snapshot
  dir makes snapshot tests fail spuriously.
- Integration tests: `pixi run integration-test`.

## Skills

Read the relevant skill before acting: `cpp-style` before writing C++,
`build`/`test` to build and run suites, `pr` before committing or opening a
PR, `format` (`pixi run format`) before every commit.
