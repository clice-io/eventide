---
name: test
description: Run kotatsu test suites. Optional args = suite (unit | integration | all, default all) and build tree for unit tests (default `build`). Runs in a forked context — test output stays out of the main conversation; only a digest returns.
context: fork
---

Run the requested suites (default: all).

- Unit tests — always from the repo root, and always with the snapshot dir:

  ```bash
  ./<tree>/unit_tests --snapshot-dir=tests/snapshots
  ```

  Omitting `--snapshot-dir=tests/snapshots` (or running from another directory) makes snapshot tests fail spuriously — never diagnose failures from a run without it.

- Integration tests (LSP stub server, pytest): `pixi run integration-test`

Filtering unit tests: `--test-filter=Suite.Case` (also `Suite.*` or a bare positional pattern). Rerun a single integration test with `pytest tests/integration/test_foo.py -v -k <name>`.

Rules:

- This skill runs and reports — nothing else. Never fix code or tests from here, never skip or weaken anything, never pass `--update-snapshots` (snapshot updates are a deliberate act in the main conversation).
- Build the tree first if the binary is stale (the build skill); a stale `unit_tests` silently tests old code.

Report back, per suite: pass/fail and counts. For each failure: test name, `file:line`, the assertion message or snapshot-diff excerpt (trimmed), and the exact filter command to reproduce it.
