# Jaguar Programming Language — Native Compiler & Runtime Toolchain

Jaguar is a statically-typed, live-reloading scripting and server-side language built for native speed.

## Features
- **Static Type Checking**: Strong compile-time type safety (`string`, `num`, `decimal`, `bool`, `scifi`, `data`, `list<T>`, `MixedList`, `Task<T>`, `Worker`, `Socket`).
- **Live Reloading**: Instant file watching (`jag -live=1`) with graceful socket re-binding without port leaks.
- **Dual Execution Backends**:
  - **Interpreter Backend** (`jag run`): Fast AST/bytecode interpreter for rapid development and live mode.
  - **AOT Native Backend** (`jag build`): Transpiles to C99 and compiles native binaries linking against `libjagrt`.
- **Built-in Async & Reactor**: Non-blocking `epoll` reactor loop, stackful coroutine scheduler (`Task<T>`, `await`, `Task.all`), and event-loop timers (`live.after`, `live.every`, `live.clear`).
- **Raw HTTP & WebSockets**: Embedded raw HTTP/1.1 and RFC 6455 WebSockets server and client.
- **Parallel Worker Threads**: POSIX thread worker pool (`worker.pool`, `worker.spawn`, `worker.run`) with strict memory isolation.

## Usage
```bash
jag hello.jag                   # Run via interpreter
jag run hello.jag               # Explicit interpreter run
jag -live=1 hello.jag           # Live reload mode
jag build hello.jag -o myapp    # AOT native compilation
jag check hello.jag             # Parse and typecheck only
```

## Installation
```bash
./install.sh
./uninstall.sh
```
