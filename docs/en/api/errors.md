# Errors

[English](../../en/api/errors.md) | [中文](../../zh/api/errors.md)

Exceptions thrown **inside a task** never reach the dispatching caller — this includes the `sync` deadlock guard, which can only fire inside a task. They go through a reporting layer: the queue's error handler if set, otherwise the global handler, otherwise the exception is silently discarded. Handlers receive `(string $message, string $className)` and run in the failing task's worker context; their return value is ignored and exceptions they throw are themselves discarded. For `Error`-derived exceptions (including all three GCD exceptions) `$message` arrives **empty** in v1 — route by `$className`.

Exceptions thrown **at dispatch time** (bad handles, capture checks, serialization checks) are ordinary synchronous exceptions — catch them in the calling script.

## set_error_handler

**Signature**

```text
GCD\set_error_handler(Closure $handler): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$handler` | `Closure` | `function (string $message, string $className)`; v1 closure rules (no captures/statics; try/catch allowed). An unsupported closure is silently not installed (no exception) |

**Return** — Nothing.

**Errors** — `TypeError` ("handler must be a user-function Closure") for a non-user closure.

**Notes** — Process-global fallback for every queue without its own handler. Replaces any previous global handler. Runs in the worker, so record results through shared containers. A handler that captures variables or declares statics is silently not installed in v1 — verify your handler actually runs.

**Example**

```php
$errors = new GCD\Atomic(0, 'err');
GCD\set_error_handler(function (string $msg, string $cls) {
    GCD\Atomic::open('err')->inc();
});
$q = GCD\queue_create('w', 4);
$g = GCD\group_create();
for ($i = 0; $i < 5; $i++) {
    GCD\group_async($g, $q, function () { throw new RuntimeException('boom'); });
}
GCD\group_wait($g);
GCD\drain();
echo $errors->get();   // 5
```

## queue_set_error_handler

**Signature**

```text
GCD\queue_set_error_handler(GCD\Queue $queue, Closure $handler): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$queue` | `GCD\Queue` | Queue whose handler is set |
| `$handler` | `Closure` | Same shape and rules as the global handler |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Queue"); `TypeError` ("handler must be a user-function Closure"); `GCD\CaptureException` ("handler closure unsupported in v1").

**Notes** — Overrides the global handler for tasks on this queue only. Replaces any previous queue handler.

**Example**

```php
$global = new GCD\Atomic(0, 'g-err');
$local = new GCD\Atomic(0, 'q-err');
GCD\set_error_handler(function (string $m, string $c) { GCD\Atomic::open('g-err')->inc(); });
$q = GCD\queue_create('w', 1);
GCD\queue_set_error_handler($q, function (string $m, string $c) { GCD\Atomic::open('q-err')->inc(); });
GCD\sync($q, function () { throw new RuntimeException('x'); });
GCD\drain();
echo $global->get(), "\n";   // 0 — queue handler took precedence
echo $local->get();          // 1
```

## CaptureException

**Inherits** — `Error`.

**Thrown when** — A closure cannot be copied to persistent storage: it captures variables or declares statics (try/catch is actually accepted). Raised at dispatch time by `async`/`sync`/`barrier_*`/`apply`/`group_async`, and when registering notify/source/queue-error handlers. The global `set_error_handler` does not throw — an unsupported closure is silently not installed.

**Message** — `"closure captures variables or uses statics/try-catch (unsupported in v1)"` (handler variants use their own wording). The message mentions try/catch, but v1 accepts it.

**Example**

```php
$q = GCD\queue_create('w', 1);
try {
    GCD\async($q, function () { static $x = 0; });
} catch (GCD\CaptureException $e) {
    echo "capture rejected\n";
}
```

## SerializationException

**Inherits** — `Error`.

**Thrown when** — A value cannot be deep-copied to persistent storage: `queue_set_specific`, `Map::set`, `ConcurrentQueue::push`.

**Message** — `"value is not serializable"`.

**Example** — See [Map::set](containers.md#set-1).

## DeadlockException

**Inherits** — `Error`.

**Thrown when** — `sync` targets the width-1 queue the calling task is currently executing on. Raised inside the task, so it surfaces through the [reporting layer](#set_error_handler), not at the outer call site.

**Message** — `"sync dispatch to current serial queue would deadlock"`.

**Example** — See the [dispatch guide](../guide/03-dispatch.md#async-or-sync).