# kotatsu

A C++20/23 coroutine wrapper for libuv. Library modules live under
`include/kota/<module>/` (`async`, `codec`, `deco`, `http`, `ipc`, `meta`,
`support`, `zest`).

## Build system

**CMake is the primary build system.** `xmake.lua` is an auxiliary build
description for downstream xmake users, kept working by the small `xmake.yml`
CI workflow — never use xmake for local development.

- All configuration lives in `CMakePresets.json`; trees land in
  `build/<preset>/`. Build with `pixi run build [preset]` (default `debug`;
  also `asan`, `tsan`, `no-exceptions`, ...) — this uses the pixi toolchain,
  never the system compiler.
- The main CI matrix is `cmake.yml` (all toolchains, sanitizers,
  no-exceptions/no-rtti variants), driving the same presets.

## Testing

- Unit tests: `pixi run test [preset]` (ctest; handles the snapshot dir).
  Running `unit_tests` by hand — e.g. to pass `--test-filter` — must be done
  from the repo root with `--snapshot-dir=tests/snapshots`, or snapshot tests
  fail spuriously.
- Integration tests: `pixi run integration-test`.

## Skills

Read the relevant skill before acting: `cpp-style` before writing C++,
`build`/`test` to build and run suites, `pr` before committing or opening a
PR, `format` (`pixi run format`) before every commit, `codex` before
delegating work to the codex CLI (plan/code review, debugging, test writing,
scoped implementation).
