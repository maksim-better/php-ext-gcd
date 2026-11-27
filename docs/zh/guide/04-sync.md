# 同步原语

[English](../../en/guide/04-sync.md) | [中文](../../zh/guide/04-sync.md)

## Group：扇出与汇合

group 统计未完成的工作。用 [`group_async`](../api/group.md#group_async) 派发（自动 enter/leave），再用 [`group_wait`](../api/group.md#group_wait) 汇合——crawler 惯用法：

```php
$g = GCD\group_create();
$q = GCD\queue_create('fan', 4);
$c = new GCD\Atomic(0, 'ga-demo');
for ($i = 0; $i < 6; $i++) {
    GCD\group_async($g, $q, function (string $n) {
        GCD\Atomic::open($n)->inc();
    }, ['ga-demo']);
}
var_export(GCD\group_wait($g));   // true —— 6 个全部完成
echo "\n";
echo $c->get();                   // 6
```

`group_wait` 接受秒级超时：`0` 探测，正值限定等待，负值（默认）永久等待。超时返回 `false`。

## 手动 enter/leave

[`group_enter`](../api/group.md#group_enter) / [`group_leave`](../api/group.md#group_leave) 自行管理计数——适用于工作不是派发任务的场景。每个 enter 必须配对 leave，否则 `group_wait` 永不返回：

```php
$g = GCD\group_create();
GCD\group_enter($g);
var_export(GCD\group_wait($g, 0.0));   // false
echo "\n";
GCD\group_leave($g);
var_export(GCD\group_wait($g, 0.0));   // true
```

## 完成通知

[`group_notify`](../api/group.md#group_notify) 在计数归零时把零参回调派发到指定队列——无需阻塞的延续：

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

回调在通知队列的 worker 上运行，结果通过容器共享。group 可跨批次复用；每个 notify 注册触发一次——需要每批通知就每批注册新 notify。

## Semaphore：限制并发

semaphore 限制同时执行某段代码的任务数，与队列宽度无关。创建时命名，worker 才能 open：

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

宽度 8 的队列上跑 8 个任务，但文件峰值计数从不超过 2——限流的是 semaphore，不是队列。`semaphore_wait` 的超时语义与 `group_wait` 相同。