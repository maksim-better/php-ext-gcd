# Error handling

[English](../../en/guide/08-errors.md) | [中文](../../zh/guide/08-errors.md)

## Two kinds of failure

- **Dispatch-time exceptions** — bad handle types, `CaptureException`, `SerializationException`, the `sync` deadlock guard. These are thrown synchronously to the caller; catch them with ordinary try/catch in your script.
- **In-task exceptions** — anything a task throws while running. These never reach the caller; they go through the reporting layers below.

## The reporting layers

When a task finishes with an exception, the extension:

1. calls the **queue's handler** (`queue_set_error_handler`) if one is set,
2. otherwise the **global handler** (`set_error_handler`),
3. otherwise **discards the exception silently**.

Handlers receive `(string $message, string $className)`, run in the failing task's worker, and cannot throw usefully — their own exceptions are discarded too. Record outcomes through shared containers:

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
echo $errors->get();   // 5 — every failure reported
```

## Custom handlers

A queue-level handler overrides the global one for that queue only:

```php
$global = new GCD\Atomic(0, 'g-err');
$local = new GCD\Atomic(0, 'q-err');
GCD\set_error_handler(function (string $m, string $c) { GCD\Atomic::open('g-err')->inc(); });
$q = GCD\queue_create('w', 1);
GCD\queue_set_error_handler($q, function (string $m, string $c) { GCD\Atomic::open('q-err')->inc(); });
GCD\sync($q, function () { throw new RuntimeException('x'); });
GCD\drain();
echo $global->get(), "\n";   // 0
echo $local->get();          // 1
```

Handler closures follow the same v1 rules as tasks: no captures, statics or try/catch.

## Catching dispatch-time exceptions

```php
$q = GCD\queue_create('w', 1);
try {
    GCD\async($q, function () { static $x = 0; });
} catch (GCD\CaptureException $e) {
    echo "capture rejected\n";
}
```

The deadlock guard fires inside a task, so observe it through a handler — the [dispatch guide](03-dispatch.md#async-or-sync) shows the full pattern. All three exception classes extend `Error`; see the [error reference](../api/errors.md).