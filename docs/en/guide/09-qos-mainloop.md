# QoS and the main loop

[English](../../en/guide/09-qos-mainloop.md) | [中文](../../zh/guide/09-qos-mainloop.md)

## Choosing a loop model

- **Script / batch:** dispatch work, then [`drain`](../api/loop.md#drain) to wait for it. This is the model most examples use.
- **Long-running service:** keep [sources](05-sources.md) resumed and drive progress with your own loop around `drain`. In v1, [`main`](../api/loop.md#main) is equivalent to `drain()` without a timeout — it returns once pending work finishes, rather than running forever.

## drain

`drain` waits on the pool's pending-task count:

```php
$q = GCD\queue_create('w', 1);
$c = new GCD\Atomic(0, 'd0');
GCD\async($q, function (string $n) { usleep(50000); GCD\Atomic::open($n)->inc(); }, ['d0']);
GCD\drain(0.0);
echo $c->get(), "\n";   // 0 — a poll does not wait
GCD\drain();
echo $c->get();         // 1
```

Timeout semantics: negative waits indefinitely, `0` polls, positive bounds the wait. There is no return value — check your own state. And remember: drain does not wait for *future* source events, only work already pending.

## main in v1

```php
$q = GCD\queue_create('w', 2);
$c = new GCD\Atomic(0, 'main-demo');
GCD\async($q, function (string $n) { GCD\Atomic::open($n)->inc(); }, ['main-demo']);
GCD\main();
echo $c->get();   // 1
```

A never-returning, source-driven `dispatch_main` is not implemented in v1; structure services as `while (...) { GCD\drain($tick); }` with sources resumed.

## QoS classes

Five classes, highest to lowest: `QOS_USER_INTERACTIVE` (4), `QOS_USER_INITIATED` (3), `QOS_DEFAULT` (2), `QOS_UTILITY` (1), `QOS_BACKGROUND` (0). Set them on queues; a queue without its own QoS inherits along its target chain:

```php
$parent = GCD\queue_create('parent', 2);
GCD\queue_set_qos($parent, GCD\QOS_USER_INITIATED);
$child = GCD\queue_create('child', 1);
GCD\queue_set_target($child, $parent);
echo GCD\queue_get_qos($child) === GCD\QOS_USER_INITIATED ? 'inherit-ok' : 'bad';
```

QoS is **recorded only** in v1: values are stored and resolved through target chains, but nothing schedules on them — there is no OS scheduling effect — and `qos_class_self()` always reports `QOS_DEFAULT`. See the [queue reference](../api/queue.md#qos-constants).