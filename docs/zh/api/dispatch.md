# 派发

[English](../../en/api/dispatch.md) | [中文](../../zh/api/dispatch.md)

## async

**签名**

```text
GCD\async(GCD\Queue $queue, Closure $task, array $argv = []): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$queue` | `GCD\Queue` | 目标队列 |
| `$task` | `Closure` | 用户函数闭包；v1 不允许捕获、静态变量；允许 try/catch |
| `$argv` | `array` | 传给闭包的参数；仅 null/bool/int/float/string 及它们的数组——对象抛 SerializationException |

**返回值** —— 无（`null`）。不等待任务。

**错误与异常** —— 非 Queue 抛 `TypeError`（"expected GCD\Queue"）；非用户闭包抛 `TypeError`（"task must be a user-function Closure"）；捕获/静态/try-catch 抛 `GCD\CaptureException`。

**备注** —— 串行队列按 FIFO 入队；并发队列提交到线程池。任务内抛出的异常进入[错误上报层](errors.md)，不会传给调用方。

**示例**

```php
$q = GCD\queue_create('jobs', 2);
$out = tempnam(sys_get_temp_dir(), 'gcd_a_');
GCD\async($q, function (string $f) { file_put_contents($f, 'async'); }, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## sync

**签名**

```text
GCD\sync(GCD\Queue $queue, Closure $task, array $argv = []): void
```

**参数** —— 同 `async`。

**返回值** —— 无（`null`）。阻塞到任务完成；任务自身的返回值被丢弃。

**错误与异常** —— 同 `async`，另加 `GCD\DeadlockException`（"sync dispatch to current serial queue would deadlock"）：目标是当前任务正在其上执行的串行队列时触发。

**备注** —— 死锁保护仅针对 `sync`、仅宽度 1 队列、仅目标 == 当前队列。结果通过文件或共享容器收集。

**示例**

```php
$q = GCD\queue_create('s', 1);
$out = tempnam(sys_get_temp_dir(), 'gcd_sy_');
GCD\sync($q, function (string $f) { file_put_contents($f, 'ran'); }, [$out]);
echo file_get_contents($out);
unlink($out);
```

## barrier_async

**签名**

```text
GCD\barrier_async(GCD\Queue $queue, Closure $task, array $argv = []): void
```

**参数** —— 同 `async`。

**返回值** —— 无（`null`）。不等待。

**错误与异常** —— 同 `async`。

**备注** —— barrier 任务等待它启动时已在途的任务结束，然后独占执行：运行期间没有其他任务启动。barrier 之后提交的任务排在它之后（序号准入）。

**示例**

```php
$q = GCD\queue_create('rw', 4);
$c = new GCD\Atomic(0, 'ba-demo');
for ($i = 0; $i < 4; $i++) {
    GCD\async($q, function (string $n) { GCD\Atomic::open($n)->inc(); }, ['ba-demo']);
}
GCD\barrier_async($q, function (string $n) { GCD\Atomic::open($n)->inc(); }, ['ba-demo']);
GCD\drain();
echo $c->get();   // 5
```

## barrier_sync

**签名**

```text
GCD\barrier_sync(GCD\Queue $queue, Closure $task, array $argv = []): void
```

**参数** —— 同 `async`。

**返回值** —— 无（`null`）。阻塞到 barrier 任务完成。

**错误与异常** —— 同 `async`。没有死锁保护（与 `sync` 不同）。

**备注** —— 排空当前在途任务、独占执行 barrier、然后返回。可用作读写锁模式（读者 `async`，写者 `barrier_sync`）；后续提交排在 barrier 之后。

**示例**

```php
$q = GCD\queue_create('c', 4);
$c = new GCD\Atomic(0, 'bs-demo');
for ($i = 0; $i < 8; $i++) {
    GCD\async($q, function (string $n) {
        GCD\Atomic::open($n)->inc();
        usleep(2000);
    }, ['bs-demo']);
}
GCD\barrier_sync($q, function () {});
echo $c->get();   // 8 —— barrier 返回前所有读者已完成
```

## apply

**签名**

```text
GCD\apply(int $iterations, GCD\Queue $queue, Closure $task): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$iterations` | `int` | 调用次数（注意：第一个参数） |
| `$queue` | `GCD\Queue` | 目标队列 |
| `$task` | `Closure` | 只接收一个参数：迭代序号 `int $i`（从 0 起） |

**返回值** —— 无（`null`）。阻塞到全部迭代完成。

**错误与异常** —— 非 Queue 抛 `TypeError`；任务构建错误（`TypeError`/`CaptureException`）会停止后续派发、等待在途迭代后重新抛出。

**备注** —— 序号是唯一参数；其他数据通过命名容器共享。并发队列上不保证迭代顺序。

**示例**

```php
$q = GCD\queue_create('par', 4);
$s = new GCD\Set('apply-demo');
GCD\apply(5, $q, function (int $i) {
    GCD\Set::open('apply-demo')->add($i);
});
echo $s->count();   // 5
```

## once

**签名**

```text
GCD\once(string $key, Closure $initializer): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$key` | `string` | 进程级 once 键 |
| `$initializer` | `Closure` | 零参初始化器 |

**返回值** —— 无（`null`）。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。初始化器抛出的异常会被清除；键仍标记为已完成。

**备注** —— 在**调用线程**同步执行（不派发到队列）。同键的并发调用者阻塞到首个初始化器完成；每进程每键恰好一次。

**示例**

```php
$c = new GCD\Atomic(0, 'once-demo');
$q = GCD\queue_create('init', 4);
for ($i = 0; $i < 8; $i++) {
    GCD\async($q, function () {
        GCD\once('boot', function () {
            GCD\Atomic::open('once-demo')->inc();
        });
    });
}
GCD\drain();
echo $c->get();   // 1 —— 8 个并发任务中恰好一次
```