---
name: build
description: Build kotatsu. Optional arg = build tree (default `build`; also build-asan, build-tsan, build-clang, build-cov, build-codec-verify, build-codec-verify-clang). Runs in a forked context — compile output and mechanical fixes stay out of the main conversation; only the outcome returns.
context: fork
---

Build the project into the requested build tree (default `build`, the day-to-day Debug tree).

- Local build: `cmake --build <tree>` from the repo root (all trees are pre-configured cmake+ninja; common targets: `unit_tests`).
- If a tree is missing or its cache is stale enough that a build can't proceed, report that instead of reconfiguring on your own — configuration flags are deliberate per tree (sanitizers, toolchain, codec verify).
- The auxiliary xmake build is CI-only (`xmake.yml`); to reproduce a failure from it locally, run the workflow's inlined `xmake config`/`xmake --all`/`xmake test` commands verbatim.

On failure:

- Mechanical breakage (missing include, renamed symbol, stale call site after an agreed-on change): fix it, rebuild, and list every file you touched in the report.
- Design-level errors (the fix requires a decision): do not guess — report the error with `file:line` and the relevant excerpt.

Report back: build tree, success or failure, files changed (if any), and for failures a digested error list — never the raw compiler spew.
