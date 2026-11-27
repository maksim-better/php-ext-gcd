# Event sources

[English](../../en/guide/05-sources.md) | [中文](../../zh/guide/05-sources.md)

A source watches something and dispatches a handler to a queue when it fires. The lifecycle is always: [`source_create`](../api/source.md#source_create) → [`source_set_event_handler`](../api/source.md#source_set_event_handler) → (type-specific setup) → [`source_resume`](../api/source.md#source_resume) → … → [`source_cancel`](../api/source.md#source_cancel). Handlers are ordinary tasks: same v1 closure rules, results through containers.

## Timers

```php
$q = GCD\queue_create('w', 2);
$c = new GCD\Atomic(0, 'src-timer');
$src = GCD\source_create(GCD\SOURCE_TIMER, 0, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open('src-timer')->inc();
});
GCD\source_set_timer($src, 0.02, 0.02, 0);   // first fire in 20ms, then every 20ms
GCD\source_resume($src);
usleep(150000);
GCD\source_cancel($src);
GCD\drain(1.0);
$n = $c->get();
echo ($n >= 3 && $n <= 12) ? 'timer-ok' : "bad:$n";
```

Always pass the interval explicitly: omitted, it is indeterminate (often a tight fire loop). `leeway` is accepted but not applied in v1.

## Signals

SIGNAL sources deliver via `sigwait` on a dedicated thread. `source_resume` blocks the signal in the calling thread **only** — workers already created by earlier dispatches do not inherit the mask, and an incoming signal can then terminate the process. **Arm signal sources before your first dispatch.** The example uses ext-posix:

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
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $c->get();   // 1
```

## Child processes

PROC sources poll `waitpid` (10ms) and fire once when the child exits. The example uses ext-pcntl:

```php
$pid = pcntl_fork();
if ($pid === 0) { usleep(50000); exit(0); }
$q = GCD\queue_create('w', 1);
$c = new GCD\Atomic(0, 'src-proc');
$src = GCD\source_create(GCD\SOURCE_PROC, $pid, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open('src-proc')->inc();
});
GCD\source_resume($src);
usleep(150000);
GCD\drain(1.0);
GCD\source_cancel($src);
echo $c->get();   // 1
```

## File descriptors

READ/WRITE sources poll a raw fd (level-triggered: the handler fires repeatedly while the fd stays ready). The handle is an integer descriptor — plain PHP userland cannot extract one from a stream, so these types serve code that owns real fds (e.g. through FFI):

```text
$src = GCD\source_create(GCD\SOURCE_READ, $fd, 0, $queue);
GCD\source_set_event_handler($src, function () { /* read($fd) ... */ });
GCD\source_resume($src);
```

For buffered data manipulation see [data buffers](06-data.md).

## Coalescing data notifications

DATA_ADD turns many small notifications into few handler calls: producers [`source_merge_data`](../api/source.md#source_merge_data) amounts into an accumulator; the watcher drains it to zero and dispatches the sum as the handler's single `int` argument.

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

[`source_get_data`](../api/source.md#source_get_data) reads the accumulator without consuming it.