# Groups

[English](../../en/api/group.md) | [中文](../../zh/api/group.md)

## The Group handle

`GCD\Group` is a handle object with no methods, created by `group_create`. It tracks a count of outstanding work; `group_wait` blocks until the count reaches zero. A group that has fired a `group_notify` callback is single-batch in v1: reusing it can make `group_wait` return early — create a fresh group per batch when using notifications.

## group_create

**Signature**

```text
GCD\group_create(): GCD\Group
```

**Parameters** — None.

**Return** — A new, empty group.

**Errors** — None.

**Notes** — Each call creates an independent group.

**Example**

```php
$g = GCD\group_create();
var_export(GCD\group_wait($g, 0.0));   // true — empty group
```

## group_async

**Signature**

```text
GCD\group_async(GCD\Group $group, GCD\Queue $queue, Closure $task, array $argv = []): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$group` | `GCD\Group` | Group to join |
| `$queue` | `GCD\Queue` | Destination queue |
| `$task` | `Closure` | Same rules as `async` (no captures/statics; try/catch allowed) |
| `$argv` | `array` | Serializable arguments |

**Return** — Nothing (`null`). Does not wait.

**Errors** — `TypeError` ("expected GCD\Group and GCD\Queue") for wrong handle types; task-build errors as in [`async`](dispatch.md#async).

**Notes** — Equivalent to `group_enter` + `async` + automatic `group_leave` when the task finishes (including on failure). The preferred fan-out idiom.

**Example**

```php
$g = GCD\group_create();
$q = GCD\queue_create('fan', 4);
$c = new GCD\Atomic(0, 'ga-demo');
for ($i = 0; $i < 6; $i++) {
    GCD\group_async($g, $q, function (string $n) {
        GCD\Atomic::open($n)->inc();
    }, ['ga-demo']);
}
var_export(GCD\group_wait($g));   // true
echo "\n";
echo $c->get();                   // 6
```

## group_enter

**Signature**

```text
GCD\group_enter(GCD\Group $group): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$group` | `GCD\Group` | Group to enter |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Group") for a non-Group.

**Notes** — Increments the outstanding count. Must be paired with `group_leave`; prefer `group_async` for dispatched tasks.

**Example**

```php
$g = GCD\group_create();
GCD\group_enter($g);
var_export(GCD\group_wait($g, 0.0));   // false — still entered
echo "\n";
GCD\group_leave($g);
var_export(GCD\group_wait($g, 0.0));   // true
```

## group_leave

**Signature**

```text
GCD\group_leave(GCD\Group $group): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$group` | `GCD\Group` | Group to leave |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Group") for a non-Group.

**Notes** — Decrements the count; wakes `group_wait` and fires pending notifications when it reaches zero.

**Example** — See [`group_enter`](#group_enter).

## group_wait

**Signature**

```text
GCD\group_wait(GCD\Group $group, float $timeout = -1): bool
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$group` | `GCD\Group` | Group to wait on |
| `$timeout` | `float` | Seconds: < 0 waits forever (default); 0 polls without blocking; > 0 waits up to that many seconds (fractions allowed) |

**Return** — `true` when the count reached zero; `false` on timeout.

**Errors** — `TypeError` ("expected GCD\Group") for a non-Group.

**Notes** — Waiting on an empty group returns `true` immediately.

**Example** — See [`group_async`](#group_async).

## group_notify

**Signature**

```text
GCD\group_notify(GCD\Group $group, GCD\Queue $queue, Closure $task): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$group` | `GCD\Group` | Group to watch |
| `$queue` | `GCD\Queue` | Queue the notification runs on |
| `$task` | `Closure` | Zero-argument callback; same v1 closure rules |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Group and GCD\Queue"); `TypeError` ("notify must be a user-function Closure"); `GCD\CaptureException` ("notify closure unsupported in v1").

**Notes** — The callback is dispatched to `$queue` when the count reaches zero — it runs on a worker, not the calling thread. Multiple notifications may be registered; share results through containers. After a notification fires, the group is single-batch in v1 (see [the handle note](#the-group-handle)).

**Example**

```php
$g = GCD\group_create();
$q = GCD\queue_create('fan', 2);
$nq = GCD\queue_create('notify', 1);
$c = new GCD\Atomic(0, 'gn-demo');
GCD\group_notify($g, $nq, function () {
    GCD\Atomic::open('gn-demo')->inc();
});
GCD\group_async($g, $q, function () {});
GCD\group_wait($g);
GCD\drain();
echo $c->get();   // 1
```