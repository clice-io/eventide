# kotatsu

`kotatsu` is a C++23 toolkit extracted from the `clice` ecosystem.
It started as a coroutine wrapper around [libuv](https://github.com/libuv/libuv), and now also includes compile-time reflection, codec utilities, a typed IPC layer, generated LSP protocol bindings, a lightweight test framework, an LLVM-compatible option parsing library, and a declarative option library built on it.

All public APIs live under the `kota::` namespace, public headers under `include/kota/`, and CMake/xmake options use the `KOTA_` prefix.

## Feature Coverage

### `async` runtime (`include/kota/async/*`)

- Coroutine task system with shared tasks, futures, and cancellation.
- Event loop scheduling and task composition (wait-all / wait-any).
- Network and IPC I/O:
  - stream base abstraction
  - pipes, TCP sockets, TCP acceptors, console streams
  - UDP sockets
- Child process API (spawn, stdio piping, async wait/kill).
- Async filesystem API and filesystem watch notifications.
- Libuv watcher wrappers (timer, idle, prepare, check, signal).
- Coroutine-friendly sync primitives (mutex, semaphore, event, condition variable).

### `meta` (`include/kota/meta/*`)

- Aggregate reflection: field count, field names, field references, field metadata iteration, field offsets.
- Enum reflection with name/value mapping.
- Compile-time type, pointer, and member name extraction.
- Callable and function signature traits.

### `codec` (`include/kota/codec/*`)

- Generic serialization/deserialization trait layer with a clean split between codec contract and backend implementations.
- Field annotation system for renaming, aliasing, flattening, skipping, literal tagging, enum-as-string, and similar schema-level tweaks.
- Built-in backends:
  - JSON: both a portable content-based implementation and a high-performance backend powered by yyjson/simdjson.
  - Bincode (compact binary format).
  - TOML.
  - FlatBuffers (schema-driven) and FlexBuffers (schemaless).

### `ipc` (`include/kota/ipc/*`)

- JSON-RPC 2.0 protocol model with typed request / notification traits.
- Transport abstraction for framed message IO, a stream-based transport on top of the `async` runtime, and a recording transport for tests.
- Codec-parametric typed peer runtime supporting request dispatch, notifications, and nested RPC; predefined peers for JSON and Bincode codecs.
- Externally-driven execution model: callers own the event loop, schedule the peer's run loop, and drive shutdown explicitly.
- Structured error model with `std::expected`-based results and structured RPC errors (code, message, optional data).
- Protocol validation aligned with the JSON-RPC spec:
  - malformed payloads map to `ParseError` with null id
  - structurally invalid messages map to `InvalidRequest` with null id
  - parameter decode failures map to `InvalidParams`
- Cancellation model:
  - inbound `$/cancelRequest` cancels the matching in-flight handler and reports `RequestCancelled` (aligned with LSP `LSPErrorCodes::RequestCancelled`)
  - outbound requests can carry cancellation tokens or timeouts; cancelling a still-pending request sends `$/cancelRequest` to the remote peer
  - timeout overloads report `RequestCancelled` with a timeout message
  - request contexts make it easy to propagate the inbound handler's cancellation token into nested outbound requests
- Optional structured logging hook (log level + callback).

### `ipc/lsp` (`include/kota/ipc/lsp/*`)

- C++ protocol model generated from the LSP TypeScript meta-model.
- LSP request/notification traits layered on top of `kota::ipc::protocol`.
- LSP URI parsing/manipulation and position / line-column encoding helpers.
- Progress reporting helper for `$/progress` work-done notifications.

### `option` (`include/kota/option/*`)

- LLVM-compatible option parsing model.
- Supports common option styles:
  - flag (e.g. `-v`)
  - joined (e.g. `-O2`)
  - separate (e.g. `--output file`)
  - joined-or-separate (accepts both `-o value` and `-ovalue`)
  - comma-joined (e.g. `--list=a,b,c`)
  - fixed-arity multi-arg (e.g. `--pair left right`)
  - remaining / trailing argument packs
- Alias and option-group matching, visibility and flag-based filtering, grouped short options.
- Callback-based parse APIs for both per-argument and whole-argv flows.

### `deco` (`include/kota/deco/*`)

- **Dec**larative **o**ption library: describe options as reflected structs rather than imperative tables.
- Compile-time option table generation driven by `meta` reflection on top of `kota::option`.
- Runtime parser, dispatcher, and sub-command router.
- Built-in usage/help rendering and category constraints (required options, required/exclusive groups, nested config scopes).

### `zest` test framework (`include/kota/zest/*`)

- Minimal unit test framework used throughout this repository.
- Suite / test-case registration and expect/assert helpers with formatted failure output and stack traces.
- Test filtering by suite or suite.case pattern (wildcards supported), available both via the default CLI runner (`--test-filter=...`) and programmatically.

### `support` (`include/kota/support/*`)

- Shared utility header layer used by every other module.
- Copy-on-write and small-buffer-optimized string / vector containers.
- String view, fixed-length compile-time string, and naming-convention (spelling) conversion helpers.
- Callable trait and compile-time type-list utilities.

## Repository Layout

```text
include/
  kota/        # Public headers
    async/       # Async runtime APIs
    support/     # Shared utilities
    deco/        # Declarative CLI layer built on option + meta
    ipc/         # IPC protocol, peer, and transport APIs
    ipc/lsp/     # LSP protocol model and utilities
    option/      # LLVM-compatible option parsing layer
    meta/        # Compile-time reflection utilities
    codec/       # Generic codec + backend adapters
    zest/        # Internal test framework headers

src/
  async/         # Async runtime implementations
  ipc/           # IPC peer and transport implementations
  option/        # Option parser implementation
  deco/          # Deco target wiring (header-only APIs)
  codec/         # FlatBuffers/FlexBuffers codec implementation
  ipc/lsp/       # URI/position implementations
  meta/          # Meta/reflection target wiring (header-only public APIs)
  zest/          # Test runner implementation

tests/
  unit/          # Unit tests
    option/      # Option parser behavior tests
    deco/        # Declarative CLI/deco tests
    meta/        # Reflection behavior tests
    async/       # Runtime/event-loop/IO/process/fs/sync tests
    ipc/         # IPC peer and transport tests
    ipc/lsp/     # LSP utility, progress, and jsonrpc-trait tests
    codec/       # JSON/FlatBuffers codec tests
    support/     # Shared utility tests (cow_string, small_vector, etc.)

examples/
  ipc/       # IPC stdio, scripted, and multi-process examples

scripts/
  lsp_codegen.py # LSP schema -> C++ protocol header generator
```
