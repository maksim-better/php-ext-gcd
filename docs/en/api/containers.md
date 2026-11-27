# Shared containers

[English](../../en/api/containers.md) | [中文](../../zh/api/containers.md)

Thread-safe containers for state shared between tasks. Every operation is safe to call from any thread: `Atomic` uses lock-free atomics; `Map`, `Set` and `ConcurrentQueue` use internal mutexes.

**Naming and sharing.** Each class keeps its own process-wide registry, persistent for the life of the process. `new ClassName(..., $name)` (or `new GCD\Atomic($value, $name)`) registers the instance under `$name`; registering an existing name replaces the registry entry while old handles keep the previous instance. `ClassName::open($name)` returns a fresh handle to the **same underlying instance** — this is how worker tasks reach state created on the main thread, since handles themselves are not serializable. `open` throws `ValueError` for an unknown name. Unnamed instances are only usable where the handle can be passed directly.

## Atomic

A shared integer counter.

| Method | Signature |
|---|---|
| `__construct` | `new GCD\Atomic(int $value = 0, string $name = null)` |
| `open` (static) | `GCD\Atomic::open(string $name): GCD\Atomic` |
| `inc` | `$a->inc(int $n = 1): int` |
| `dec` | `$a->dec(int $n = 1): int` |
| `get` | `$a->get(): int` |
| `set` | `$a->set(int $value): void` |

### __construct

**Signature**

```text
new GCD\Atomic(int $value = 0, string $name = null)
```

**Parameters** — Initial value (default 0); optional registry name.

**Return** — A new counter.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — Naming registers the counter for `open`.

**Example**

```php
$a = new GCD\Atomic(10, 'hits');
echo $a->inc(), "\n";                    // 11
echo $a->dec(2), "\n";                   // 9
echo GCD\Atomic::open('hits')->get();    // 9 — same instance
```

### open

**Signature**

```text
GCD\Atomic::open(string $name): GCD\Atomic
```

**Parameters** — Registry name.

**Return** — A handle to the named counter.

**Errors** — `ValueError` ("unknown GCD\Atomic name") if never registered.

**Notes** — Static; same underlying counter as every other handle with that name.

**Example** — See [`__construct`](#__construct).

### inc

**Signature**

```text
$a->inc(int $n = 1): int
```

**Parameters** — Amount to add (default 1).

**Return** — The value **after** the increment.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — Atomic read-modify-write; no lost updates under contention.

**Example** — See [`__construct`](#__construct).

### dec

**Signature**

```text
$a->dec(int $n = 1): int
```

**Parameters** — Amount to subtract (default 1).

**Return** — The value **after** the decrement.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — Atomic; may go negative.

**Example** — See [`__construct`](#__construct).

### get

**Signature**

```text
$a->get(): int
```

**Parameters** — None.

**Return** — The current value.

**Errors** — None.

**Notes** — Atomic load.

**Example** — See [`__construct`](#__construct).

### set

**Signature**

```text
$a->set(int $value): void
```

**Parameters** — New value.

**Return** — Nothing.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — Atomic store; overwrites regardless of the current value.

**Example** — See [`__construct`](#__construct).

## Map

A shared string-keyed map. Values are deep-copied on the way in and out; they must be serializable.

| Method | Signature |
|---|---|
| `__construct` | `new GCD\Map(string $name = null)` |
| `open` (static) | `GCD\Map::open(string $name): GCD\Map` |
| `set` | `$m->set(string $key, mixed $value): void` |
| `get` | `$m->get(string $key): mixed` |
| `has` | `$m->has(string $key): bool` |
| `delete` | `$m->delete(string $key): bool` |
| `count` | `$m->count(): int` |

### __construct

**Signature**

```text
new GCD\Map(string $name = null)
```

**Parameters** — Optional registry name.

**Return** — A new, empty map.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — Naming registers the map for `open`.

**Example**

```php
$m = new GCD\Map('cfg');
$m->set('retries', 3);
var_export($m->get('retries')); echo "\n";      // 3
var_export($m->has('retries')); echo "\n";      // true
var_export($m->delete('retries')); echo "\n";   // true
var_export($m->get('retries'));                 // NULL
```

### open

**Signature**

```text
GCD\Map::open(string $name): GCD\Map
```

**Parameters** — Registry name.

**Return** — A handle to the named map.

**Errors** — `ValueError` ("unknown GCD\Map name") if never registered.

**Notes** — Same underlying map as every other handle with that name.

**Example** — See [`__construct`](#__construct-1).

### set

**Signature**

```text
$m->set(string $key, mixed $value): void
```

**Parameters** — String key; value limited to null/bool/int/float/string and arrays of those (objects throw SerializationException).

**Return** — Nothing.

**Errors** — `GCD\SerializationException` ("value is not serializable") for non-serializable values.

**Notes** — The value is deep-copied; later changes to the original do not affect the stored copy. Overwrites an existing key.

**Example**

```php
$m = new GCD\Map('bad');
try {
    $m->set('fn', function () {});
} catch (GCD\SerializationException $e) {
    echo "rejected\n";
}
```

### get

**Signature**

```text
$m->get(string $key): mixed
```

**Parameters** — String key.

**Return** — A copy of the stored value, or `null` if absent.

**Errors** — None.

**Notes** — Mutating the returned copy does not affect the map.

**Example** — See [`__construct`](#__construct-1).

### has

**Signature**

```text
$m->has(string $key): bool
```

**Parameters** — String key.

**Return** — Whether the key exists.

**Errors** — None.

**Notes** — A stored `null` still counts as present.

**Example** — See [`__construct`](#__construct-1).

### delete

**Signature**

```text
$m->delete(string $key): bool
```

**Parameters** — String key.

**Return** — `true` if a key was removed, `false` if it was absent.

**Errors** — None.

**Notes** — Mutex-protected.

**Example** — See [`__construct`](#__construct-1).

### count

**Signature**

```text
$m->count(): int
```

**Parameters** — None.

**Return** — Number of keys.

**Errors** — None.

**Notes** — Snapshot under the lock.

**Example** — See [`__construct`](#__construct-1).

## Set

A shared set. Values are identified by their **serialized form** (PHP serialization, so serializable objects are accepted). **Closures and resources are unsupported and can crash the process** — `add`, `contains` and `remove` all serialize the value.

| Method | Signature |
|---|---|
| `__construct` | `new GCD\Set(string $name = null)` |
| `open` (static) | `GCD\Set::open(string $name): GCD\Set` |
| `add` | `$s->add(mixed $value): bool` |
| `contains` | `$s->contains(mixed $value): bool` |
| `remove` | `$s->remove(mixed $value): bool` |
| `count` | `$s->count(): int` |

### __construct

**Signature**

```text
new GCD\Set(string $name = null)
```

**Parameters** — Optional registry name.

**Return** — A new, empty set.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — Naming registers the set for `open`.

**Example**

```php
$s = new GCD\Set('seen');
var_export($s->add('x')); echo "\n";       // true — newly added
var_export($s->add('x')); echo "\n";       // false — duplicate
var_export($s->contains('x')); echo "\n";  // true
echo $s->count();                          // 1
```

### open

**Signature**

```text
GCD\Set::open(string $name): GCD\Set
```

**Parameters** — Registry name.

**Return** — A handle to the named set.

**Errors** — `ValueError` ("unknown GCD\Set name") if never registered.

**Notes** — Same underlying set as every other handle with that name.

**Example** — See [`__construct`](#__construct-2).

### add

**Signature**

```text
$s->add(mixed $value): bool
```

**Parameters** — Value to insert (serializable).

**Return** — `true` if newly added, `false` if already present.

**Errors** — Unsupported values (closures, resources) can crash the process; keep values to scalars, arrays and serializable objects.

**Notes** — Dedup key is the serialized value; safe under concurrent adds.

**Example** — See [`__construct`](#__construct-2).

### contains

**Signature**

```text
$s->contains(mixed $value): bool
```

**Parameters** — Value to look up.

**Return** — Whether an equal value is present.

**Errors** — Unsupported values (closures, resources) can crash the process.

**Notes** — Compares serialized forms.

**Example** — See [`__construct`](#__construct-2).

### remove

**Signature**

```text
$s->remove(mixed $value): bool
```

**Parameters** — Value to remove.

**Return** — `true` if removed, `false` if absent.

**Errors** — Unsupported values (closures, resources) can crash the process.

**Notes** — Mutex-protected.

**Example** — See [`__construct`](#__construct-2).

### count

**Signature**

```text
$s->count(): int
```

**Parameters** — None.

**Return** — Number of members.

**Errors** — None.

**Notes** — Snapshot under the lock.

**Example** — See [`__construct`](#__construct-2).

## ConcurrentQueue

A shared multi-producer/multi-consumer FIFO queue. Values are deep-copied and must be serializable.

| Method | Signature |
|---|---|
| `__construct` | `new GCD\ConcurrentQueue(string $name = null)` |
| `open` (static) | `GCD\ConcurrentQueue::open(string $name): GCD\ConcurrentQueue` |
| `push` | `$q->push(mixed $value): void` |
| `pop` | `$q->pop(float $timeout = -1): mixed` |
| `tryPop` | `$q->tryPop(): mixed` |
| `count` | `$q->count(): int` |

### __construct

**Signature**

```text
new GCD\ConcurrentQueue(string $name = null)
```

**Parameters** — Optional registry name.

**Return** — A new, empty queue.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — Naming registers the queue for `open`.

**Example**

```php
$q = new GCD\ConcurrentQueue('jobs');
$q->push('a');
$q->push('b');
echo $q->pop(), "\n";        // a — FIFO
echo $q->tryPop(), "\n";     // b
var_export($q->tryPop());    // NULL — empty
```

### open

**Signature**

```text
GCD\ConcurrentQueue::open(string $name): GCD\ConcurrentQueue
```

**Parameters** — Registry name.

**Return** — A handle to the named queue.

**Errors** — `ValueError` ("unknown GCD\ConcurrentQueue name") if never registered.

**Notes** — Same underlying queue as every other handle with that name.

**Example** — See [`__construct`](#__construct-3).

### push

**Signature**

```text
$q->push(mixed $value): void
```

**Parameters** — Value to enqueue: null/bool/int/float/string and arrays of those (objects throw SerializationException).

**Return** — Nothing.

**Errors** — `GCD\SerializationException` ("value is not serializable") for non-serializable values.

**Notes** — Appends to the tail and wakes one blocked `pop`.

**Example** — See [`__construct`](#__construct-3).

### pop

**Signature**

```text
$q->pop(float $timeout = -1): mixed
```

**Parameters** — Seconds: < 0 waits forever (default); 0 polls without blocking; > 0 waits up to that many seconds.

**Return** — The head value (a copy), or `null` on timeout / empty poll.

**Errors** — None.

**Notes** — Blocks while empty. A stored `null` is indistinguishable from a timeout — avoid pushing `null`.

**Example** — See [`__construct`](#__construct-3).

### tryPop

**Signature**

```text
$q->tryPop(): mixed
```

**Parameters** — None.

**Return** — The head value, or `null` if empty. Never blocks.

**Errors** — None.

**Notes** — Equivalent to `pop(0.0)`.

**Example** — See [`__construct`](#__construct-3).

### count

**Signature**

```text
$q->count(): int
```

**Parameters** — None.

**Return** — Number of queued values.

**Errors** — None.

**Notes** — Snapshot under the lock.

**Example** — See [`__construct`](#__construct-3).