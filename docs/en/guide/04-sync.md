# Synchronization

[English](../../en/guide/04-sync.md) | [中文](../../zh/guide/04-sync.md)

## Groups: fan-out and join

A group counts outstanding work. Dispatch with [`group_async`](../api/group.md#group_async) (auto enter/leave), then [`group_wait`](../api/group.md#group_wait) for the join — the crawler pattern:

```php
$g = GCD\group_create();
$q = GCD\queue_create('fan', 4);
$c = new GCD\Atomic(0, 'ga-demo');
for ($i = 0; $i < 6; $i++) {
    GCD\group_async($g, $q, function (string $n) {
        GCD\Atomic::open($n)->inc();
    }, ['ga-demo']);
}
var_export(GCD\group_wait($g));   // true — all six finished
echo "\n";
echo $c->get();                   // 6
```

`group_wait` accepts a timeout in seconds: `0` polls, a positive value bounds the wait, negative (default) waits forever. It returns `false` on timeout.

## Manual enter/leave

[`group_enter`](../api/group.md#group_enter) / [`group_leave`](../api/group.md#group_leave) manage the count yourself — useful when work is not a dispatched task. Every enter must be paired with a leave, or `group_wait` never returns:

```php
$g = GCD\group_create();
GCD\group_enter($g);
var_export(GCD\group_wait($g, 0.0));   // false
echo "\n";
GCD\group_leave($g);
var_export(GCD\group_wait($g, 0.0));   // true
```

## Notify on completion

[`group_notify`](../api/group.md#group_notify) dispatches a zero-argument callback to a queue you choose when the count hits zero — a continuation without blocking:

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

The callback runs on a worker of the notify queue, so share results through containers. The group is reusable across batches; each notify registration fires once — register a new notify for each batch you want notified.

## Semaphores: limiting concurrency

A semaphore caps how many tasks run a section at once, independent of queue width. Name it at creation so workers can open it:

```php
$path = tempnam(sys_get_temp_dir(), 'gcd_cap_');
file_put_contents($path, '0 0');
GCD\semaphore_create(2, 'cap');
$q = GCD\queue_create('w', 8);
$g = GCD\group_create();
$worker = function (string $p) {
    $s = GCD\semaphore_open('cap');
    GCD\semaphore_wait($s);
    $fp = fopen($p, 'r+'); flock($fp, LOCK_EX);
    $d = explode(' ', stream_get_contents($fp));
    $cur = (int) $d[0] + 1; $max = max((int) $d[1], $cur);
    ftruncate($fp, 0); rewind($fp); fwrite($fp, "$cur $max"); fflush($fp);
    flock($fp, LOCK_UN); fclose($fp);
    usleep(8000);
    $fp = fopen($p, 'r+'); flock($fp, LOCK_EX);
    $d = explode(' ', stream_get_contents($fp));
    $cur = (int) $d[0] - 1;
    ftruncate($fp, 0); rewind($fp); fwrite($fp, "$cur $d[1]"); fflush($fp);
    flock($fp, LOCK_UN); fclose($fp);
    GCD\semaphore_signal($s);
};
for ($i = 0; $i < 8; $i++) {
    GCD\group_async($g, $q, $worker, [$path]);
}
GCD\group_wait($g);
$d = explode(' ', file_get_contents($path));
echo ((int) $d[1] <= 2 && (int) $d[0] == 0) ? 'cap-ok' : 'bad';
unlink($path);
```

Eight tasks on a width-8 queue, but the file's peak counter never exceeds 2 — the semaphore, not the queue, sets the limit. `semaphore_wait` takes the same timeout semantics as `group_wait`.