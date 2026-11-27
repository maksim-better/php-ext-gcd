# Dispatching work

[English](../../en/guide/03-dispatch.md) | [中文](../../zh/guide/03-dispatch.md)

## async or sync

- [`async`](../api/dispatch.md#async) submits and returns immediately. Use it for fire-and-forget work; collect results through files or [shared containers](07-containers.md).
- [`sync`](../api/dispatch.md#sync) submits and **blocks** until the task finishes. It returns `null` — the task's return value is discarded.

**The deadlock guard.** Calling `sync` on the serial queue your task is currently running on would wait for yourself. The extension detects this and throws `GCD\DeadlockException` instead of hanging. Exceptions inside tasks surface through the [error handler](08-errors.md), so observe it there:

```php
$q = GCD\queue_create('s', 1);
$hits = new GCD\Atomic(0, 'dl-demo');
GCD\set_error_handler(function (string $msg, string $cls) {
    if ($cls === 'GCD\\DeadlockException') {
        GCD\Atomic::open('dl-demo')->inc();
    }
});
GCD\sync($q, function () {
    GCD\sync(GCD\current_queue(), function () {});   // would deadlock
});
GCD\drain();
echo $hits->get();   // 1 — guard fired, no hang
```

The guard covers `sync` only; `barrier_sync` has no such check, so never barrier-sync onto your own serial queue.

## Barriers

A barrier task waits for in-flight tasks on the queue, then runs **exclusively** — nothing else starts while it runs. That gives a read-write lock pattern:

- readers: [`async`](../api/dispatch.md#async) (many at once),
- writer: [`barrier_sync`](../api/dispatch.md#barrier_sync) (alone, after all readers),
- or [`barrier_async`](../api/dispatch.md#barrier_async) to submit the writer without blocking.

## Parallel iteration with apply

[`apply`](../api/dispatch.md#apply) runs a closure N times and blocks until all iterations finish. The closure receives only the iteration index — share everything else through named containers:

```php
$q = GCD\queue_create('par', 4);
$s = new GCD\Set('apply-demo');
GCD\apply(5, $q, function (int $i) {
    GCD\Set::open('apply-demo')->add($i);
});
echo $s->count();   // 5
```

On concurrent queues, iteration order is not guaranteed.

## One-time initialization

[`once`](../api/dispatch.md#once) runs an initializer exactly once per key per process, synchronously in the calling thread. Concurrent callers block until it finishes — safe to call from every task:

```php
GCD\once('boot', function () {
    GCD\Atomic::open('once-demo')->inc();
});
```

## Waiting for work

- [`GCD\drain()`](09-qos-mainloop.md) — wait until pending work finishes (script/batch style).
- [`group_wait`](04-sync.md) — wait for a specific set of tasks.
- [`GCD\main()`](09-qos-mainloop.md) — the callback-driven loop for long-running services.