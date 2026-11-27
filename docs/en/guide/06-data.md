# Data buffers

[English](../../en/guide/06-data.md) | [中文](../../zh/guide/06-data.md)

`GCD\Data` is an immutable, binary-safe byte buffer. A Data object lives in the runtime that created it and cannot be passed to tasks; to move bytes between threads, pass a string (via `$argv`, Map or ConcurrentQueue) and call `data_create` inside the task.

## Creating and reading

[`data_create`](../api/data.md#data_create) copies a string in; [`data_create_map`](../api/data.md#data_create_map) copies the bytes back out; `size()` reports the length:

```php
$d = GCD\data_create('hello');
echo GCD\data_create_map($d), "\n";
echo $d->size();   // 5
```

## Concatenation

[`data_concat`](../api/data.md#data_concat) is variadic and returns a new buffer — nothing is mutated:

```php
$d = GCD\data_concat(GCD\data_create('hello'), GCD\data_create(' '), GCD\data_create('world'));
echo GCD\data_create_map($d);   // hello world
```

## Subranges

[`data_subrange`](../api/data.md#data_subrange) copies a slice; offsets and lengths clamp instead of throwing:

```php
$d = GCD\data_create('hello world');
echo GCD\data_create_map(GCD\data_subrange($d, 6, 5)), "\n";   // world
echo GCD\data_create_map(GCD\data_subrange($d, 6, 99));         // world — clamped
```

## fd I/O with sources

v1 has no `dispatch_io` channel API. File-descriptor I/O is built from two pieces:

1. a [READ/WRITE source](05-sources.md#file-descriptors) that fires while the fd is ready, and
2. data buffers to hold what you read or write.

```text
$src = GCD\source_create(GCD\SOURCE_READ, $fd, 0, $queue);
GCD\source_set_event_handler($src, function () {
    // read($fd) into a string, then GCD\data_create(...) to buffer it
});
GCD\source_resume($src);
```

The handler runs as an ordinary task, so accumulate chunks through [shared containers](07-containers.md) or files. READ/WRITE handles are raw integer fds (see the [source reference](../api/source.md#source-type-constants)).