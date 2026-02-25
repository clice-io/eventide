# eventide

`eventide` is a C++23 toolkit extracted from the `clice` ecosystem.
It started as a coroutine wrapper around [libuv](https://github.com/libuv/libuv), and now also includes compile-time reflection, serde utilities, a typed LSP server layer, a lightweight test framework, an LLVM-compatible option parsing library, and a declarative option library built on it.

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

### `reflection` (`include/eventide/reflection/*`)

- Aggregate reflection:
  - field count
  - field names
  - field references and field metadata iteration
  - field offsets (by index and by member pointer)
- Enum reflection and enum name/value mapping.
- Compile-time type/pointer/member name extraction.
- Callable/function traits (`callable_args_t`, `callable_return_t`, etc.).

### `serde` (`include/eventide/serde/*`)

- Generic serialization/deserialization trait layer.
- Field annotation system:
  - `rename`, `alias`, `flatten`, `skip`, `skip_if`, `literal`, `enum_string`, etc.
- Backend integrations:
  - `serde::json::simd` (simdjson-based JSON serializer/deserializer)
  - FlatBuffers/FlexBuffers helpers (`flatbuffers/flex/*`, schema helpers)

### `language` (`include/eventide/language/*`)

- Typed language server abstraction (`LanguageServer`).
- Typed request/notification registration with compile-time signature checks.
- Stream transport abstraction for stdio / TCP (`Transport`, `StreamTransport`).
- Generated LSP protocol model (`include/eventide/language/protocol.h`).

### `option` (`include/eventide/option/*`)

- LLVM-compatible option parsing model (`OptTable`, `Option`, `ParsedArgument`).
- Supports common option styles:
  - flag (`-v`)
  - joined (`-O2`)
  - separate (`--output file`)
  - comma-joined (`--list=a,b,c`)
  - fixed multi-arg (`--pair left right`)
  - remaining args / trailing packs
- Alias/group matching, visibility/flag filtering, and grouped short options.
- Callback-based parse APIs for one-arg and whole-argv flows.

### `deco` (`include/eventide/deco/*`)

- **Dec**larative **o**ption library
- Declarative CLI option definition macros (`DecoFlag`, `DecoKV`, `DecoInput`, `DecoPack`, etc.).
- Reflection-driven compile-time option table generation on top of `eventide::option`.
- Runtime parser + dispatch layer (`deco::cli::parse`, `Dispatcher`, `SubCommander`).
- Built-in usage/help descriptor rendering and category constraints:
  - required options
  - required/exclusive categories
  - nested config scopes

### `zest` test framework (`include/eventide/zest/*`)

- Minimal unit test framework used by this repository.
- Test suite / test case registration macros.
- Expect/assert helpers with formatted failure output and stack trace support.
- Test filtering via `--test-filter=...`.

## Repository Layout

```text
include/
  eventide/      # Public headers
    async/       # Async runtime APIs
    common/      # Shared utilities
    deco/        # Declarative CLI layer built on option + reflection
    language/    # LSP-facing server and transport interfaces
    option/      # LLVM-compatible option parsing layer
    reflection/  # Compile-time reflection utilities
    serde/       # Generic serde + backend adapters
    zest/        # Internal test framework headers

src/
  async/         # Async runtime implementations
  option/        # Option parser implementation
  deco/          # Deco target wiring (header-only APIs)
  serde/         # FlatBuffers/FlexBuffers serde implementation
  language/      # Language server + transport implementation
  reflection/    # Reflection target wiring (header-only public APIs)
  zest/          # Test runner implementation

tests/
  option/        # Option parser behavior tests
  deco/          # Declarative CLI/deco tests
  reflection/    # Reflection behavior tests
  eventide/      # Runtime/event-loop/IO/process/fs/sync tests
  serde/         # JSON/FlatBuffers serde tests
  language/      # Language server tests

scripts/
  lsp_codegen.py # LSP schema -> C++ protocol header generator
```
