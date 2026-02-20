# eventide

`eventide` is a C++23 toolkit extracted from the `clice` ecosystem.
It started as a coroutine wrapper around [libuv](https://github.com/libuv/libuv), and now also includes compile-time reflection, serde utilities, a typed LSP server layer, and a lightweight test framework.

## Feature Coverage

### `eventide` async runtime (`include/eventide/*`)

- Coroutine task system (`task`, shared task/future utilities, cancellation).
- Event loop scheduling (`event_loop`, `run(...)` helper).
- Task composition (`when_all`, `when_any`).
- Network / IPC wrappers:
  - `stream` base API
  - `pipe`, `tcp_socket`, `console`
  - `udp`
- Process API (`process::spawn`, stdio pipe wiring, async wait/kill).
- Async filesystem API (`fs::*`) and file watch API (`fs_event`).
- Libuv watcher wrappers (`timer`, `idle`, `prepare`, `check`, `signal`).
- Coroutine-friendly sync primitives (`mutex`, `semaphore`, `event`, `condition_variable`).

### `reflection` (`include/reflection/*`)

- Aggregate reflection:
  - field count
  - field names
  - field references and field metadata iteration
  - field offsets (by index and by member pointer)
- Enum reflection and enum name/value mapping.
- Compile-time type/pointer/member name extraction.
- Callable/function traits (`callable_args_t`, `callable_return_t`, etc.).

### `serde` (`include/serde/*`)

- Generic serialization/deserialization trait layer.
- Field annotation system:
  - `rename`, `alias`, `flatten`, `skip`, `skip_if`, `literal`, `enum_string`, etc.
- Backend integrations:
  - `serde::json::simd` (simdjson-based JSON serializer/deserializer)
  - FlatBuffers/FlexBuffers helpers (`flatbuffers/flex/*`, schema helpers)

### `language` (`include/language/*`)

- Typed language server abstraction (`LanguageServer`).
- Typed request/notification registration with compile-time signature checks.
- Stream transport abstraction for stdio / TCP (`Transport`, `StreamTransport`).
- Generated LSP protocol model (`include/language/protocol.h`).

### `zest` test framework (`include/zest/*`)

- Minimal unit test framework used by this repository.
- Test suite / test case registration macros.
- Expect/assert helpers with formatted failure output and stack trace support.
- Test filtering via `--test-filter=...`.

## Repository Layout

```text
include/
  eventide/      # Public async runtime APIs
  reflection/    # Compile-time reflection utilities
  serde/         # Generic serde + backend adapters
  language/      # LSP-facing server and transport interfaces
  zest/          # Internal test framework headers

src/
  eventide/      # Async runtime implementations
  serde/         # FlatBuffers/FlexBuffers serde implementation
  language/      # Language server + transport implementation
  reflection/    # Reflection target wiring (header-only public APIs)
  zest/          # Test runner implementation

tests/
  reflection/    # Reflection behavior tests
  eventide/      # Runtime/event-loop/IO/process/fs/sync tests
  serde/         # JSON/FlatBuffers serde tests
  language/      # Language server tests

scripts/
  lsp_codegen.py # LSP schema -> C++ protocol header generator
```
