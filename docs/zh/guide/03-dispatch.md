# 派发任务

[English](../../en/guide/03-dispatch.md) | [中文](../../zh/guide/03-dispatch.md)

## async 还是 sync

- [`async`](../api/dispatch.md#async) 提交后立即返回。用于发射后不管的工作；结果通过文件或[共享容器](07-containers.md)收集。
- [`sync`](../api/dispatch.md#sync) 提交并**阻塞**到任务完成。返回 `null`——任务自身的返回值被丢弃。

**死锁保护。** 在任务正在执行的串行队列上再调 `sync` 等于等自己。扩展会检测并抛 `GCD\DeadlockException`，而不是挂死。任务内异常经由[错误处理器](08-errors.md)上报，所以在那里观察：

```php
$q = GCD\queue_create('s', 1);
$hits = new GCD\Atomic(0, 'dl-demo');
GCD\set_error_handler(function (string $msg, string $cls) {
    if ($cls === 'GCD\\DeadlockException') {
        GCD\Atomic::open('dl-demo')->inc();
    }
});
GCD\sync($q, function () {
    GCD\sync(GCD\current_queue(), function () {});   // 会死锁
});
GCD\drain();
echo $hits->get();   // 1 —— 保护触发，没有挂死
```

保护只覆盖 `sync`；`barrier_sync` 没有此检查，切勿在自己的串行队列上 barrier-sync。

## Barrier

barrier 任务等待它启动时已在途的任务结束，然后**独占**执行——运行期间没有其他任务启动。由此得到读写锁模式：

- 读者：[`async`](../api/dispatch.md#async)（可多个并行），
- 写者：[`barrier_sync`](../api/dispatch.md#barrier_sync)（独占，且在所有当前读者之后），
- 或用 [`barrier_async`](../api/dispatch.md#barrier_async) 非阻塞地提交写者。

顺序注意：barrier *之后*提交的任务可能先于它运行——barrier 只等待已在途的工作。

## apply 并行迭代

[`apply`](../api/dispatch.md#apply) 把闭包执行 N 次并阻塞到全部完成。闭包只接收迭代序号——其他数据通过命名容器共享：

```php
$q = GCD\queue_create('par', 4);
$s = new GCD\Set('apply-demo');
GCD\apply(5, $q, function (int $i) {
    GCD\Set::open('apply-demo')->add($i);
});
echo $s->count();   // 5
```

并发队列上不保证迭代顺序。

## 一次性初始化

[`once`](../api/dispatch.md#once) 每进程每键恰好执行一次初始化器，在调用线程同步运行。并发调用者阻塞到完成——在每个任务里调用都安全：

```php
GCD\once('boot', function () {
    GCD\Atomic::open('once-demo')->inc();
});
```

## 等待工作

- [`GCD\drain()`](09-qos-mainloop.md) —— 等待未完成的工作结束（脚本/批处理风格）。
- [`group_wait`](04-sync.md) —— 等待特定一组任务。
- [`GCD\main()`](09-qos-mainloop.md) —— v1 中等价于无超时的 `drain()`；见 [QoS 与主循环](09-qos-mainloop.md)。