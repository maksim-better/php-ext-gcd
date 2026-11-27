# ext-gcd Documentation (English)

[English](../en/README.md) | [中文](../zh/README.md)

GCD-style (Grand Central Dispatch) concurrency for ZTS PHP: dispatch closures onto serial or concurrent queues backed by a real OS thread pool, synchronize with groups and semaphores, watch event sources, and share state through thread-safe containers.

## Guide

1. [Quickstart](guide/01-quickstart.md) — install, enable, first concurrent task
2. [Core concepts](guide/02-concepts.md) — queues, the thread model, task rules
3. [Dispatching work](guide/03-dispatch.md) — async/sync, barriers, apply, once
4. [Synchronization](guide/04-sync.md) — groups and semaphores
5. [Event sources](guide/05-sources.md) — timers, signals, processes, file descriptors
6. [Data buffers](guide/06-data.md) — byte buffers and fd I/O
7. [Shared containers](guide/07-containers.md) — Atomic, Map, Set, ConcurrentQueue
8. [Error handling](guide/08-errors.md) — handlers and exceptions
9. [QoS and the main loop](guide/09-qos-mainloop.md) — priorities, main()/drain()

## API Reference

- [API index](api/README.md) — all functions, classes, constants, exceptions
- [Queues & QoS](api/queue.md)
- [Dispatch](api/dispatch.md)
- [Groups](api/group.md)
- [Semaphores](api/semaphore.md)
- [Event sources](api/source.md)
- [Data](api/data.md)
- [Main loop](api/loop.md)
- [Errors](api/errors.md)
- [Containers](api/containers.md)