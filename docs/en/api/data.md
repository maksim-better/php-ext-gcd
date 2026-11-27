# Data

[English](../../en/api/data.md) | [中文](../../zh/api/data.md)

## The Data class

`GCD\Data` is an immutable byte buffer with one method, `size(): int`. Create instances with `data_create`, `data_concat` or `data_subrange`; read bytes back with `data_create_map`. Buffers are binary-safe. A `GCD\Data` object cannot cross runtimes: passing it through `$argv` throws `GCD\SerializationException`, and containers reject it. To move bytes between threads, pass a string and call `data_create` inside the task.

## data_create

**Signature**

```text
GCD\data_create(string $bytes): GCD\Data
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$bytes` | `string` | Bytes to copy (binary-safe, NUL allowed) |

**Return** — A new immutable `GCD\Data`.

**Errors** — Standard `TypeError` on wrong argument types.

**Notes** — The string is copied; later changes to it do not affect the buffer.

**Example**

```php
$d = GCD\data_create("a\0b");
echo $d->size();   // 3 — binary-safe
```

## data_concat

**Signature**

```text
GCD\data_concat(GCD\Data ...$parts): GCD\Data
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `...$parts` | `GCD\Data` | One or more buffers, concatenated in order |

**Return** — A new buffer holding the concatenation.

**Errors** — Standard `TypeError` if no argument is given.

**Notes** — Arguments that are not `GCD\Data` are silently skipped.

**Example**

```php
$d = GCD\data_concat(GCD\data_create('hello'), GCD\data_create(' '), GCD\data_create('world'));
echo GCD\data_create_map($d);   // hello world
```

## data_subrange

**Signature**

```text
GCD\data_subrange(GCD\Data $data, int $offset, int $length): GCD\Data
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$data` | `GCD\Data` | Source buffer |
| `$offset` | `int` | Start offset; clamped to [0, size] |
| `$length` | `int` | Byte count; negative or overflowing values clamp to the remaining bytes |

**Return** — A new buffer holding the subrange (a copy).

**Errors** — `TypeError` ("expected GCD\Data") for a non-Data.

**Notes** — Clamping never throws: an offset past the end yields an empty buffer.

**Example**

```php
$d = GCD\data_create('hello world');
echo GCD\data_create_map(GCD\data_subrange($d, 6, 5)), "\n";   // world
echo GCD\data_create_map(GCD\data_subrange($d, 6, 99));         // world — clamped
```

## data_create_map

**Signature**

```text
GCD\data_create_map(GCD\Data $data): string
```

**Parameters**

| Name | Type | Description |
|---|---|---|
| `$data` | `GCD\Data` | Buffer to map |

**Return** — The buffer's bytes as a PHP string (binary-safe).

**Errors** — `TypeError` ("expected GCD\Data") for a non-Data.

**Notes** — v1 returns a contiguous copy of the whole buffer; there is no lazy mapping callback.

**Example**

```php
$d = GCD\data_create('hello');
echo GCD\data_create_map($d), "\n";
echo $d->size();   // 5
```