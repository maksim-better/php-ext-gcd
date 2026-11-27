# Main loop

[English](../../en/api/loop.md) | [中文](../../zh/api/loop.md)

## drain

**Signature**

```text
GCD\drain(float $timeout = -1): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$timeout` | `float` | Seconds: < 0 waits until no pending work remains (default); 0 returns immediately; > 0 waits up to that many seconds |

**Return** — Nothing. There is no success indicator — inspect your own state (containers, files) to know whether work finished.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — Waits on the pool's pending-task count. It does **not** wait for future events: once pending work hits zero, drain returns even if a source might fire later. Give sources time to dispatch before draining (see the [source guide](../guide/05-sources.md)).

**Example**

```php
$q = GCD\queue_create('w', 4);
$c = new GCD\Atomic(0, 'mc');
for ($i = 0; $i < 8; $i++) {
    GCD\async($q, function (string $n) {
        GCD\Atomic::open($n)->inc();
        usleep(1000);
    }, ['mc']);
}
GCD\drain(5.0);
echo $c->get();   // 8
```

## main

**Signature**

```text
GCD\main(): void
```

**Parameters** — None.

**Return** — Nothing.

**Errors** — None.

**Notes** — In v1, `main()` is equivalent to `drain()` with no timeout: it waits for pending work, then returns. It does not yet run a never-returning, source-driven loop; for periodic work keep sources resumed and call `drain()` in your own loop.

**Example**

```php
$q = GCD\queue_create('w', 2);
$c = new GCD\Atomic(0, 'main-demo');
GCD\async($q, function (string $n) { GCD\Atomic::open($n)->inc(); }, ['main-demo']);
GCD\main();
echo $c->get();   // 1
```