# Core concepts

[English](../../en/guide/02-concepts.md) | [中文](../../zh/guide/02-concepts.md)

## Queues: serial and concurrent

A queue is a destination for work. Its **width** decides how tasks run:

- **Width 1 — serial.** Tasks run one at a time, in submission order (FIFO). Use it to protect state that must not be touched concurrently.
- **Width > 1 — concurrent.** Up to `width` tasks run in parallel on the thread pool.

```php
$q = GCD\queue_create('ordered', 1);
$out = tempnam(sys_get_temp_dir(), 'gcd_ord_');
file_put_contents($out, '');
for ($i = 0; $i < 5; $i++) {
    GCD\async($q, function (string $f, int $n) {
        file_put_contents($f, (string) $n, FILE_APPEND | LOCK_EX);
    }, [$out, $i]);
}
GCD\drain();
echo file_get_contents($out);   // 01234 — FIFO guaranteed
unlink($out);
```

Queues are handles: create them with [`queue_create`](../api/queue.md#queue_create), or use the built-in [`global_queue`](../api/queue.md#global_queue) and [`main_queue`](../api/queue.md#main_queue).

## The thread model

The extension runs a custom pthread pool. Each worker thread boots an **isolated PHP runtime** — one request per thread. Consequences you must design around:

- Worker output is unsynchronized: a task's `echo` can appear interleaved on STDOUT. Collect results through files or [shared containers](07-containers.md); [`sync`](03-dispatch.md) waits for a task but returns `null`.
- Worker runtimes persist across tasks: globals and statics can leak between tasks that land on the same worker, and are never shared across workers. Do not use them for cross-task state.
- Only the closure's own code travels to the worker. Functions, classes and constants defined in your script are **not available** inside tasks — calling them can crash the process. Define helpers inside the task closure, or `require` them from it.
- Only the shared containers (Atomic, Map, Set, ConcurrentQueue) are built for cross-thread access.

## Task rules

A task is a **closure plus an argument array**. v1 copies the closure's op_array to persistent storage; the copy fails — throwing `GCD\CaptureException` — when the closure:

- captures variables (the `use` construct),
- declares static variables.

try/catch inside a task is allowed. Pass everything the task needs through `$argv` instead:

```php
$q = GCD\queue_create('rules', 1);
try {
    GCD\async($q, function () { static $counter = 0; $counter++; });
} catch (GCD\CaptureException $e) {
    echo "rejected: statics are not supported in v1 tasks\n";
}
```

Argument values are deep-copied. **Only `null`, booleans, integers, floats, strings, and arrays of those are supported.** Objects (including `GCD\Data`), closures and resources are not: passing them can crash the process. Move object data as strings and rebuild it inside the task.

## Handle objects

`GCD\Queue`, `GCD\Group`, `GCD\Semaphore` and `GCD\Source` are handles: objects with no methods that name a kernel-side resource. You create them through factory functions and pass them back to the API. `GCD\Data` is an immutable buffer with a single `size()` method; the shared containers expose real methods.

## Where to go next

- [Dispatching work](03-dispatch.md) — async/sync, barriers, apply, once
- [Shared containers](07-containers.md) — the cross-thread state toolbox
- [Queues & QoS API](../api/queue.md)