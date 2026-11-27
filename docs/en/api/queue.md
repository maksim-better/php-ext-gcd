# Queues & QoS

[English](../../en/api/queue.md) | [中文](../../zh/api/queue.md)

## The Queue handle

`GCD\Queue` is a handle object with no methods. Obtain one from `queue_create`, `global_queue`, `current_queue` or `main_queue`. Direct instantiation (`new GCD\Queue()`) is possible and yields an unlabeled serial queue (width 1), but the factory functions are the intended path.

## queue_create

**Signature**

```text
GCD\queue_create(string $label, int $width = 1): GCD\Queue
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$label` | `string` | Queue name (required); stored, not currently exposed |
| `$width` | `int` | Maximum concurrent tasks; values < 1 are clamped to 1 |

**Return** — A new `GCD\Queue`. Width 1 makes it serial (FIFO); width > 1 makes it concurrent.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — The label is stored but not exposed by any API; it does not register the queue anywhere.

**Example**

```php
$serial = GCD\queue_create('jobs', 1);
$out = tempnam(sys_get_temp_dir(), 'gcd_q_');
GCD\async($serial, function (string $f) { file_put_contents($f, 'done'); }, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## global_queue

**Signature**

```text
GCD\global_queue(int $qos = 0, int $pool_size = 0): GCD\Queue
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$qos` | `int` | Accepted but ignored in v1 |
| `$pool_size` | `int` | Accepted but ignored in v1 |

**Return** — A concurrent queue with label `global` and fixed width 64.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — v1 does not apply the parameters; every call returns a fresh handle with the same fixed configuration.

**Example**

```php
$g = GCD\global_queue();
$out = tempnam(sys_get_temp_dir(), 'gcd_g_');
GCD\async($g, function (string $f) { file_put_contents($f, 'global'); }, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## current_queue

**Signature**

```text
GCD\current_queue(): ?GCD\Queue
```

**Parameters** — None.

**Return** — The calling task's queue **only while running on a width-1 (serial) queue**; `null` on the main thread, outside tasks, and inside tasks on concurrent (width > 1) queues.

**Errors** — None.

**Notes** — Thread-local, and only set for serial queues: the `current_queue()` + `queue_get_specific()` pattern works only inside serial-queue tasks.

**Example**

```php
$q = GCD\queue_create('ctx', 1);
GCD\queue_set_specific($q, 'role', 'worker');
$out = tempnam(sys_get_temp_dir(), 'gcd_cq_');
GCD\async($q, function (string $f) {
    $cur = GCD\current_queue();
    file_put_contents($f, (string) GCD\queue_get_specific($cur, 'role'));
}, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## main_queue

**Signature**

```text
GCD\main_queue(): GCD\Queue
```

**Parameters** — None.

**Return** — The process-wide main queue: a lazily created serial queue (width 1, label `main`).

**Errors** — None.

**Notes** — Singleton: every call returns the same queue. Work dispatched here runs when the loop drains (see [main/drain](loop.md)).

**Example**

```php
$mq = GCD\main_queue();
$out = tempnam(sys_get_temp_dir(), 'gcd_mq_');
GCD\async($mq, function (string $f) { file_put_contents($f, 'main'); }, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## queue_suspend

**Signature**

```text
GCD\queue_suspend(GCD\Queue $queue): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$queue` | `GCD\Queue` | Queue to suspend |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Queue") if the argument is not a Queue.

**Notes** — A suspended queue does not start new tasks; already-running tasks are not interrupted. Call `queue_resume` to continue. Suspension is a flag, not a counter: suspending twice still takes one resume.

**Example**

```php
$q = GCD\queue_create('gate', 1);
$out = tempnam(sys_get_temp_dir(), 'gcd_s_');
GCD\queue_suspend($q);
GCD\async($q, function (string $f) { file_put_contents($f, 'released'); }, [$out]);
GCD\queue_resume($q);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## queue_resume

**Signature**

```text
GCD\queue_resume(GCD\Queue $queue): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$queue` | `GCD\Queue` | Queue to resume |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Queue") if the argument is not a Queue.

**Notes** — Clears the suspension flag and wakes waiting workers. Resuming a queue that is not suspended is harmless.

**Example** — See [`queue_suspend`](#queue_suspend).

## queue_set_specific

**Signature**

```text
GCD\queue_set_specific(GCD\Queue $queue, string $key, mixed $value): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$queue` | `GCD\Queue` | Target queue |
| `$key` | `string` | Context key |
| `$value` | `mixed` | Only null/bool/int/float/string and arrays of those (deep-copied; objects throw SerializationException) |

**Return** — Nothing.

**Errors** — `TypeError` if not a Queue; `GCD\SerializationException` ("value is not serializable") for non-serializable values.

**Notes** — Queue-specific context travels with the queue; tasks read it via `current_queue()` + `queue_get_specific`.

**Example**

```php
$q = GCD\queue_create('cfg', 1);
GCD\queue_set_specific($q, 'retries', 3);
var_export(GCD\queue_get_specific($q, 'retries'));
echo "\n";
var_export(GCD\queue_get_specific($q, 'missing'));
```

## queue_get_specific

**Signature**

```text
GCD\queue_get_specific(GCD\Queue $queue, string $key): mixed
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$queue` | `GCD\Queue` | Source queue |
| `$key` | `string` | Context key |

**Return** — A copy of the stored value, or `null` if the key is absent.

**Errors** — `TypeError` ("expected GCD\Queue") if the argument is not a Queue.

**Notes** — Returns a copy; mutating it does not affect the stored value.

**Example** — See [`queue_set_specific`](#queue_set_specific).

## queue_set_target

**Signature**

```text
GCD\queue_set_target(GCD\Queue $queue, GCD\Queue $target): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$queue` | `GCD\Queue` | Queue whose target is set |
| `$target` | `GCD\Queue` | Target queue |

**Return** — Nothing.

**Errors** — `TypeError` if either argument is not a Queue; `ValueError` ("queue cannot target itself") for self-targeting; `ValueError` ("target queue cycle") if the chain would loop.

**Notes** — QoS resolution walks the target chain (see `queue_get_qos`). Keep the target handle alive: v1 does not retain it, and unsetting the target variable invalidates the chain.

**Example**

```php
$parent = GCD\queue_create('parent', 2);
GCD\queue_set_qos($parent, GCD\QOS_USER_INITIATED);
$child = GCD\queue_create('child', 1);
GCD\queue_set_target($child, $parent);
echo GCD\queue_get_qos($child) === GCD\QOS_USER_INITIATED ? 'inherit-ok' : 'bad';
```

## queue_set_qos

**Signature**

```text
GCD\queue_set_qos(GCD\Queue $queue, int $qos): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$queue` | `GCD\Queue` | Target queue |
| `$qos` | `int` | One of the [QoS constants](#qos-constants) |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Queue") if the argument is not a Queue.

**Notes** — QoS is best-effort in v1 (see `qos_class_self`); the value is recorded and resolved through target chains.

**Example**

```php
$q = GCD\queue_create('q', 2);
GCD\queue_set_qos($q, GCD\QOS_BACKGROUND);
echo GCD\queue_get_qos($q);
```

## queue_get_qos

**Signature**

```text
GCD\queue_get_qos(GCD\Queue $queue): int
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$queue` | `GCD\Queue` | Queue to inspect |

**Return** — The effective QoS class: the first explicitly set value walking the target chain, or `GCD\QOS_DEFAULT` (2) if none is set.

**Errors** — `TypeError` ("expected GCD\Queue") if the argument is not a Queue.

**Notes** — A queue without its own QoS inherits from its target, recursively.

**Example** — See [`queue_set_target`](#queue_set_target).

## qos_class_self

**Signature**

```text
GCD\qos_class_self(): int
```

**Parameters** — None.

**Return** — The QoS class of the calling context. In v1 this always reports `GCD\QOS_DEFAULT` (2).

**Errors** — None.

**Notes** — Linux best-effort: v1 does not derive the class from the executing worker.

**Example**

```php
echo GCD\qos_class_self() >= 0 ? 'ok' : 'bad';
```

## QoS constants

| Constant | Value |
|---|---|
| `GCD\QOS_USER_INTERACTIVE` | 4 |
| `GCD\QOS_USER_INITIATED` | 3 |
| `GCD\QOS_DEFAULT` | 2 |
| `GCD\QOS_UTILITY` | 1 |
| `GCD\QOS_BACKGROUND` | 0 |