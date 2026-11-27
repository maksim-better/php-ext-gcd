# Semaphores

[English](../../en/api/semaphore.md) | [中文](../../zh/api/semaphore.md)

## The Semaphore handle

`GCD\Semaphore` is a handle object with no methods, created by `semaphore_create`. It holds a counting value: `semaphore_wait` decrements (blocking at zero), `semaphore_signal` increments.

## semaphore_create

**Signature**

```text
GCD\semaphore_create(int $value, string $name = null): GCD\Semaphore
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$value` | `int` | Initial count |
| `$name` | `string\|null` | Optional process-wide registry name |

**Return** — A new semaphore handle.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — Naming registers the semaphore so tasks can reach it via `semaphore_open`; unnamed semaphores are only usable where the handle itself can be passed.

**Example**

```php
$s = GCD\semaphore_create(1);
var_export(GCD\semaphore_wait($s, 0.0));   // true
echo "\n";
var_export(GCD\semaphore_wait($s, 0.0));   // false — exhausted
echo "\n";
GCD\semaphore_signal($s);
var_export(GCD\semaphore_wait($s, 0.0));   // true
```

## semaphore_open

**Signature**

```text
GCD\semaphore_open(string $name): GCD\Semaphore
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$name` | `string` | Registry name given at creation |

**Return** — A handle to the named semaphore (same underlying count).

**Errors** — `ValueError` ("unknown GCD\Semaphore name") if the name was never registered.

**Notes** — The registry is process-wide and persistent; this is how worker tasks reach a semaphore created on the main thread.

**Example**

```php
GCD\semaphore_create(2, 'limiter');
$s = GCD\semaphore_open('limiter');
var_export(GCD\semaphore_wait($s, 0.0));   // true
echo "\n";
try {
    GCD\semaphore_open('nope');
} catch (ValueError $e) {
    echo "unknown name rejected\n";
}
```

## semaphore_wait

**Signature**

```text
GCD\semaphore_wait(GCD\Semaphore $semaphore, float $timeout = -1): bool
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$semaphore` | `GCD\Semaphore` | Semaphore to acquire |
| `$timeout` | `float` | Seconds: < 0 waits forever (default); 0 polls without blocking; > 0 waits up to that many seconds |

**Return** — `true` when acquired (count decremented); `false` on timeout.

**Errors** — `TypeError` ("expected GCD\Semaphore") for a non-Semaphore.

**Notes** — Blocks while the count is zero. Always pair with `semaphore_signal` when the protected work finishes.

**Example** — See [`semaphore_create`](#semaphore_create).

## semaphore_signal

**Signature**

```text
GCD\semaphore_signal(GCD\Semaphore $semaphore): void
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$semaphore` | `GCD\Semaphore` | Semaphore to release |

**Return** — Nothing.

**Errors** — `TypeError` ("expected GCD\Semaphore") for a non-Semaphore.

**Notes** — Increments the count and wakes one waiter.

**Example** — See [`semaphore_create`](#semaphore_create).