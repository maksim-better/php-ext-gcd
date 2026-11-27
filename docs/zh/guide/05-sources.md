# 事件源

[English](../../en/guide/05-sources.md) | [中文](../../zh/guide/05-sources.md)

源监视某种事物，触发时把处理器派发到队列。生命周期固定：[`source_create`](../api/source.md#source_create) → [`source_set_event_handler`](../api/source.md#source_set_event_handler) →（按类型设置）→ [`source_resume`](../api/source.md#source_resume) → … → [`source_cancel`](../api/source.md#source_cancel)。处理器就是普通任务：同样的 v1 闭包规则，结果通过容器传回。

## 定时器

```php
$q = GCD\queue_create('w', 2);
$c = new GCD\Atomic(0, 'src-timer');
$src = GCD\source_create(GCD\SOURCE_TIMER, 0, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open('src-timer')->inc();
});
GCD\source_set_timer($src, 0.02, 0.02, 0);   // 20ms 后首次触发，之后每 20ms
GCD\source_resume($src);
usleep(150000);
GCD\source_cancel($src);
GCD\drain(1.0);
$n = $c->get();
echo ($n >= 3 && $n <= 12) ? 'timer-ok' : "bad:$n";
```

间隔务必显式传入。`leeway` v1 接受但不应用。

## 信号

SIGNAL 源在专用线程上经 `sigwait` 交付——ZTS 下安全，无异步信号风险。`source_resume` 在调用线程阻塞该信号，worker 继承掩码。需要 ext-posix：

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
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $c->get();   // 1
```

## 子进程

PROC 源轮询 `waitpid`（10ms），子进程退出时触发一次。需要 ext-pcntl：

```php
$pid = pcntl_fork();
if ($pid === 0) { usleep(50000); exit(0); }
$q = GCD\queue_create('w', 1);
$c = new GCD\Atomic(0, 'src-proc');
$src = GCD\source_create(GCD\SOURCE_PROC, $pid, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open('src-proc')->inc();
});
GCD\source_resume($src);
usleep(150000);
GCD\drain(1.0);
GCD\source_cancel($src);
echo $c->get();   // 1
```

## 文件描述符

READ/WRITE 源轮询原生 fd（水平触发：fd 持续就绪期间处理器反复触发）。handle 是整型描述符——纯 PHP 用户态无法从流中提取，因此这两类服务于拥有真实 fd 的代码（如经 FFI）：

```text
$src = GCD\source_create(GCD\SOURCE_READ, $fd, 0, $queue);
GCD\source_set_event_handler($src, function () { /* read($fd) ... */ });
GCD\source_resume($src);
```

缓冲数据操作见[数据缓冲](06-data.md)。

## 数据合并通知

DATA_ADD 把大量小通知合并成少量处理器调用：生产者用 [`source_merge_data`](../api/source.md#source_merge_data) 累加；监视线程清零累加值并把总和作为处理器唯一的 `int` 参数派发。

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

[`source_get_data`](../api/source.md#source_get_data) 读取累加值但不消费。