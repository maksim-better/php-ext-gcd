# Event sources

[English](../../en/api/source.md) | [中文](../../zh/api/source.md)

## The Source handle

`GCD\Source` is a handle object with no methods, created by `source_create`. A source watches something (time, a signal, a child process, a file descriptor, a data counter) and dispatches a handler closure to a queue when it fires. Sources start suspended: call `source_resume` to arm them.

## source_create

**Signature**

```text
GCD\source_create(int $type, int $handle = 0, int $mask = 0, GCD\Queue $queue): GCD\Source
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$type` | `int` | One of the [source type constants](#source-type-constants) |
| `$handle` | `int` | Type-dependent: signal number (SIGNAL), child pid (PROC), raw fd (READ/WRITE); unused for TIMER/DATA_ADD |
| `$mask` | `int` | Accepted but not applied in v1 |
| `$queue` | `GCD\Queue` | Optional — omit the argument (explicit `null` throws TypeError); queue the handler is dispatched to |

**Return** — A new, suspended source.

**Errors** — `ValueError` ("unsupported source type in v1") for an unknown type.

**Notes** — The handler only fires when **both** a handler and a queue are set. Handler arguments: none for TIMER/SIGNAL/READ/WRITE/PROC; DATA_ADD receives the coalesced sum as one `int` argument.

**Example**

```php
$q = GCD\queue_create('w', 2);
$c = new GCD\Atomic(0, 'src-timer');
$src = GCD\source_create(GCD\SOURCE_TIMER, 0, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open('src-timer')->inc();
});
GCD\source_set_timer($src, 0.02, 0.02, 0);
GCD\source_resume($src);
usleep(150000);
GCD\source_cancel($src);
GCD\drain(1.0);
$n = $c->get();
echo ($n >= 3 && $n <= 12) ? 'timer-ok' : "bad:$n";
```

## source_set_event_handler

**Signature**

```text
GCD\source_set_event_handler(GCD\Source $source, Closure $handler): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$source` | `GCD\Source` | Source to configure |
| `$handler` | `Closure` | User-function closure; same v1 rules (no captures/statics; try/catch allowed) |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Source"); `TypeError` ("handler must be a user-function Closure"); `GCD\CaptureException` ("handler unsupported in v1").

**Notes** — Replaces any previous handler. The handler runs as a task on the source's queue, not in the watcher thread.

**Example** — See [`source_create`](#source_create).

## source_set_timer

**Signature**

```text
GCD\source_set_timer(GCD\Source $source, float $start, float $interval, float $leeway = 0): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$source` | `GCD\Source` | A TIMER source |
| `$start` | `float` | Seconds before the first fire |
| `$interval` | `float` | Seconds between fires; omitted or <= 0 means fire once (one-shot) |
| `$leeway` | `float` | Accepted but not applied in v1 |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Source") for a non-Source.

**Notes** — Applies to TIMER sources. The watcher sleeps `start`, then repeats fire/sleep(`interval`) until cancelled.

**Example** — See [`source_create`](#source_create).

## source_resume

**Signature**

```text
GCD\source_resume(GCD\Source $source): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$source` | `GCD\Source` | Source to arm |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Source") for a non-Source.

**Notes** — Starts the watcher thread; resuming an already-running source is a no-op. For SIGNAL sources the signal is added to a process-wide mask applied to every worker and source thread, so delivery goes to `sigwait` regardless of when the pool was started. Source signals stay blocked for the life of the process.

**Example**

```php
$q = GCD\queue_create('w', 2);
$c = new GCD\Atomic(0, 'src-sig');
$src = GCD\source_create(GCD\SOURCE_SIGNAL, SIGUSR1, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open('src-sig')->inc();
});
GCD\source_resume($src);
usleep(50000);
posix_kill(getmypid(), SIGUSR1);
usleep(50000);
posix_kill(getmypid(), SIGUSR1);
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $c->get();   // 2
```

## source_cancel

**Signature**

```text
GCD\source_cancel(GCD\Source $source): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$source` | `GCD\Source` | Source to stop |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Source") for a non-Source.

**Notes** — Stops the watcher and waits for its thread to exit; cancelling a stopped source is a no-op. Handlers already dispatched still run — drain afterwards if you need their results.

**Example** — See [`source_resume`](#source_resume).

## source_merge_data

**Signature**

```text
GCD\source_merge_data(GCD\Source $source, int $value): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$source` | `GCD\Source` | A DATA_ADD source |
| `$value` | `int` | Amount to add to the accumulator |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Source") for a non-Source.

**Notes** — Thread-safe add; wakes the watcher. Multiple merges before the watcher wakes coalesce into one handler call carrying the sum.

**Example**

```php
$q = GCD\queue_create('w', 1);
$sum = new GCD\Atomic(0, 'src-data');
$src = GCD\source_create(GCD\SOURCE_DATA_ADD, 0, 0, $q);
GCD\source_set_event_handler($src, function (int $batch) {
    GCD\Atomic::open('src-data')->set($batch);
});
GCD\source_resume($src);
GCD\source_merge_data($src, 12);
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $sum->get();   // 12
```

## source_get_data

**Signature**

```text
GCD\source_get_data(GCD\Source $source): int
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$source` | `GCD\Source` | A DATA_ADD source |

**Return** — The current accumulator value. The watcher resets it to zero when it dispatches a batch.

**Errors** — `TypeError` ("expected GCD\Source") for a non-Source.

**Notes** — Reading does not consume the accumulator.

**Example**

```php
$src = GCD\source_create(GCD\SOURCE_DATA_ADD);
GCD\source_merge_data($src, 5);
echo GCD\source_get_data($src);   // 5 — not resumed, nothing consumed
```

## Source type constants

| Constant | Value | Watches | Handler argument |
|---|---|---|---|
| `GCD\SOURCE_TIMER` | 1 | start/interval schedule | none |
| `GCD\SOURCE_SIGNAL` | 2 | signal number (`$handle`) | none |
| `GCD\SOURCE_READ` | 3 | fd readable (`$handle`) | none |
| `GCD\SOURCE_WRITE` | 4 | fd writable (`$handle`) | none |
| `GCD\SOURCE_PROC` | 5 | child exit (`$handle` = pid) | none |
| `GCD\SOURCE_DATA_ADD` | 6 | accumulator nonzero | `int` coalesced sum |

READ/WRITE take a raw integer file descriptor, which plain PHP userland cannot extract from a stream; use them from code that owns real fds (e.g. via FFI). The PROC example uses ext-pcntl and the SIGNAL example ext-posix; the source types themselves only need a pid / signal number.