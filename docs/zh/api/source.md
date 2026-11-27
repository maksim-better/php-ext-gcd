# 事件源

[English](../../en/api/source.md) | [中文](../../zh/api/source.md)

## Source 句柄

`GCD\Source` 是无方法的句柄对象，由 `source_create` 创建。源监视某种事物（时间、信号、子进程、文件描述符、数据计数），触发时把处理器闭包派发到队列。源创建后处于挂起状态：调用 `source_resume` 启动。

## source_create

**签名**

```text
GCD\source_create(int $type, int $handle = 0, int $mask = 0, GCD\Queue $queue): GCD\Source
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$type` | `int` | [源类型常量](#源类型常量)之一 |
| `$handle` | `int` | 随类型而定：信号号（SIGNAL）、子进程 pid（PROC）、原生 fd（READ/WRITE）；TIMER/DATA_ADD 不用 |
| `$mask` | `int` | v1 接受但不应用 |
| `$queue` | `GCD\Queue` | 可选——省略该参数（显式传 `null` 抛 TypeError）；处理器派发到的队列 |

**返回值** —— 新建的、挂起的源。

**错误与异常** —— 未知类型抛 `ValueError`（"unsupported source type in v1"）。

**备注** —— 只有**同时**设置了处理器和队列，处理器才会触发。处理器参数：TIMER/SIGNAL/READ/WRITE/PROC 无参；DATA_ADD 接收合并后的总和（一个 `int`）。

**示例**

```php
$q = GCD\queue_create('w', 2);
$c = new GCD\Atomic(0, 'src-timer');
$src = GCD\source_create(GCD\SOURCE_TIMER, 0, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open('src-timer')->inc();
});
GCD\source_set_timer($src, 0.02, 0.02, 0);
GCD\source_resume($src);
usleep(150000);
GCD\source_cancel($src);
GCD\drain(1.0);
$n = $c->get();
echo ($n >= 3 && $n <= 12) ? 'timer-ok' : "bad:$n";
```

## source_set_event_handler

**签名**

```text
GCD\source_set_event_handler(GCD\Source $source, Closure $handler): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$source` | `GCD\Source` | 要配置的源 |
| `$handler` | `Closure` | 用户函数闭包；同样的 v1 规则（无捕获/静态；允许 try/catch） |

**返回值** —— 无。

**错误与异常** —— `TypeError`（"expected GCD\Source"）；`TypeError`（"handler must be a user-function Closure"）；`GCD\CaptureException`（"handler unsupported in v1"）。

**备注** —— 替换先前的处理器。处理器作为任务在源的队列上运行，而非监视线程。

**示例** —— 见 [`source_create`](#source_create)。

## source_set_timer

**签名**

```text
GCD\source_set_timer(GCD\Source $source, float $start, float $interval, float $leeway = 0): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$source` | `GCD\Source` | TIMER 源 |
| `$start` | `float` | 首次触发前的秒数 |
| `$interval` | `float` | 触发间隔秒数——务必显式传入：省略时其值不确定（常表现为紧密触发循环） |
| `$leeway` | `float` | v1 接受但不应用 |

**返回值** —— 无。

**错误与异常** —— 非 Source 抛 `TypeError`（"expected GCD\Source"）。

**备注** —— 适用于 TIMER 源。监视线程先睡 `start`，然后循环 触发/睡(`interval`) 直到取消。

**示例** —— 见 [`source_create`](#source_create)。

## source_resume

**签名**

```text
GCD\source_resume(GCD\Source $source): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$source` | `GCD\Source` | 要启动的源 |

**返回值** —— 无。

**错误与异常** —— 非 Source 抛 `TypeError`（"expected GCD\Source"）。

**备注** —— 启动监视线程；重复 resume 无副作用。SIGNAL 源只在调用线程阻塞该信号：先前派发已创建的 worker 不继承掩码，此时收到信号可能终止进程。请在首次派发之前启动信号源。

**示例**

```php
$q = GCD\queue_create('w', 2);
$c = new GCD\Atomic(0, 'src-sig');
$src = GCD\source_create(GCD\SOURCE_SIGNAL, SIGUSR1, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open('src-sig')->inc();
});
GCD\source_resume($src);
usleep(50000);
posix_kill(getmypid(), SIGUSR1);
usleep(50000);
posix_kill(getmypid(), SIGUSR1);
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $c->get();   // 2
```

## source_cancel

**签名**

```text
GCD\source_cancel(GCD\Source $source): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$source` | `GCD\Source` | 要停止的源 |

**返回值** —— 无。

**错误与异常** —— 非 Source 抛 `TypeError`（"expected GCD\Source"）。

**备注** —— 停止监视并等待其线程退出；取消已停止的源无副作用。已派发的处理器仍会执行——需要结果时随后 drain。

**示例** —— 见 [`source_resume`](#source_resume)。

## source_merge_data

**签名**

```text
GCD\source_merge_data(GCD\Source $source, int $value): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$source` | `GCD\Source` | DATA_ADD 源 |
| `$value` | `int` | 累加量 |

**返回值** —— 无。

**错误与异常** —— 非 Source 抛 `TypeError`（"expected GCD\Source"）。

**备注** —— 线程安全累加并唤醒监视线程。监视线程醒来前的多次 merge 合并为一次处理器调用，参数为总和。

**示例**

```php
$q = GCD\queue_create('w', 1);
$sum = new GCD\Atomic(0, 'src-data');
$src = GCD\source_create(GCD\SOURCE_DATA_ADD, 0, 0, $q);
GCD\source_set_event_handler($src, function (int $batch) {
    GCD\Atomic::open('src-data')->set($batch);
});
GCD\source_resume($src);
GCD\source_merge_data($src, 12);
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $sum->get();   // 12
```

## source_get_data

**签名**

```text
GCD\source_get_data(GCD\Source $source): int
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$source` | `GCD\Source` | DATA_ADD 源 |

**返回值** —— 当前累加值。监视线程派发批次时将其清零。

**错误与异常** —— 非 Source 抛 `TypeError`（"expected GCD\Source"）。

**备注** —— 读取不消费累加值。

**示例**

```php
$src = GCD\source_create(GCD\SOURCE_DATA_ADD);
GCD\source_merge_data($src, 5);
echo GCD\source_get_data($src);   // 5 —— 未 resume，无人消费
```

## 源类型常量

| 常量 | 值 | 监视 | 处理器参数 |
|---|---|---|---|
| `GCD\SOURCE_TIMER` | 1 | start/interval 调度 | 无 |
| `GCD\SOURCE_SIGNAL` | 2 | 信号号（`$handle`） | 无 |
| `GCD\SOURCE_READ` | 3 | fd 可读（`$handle`） | 无 |
| `GCD\SOURCE_WRITE` | 4 | fd 可写（`$handle`） | 无 |
| `GCD\SOURCE_PROC` | 5 | 子进程退出（`$handle` = pid） | 无 |
| `GCD\SOURCE_DATA_ADD` | 6 | 累加值非零 | `int` 合并总和 |

READ/WRITE 接受原生整型文件描述符，纯 PHP 用户态无法从流中提取；在拥有真实 fd 的代码中使用（如经 FFI）。PROC 示例使用 ext-pcntl、SIGNAL 示例使用 ext-posix；源类型本身只需要 pid / 信号号。