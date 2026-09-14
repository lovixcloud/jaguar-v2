# Jaguar Compiler — Design Decisions & Architectural Notes

## Architecture Overview
The Jaguar toolchain is designed as a C11 codebase structured into clear subsystems:
- `lexer/`: Scanner with string template interpolation tracking (`{{var}}`, `{expr}`, `${}`).
- `parser/`: Recursive-descent parser constructing the AST.
- `ast/`: AST node structures and type representations.
- `typecheck/`: Static type checker enforcing `await` context legality, `fixed` immutability, and thread isolation across worker boundaries.
- `runtime/`: `libjagrt` runtime engine providing core values, maps (`data`), lists, JSON parsing, non-blocking `epoll` event loop, coroutines (`ucontext.h`), HTTP/1.1, WebSockets, and POSIX worker pools.
- `backend_vm/`: Virtual Machine interpreter.
- `backend_c/`: Portable C99 AOT transpiler.
- `src/cli.c`: `jag` CLI binary.

## Concurrency & Thread Isolation
- Communication across worker boundaries uses value deep-copying over thread-safe queues.
- Captured mutable handles (e.g. sockets) across `worker.spawn`/`worker.run` are rejected at typecheck time.
