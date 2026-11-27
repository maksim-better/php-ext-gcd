# Shared containers

[English](../../en/guide/07-containers.md) | [中文](../../zh/guide/07-containers.md)

Ordinary PHP variables are per-runtime: tasks cannot share them. The four containers are the cross-thread toolbox.

## Naming and sharing

Handles are not serializable, so a task cannot receive a container through `$argv`. Instead, **name** the container on the main thread and **open** it by name inside tasks — every open returns the same underlying instance:

```php
$c = new GCD\Atomic(0, 'shared');
$q = GCD\queue_create('w', 4);
$g = GCD\group_create();
for ($i = 0; $i < 8; $i++) {
    GCD\group_async($g, $q, function () {
        GCD\Atomic::open('shared')->inc();
    });
}
GCD\group_wait($g);
echo $c->get();   // 8 — every worker incremented the same counter
```

The registry is process-wide and persistent. See the [container reference](../api/containers.md) for the full rules.

## Atomic: counters without lost updates

`inc`/`dec` are atomic read-modify-write operations returning the new value. Even heavy contention loses no updates:

```php
$c = new GCD\Atomic(0, 'stress');
$q = GCD\queue_create('w', 8);
$g = GCD\group_create();
for ($i = 0; $i < 8; $i++) {
    GCD\group_async($g, $q, function () {
        $a = GCD\Atomic::open('stress');
        for ($j = 0; $j < 1000; $j++) { $a->inc(); }
    });
}
GCD\group_wait($g);
echo $c->get();   // 8000
```

## Map and Set

`Map` stores serializable values under string keys; `Set` deduplicates by serialized form. Both are mutex-protected:

```php
$s = new GCD\Set('dedup');
$q = GCD\queue_create('w', 4);
$g = GCD\group_create();
for ($i = 0; $i < 8; $i++) {
    GCD\group_async($g, $q, function (int $n) {
        GCD\Set::open('dedup')->add($n % 4);
    }, [$i]);
}
GCD\group_wait($g);
echo $s->count();   // 4 — eight concurrent adds, four distinct values
```

`Map::set` and `ConcurrentQueue::push` accept only scalars and arrays (objects throw `GCD\SerializationException`). `Set` uses PHP serialization: serializable objects work, but closures and resources throw `GCD\SerializationException`.

## ConcurrentQueue: producer–consumer

`push` appends and wakes a blocked `pop`; `tryPop` never blocks. A minimal pipeline:

```php
$q = new GCD\ConcurrentQueue('pipe');
$wq = GCD\queue_create('workers', 2);
$g = GCD\group_create();
GCD\group_async($g, $wq, function () {
    $pipe = GCD\ConcurrentQueue::open('pipe');
    for ($i = 0; $i < 5; $i++) { $pipe->push($i); }
});
GCD\group_wait($g);
$sum = 0;
while (($v = $q->tryPop()) !== null) { $sum += $v; }
echo $sum;   // 10
```

For long-running consumers, `pop()` blocks until a value arrives (with the usual timeout semantics); avoid pushing `null`, which is indistinguishable from a timeout.