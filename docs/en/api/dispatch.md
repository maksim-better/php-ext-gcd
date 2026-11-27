# Dispatch

[English](../../en/api/dispatch.md) | [中文](../../zh/api/dispatch.md)

## async

**Signature**

```text
GCD\async(GCD\Queue $queue, Closure $task, array $argv = []): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$queue` | `GCD\Queue` | Destination queue |
| `$task` | `Closure` | User-function closure; no captures or statics (v1); try/catch allowed |
| `$argv` | `array` | Arguments passed to the closure; only null/bool/int/float/string and arrays of those — objects can crash the process |

**Return** — Nothing (`null`). Does not wait for the task.

**Errors** — `TypeError` ("expected GCD\Queue") for a non-Queue; `TypeError` ("task must be a user-function Closure") for a non-user closure; `GCD\CaptureException` for captures/statics/try-catch.

**Notes** — Serial queues enqueue FIFO; concurrent queues submit to the pool. Exceptions thrown inside a task go to the [error reporting layer](errors.md), not to the caller.

**Example**

```php
$q = GCD\queue_create('jobs', 2);
$out = tempnam(sys_get_temp_dir(), 'gcd_a_');
GCD\async($q, function (string $f) { file_put_contents($f, 'async'); }, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## sync

**Signature**

```text
GCD\sync(GCD\Queue $queue, Closure $task, array $argv = []): void
```

**Parameters** — Same as `async`.

**Return** — Nothing (`null`). Blocks until the task completes; the task's own return value is discarded.

**Errors** — Same as `async`, plus `GCD\DeadlockException` ("sync dispatch to current serial queue would deadlock") when the target is the serial queue the calling task is currently executing on.

**Notes** — The deadlock guard applies only to `sync`, only for width-1 queues, only when target == current queue. Collect results through files or shared containers.

**Example**

```php
$q = GCD\queue_create('s', 1);
$out = tempnam(sys_get_temp_dir(), 'gcd_sy_');
GCD\sync($q, function (string $f) { file_put_contents($f, 'ran'); }, [$out]);
echo file_get_contents($out);
unlink($out);
```

## barrier_async

**Signature**

```text
GCD\barrier_async(GCD\Queue $queue, Closure $task, array $argv = []): void
```

**Parameters** — Same as `async`.

**Return** — Nothing (`null`). Does not wait.

**Errors** — Same as `async`.

**Notes** — The barrier task waits for tasks already in flight when it starts, then runs exclusively: no other task starts while it runs. Tasks submitted after the barrier may start before it — there is no ordering guarantee for later submissions.

**Example**

```php
$q = GCD\queue_create('rw', 4);
$c = new GCD\Atomic(0, 'ba-demo');
for ($i = 0; $i < 4; $i++) {
    GCD\async($q, function (string $n) { GCD\Atomic::open($n)->inc(); }, ['ba-demo']);
}
GCD\barrier_async($q, function (string $n) { GCD\Atomic::open($n)->inc(); }, ['ba-demo']);
GCD\drain();
echo $c->get();   // 5
```

## barrier_sync

**Signature**

```text
GCD\barrier_sync(GCD\Queue $queue, Closure $task, array $argv = []): void
```

**Parameters** — Same as `async`.

**Return** — Nothing (`null`). Blocks until the barrier task completes.

**Errors** — Same as `async`. No deadlock guard (unlike `sync`).

**Notes** — Drains currently in-flight tasks, runs the barrier exclusively, then returns. Useful as a read-write lock pattern (readers via `async`, writer via `barrier_sync`), but later submissions are not ordered behind the barrier.

**Example**

```php
$q = GCD\queue_create('c', 4);
$c = new GCD\Atomic(0, 'bs-demo');
for ($i = 0; $i < 8; $i++) {
    GCD\async($q, function (string $n) {
        GCD\Atomic::open($n)->inc();
        usleep(2000);
    }, ['bs-demo']);
}
GCD\barrier_sync($q, function () {});
echo $c->get();   // 8 — all readers finished before the barrier returned
```

## apply

**Signature**

```text
GCD\apply(int $iterations, GCD\Queue $queue, Closure $task): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$iterations` | `int` | Number of invocations (note: first argument) |
| `$queue` | `GCD\Queue` | Destination queue |
| `$task` | `Closure` | Receives one argument: the iteration index `int $i` (0-based) |

**Return** — Nothing (`null`). Blocks until all iterations complete.

**Errors** — `TypeError` for a non-Queue; task-build errors (`TypeError`/`CaptureException`) stop further dispatching, wait for in-flight iterations, then rethrow.

**Notes** — The index is the only argument; share other data through named containers. Iteration order is not guaranteed on concurrent queues.

**Example**

```php
$q = GCD\queue_create('par', 4);
$s = new GCD\Set('apply-demo');
GCD\apply(5, $q, function (int $i) {
    GCD\Set::open('apply-demo')->add($i);
});
echo $s->count();   // 5
```

## once

**Signature**

```text
GCD\once(string $key, Closure $initializer): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$key` | `string` | Process-wide once key |
| `$initializer` | `Closure` | Zero-argument initializer |

**Return** — Nothing (`null`).

**Errors** — Standard `TypeError` on wrong argument types. Exceptions thrown by the initializer are cleared; the key is still marked done.

**Notes** — Runs synchronously **in the calling thread** (not dispatched to a queue). Concurrent callers with the same key block until the first initializer finishes; exactly once per key per process.

**Example**

```php
$c = new GCD\Atomic(0, 'once-demo');
$q = GCD\queue_create('init', 4);
for ($i = 0; $i < 8; $i++) {
    GCD\async($q, function () {
        GCD\once('boot', function () {
            GCD\Atomic::open('once-demo')->inc();
        });
    });
}
GCD\drain();
echo $c->get();   // 1 — exactly once across 8 concurrent tasks
```