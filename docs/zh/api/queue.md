# 队列与 QoS

[English](../../en/api/queue.md) | [中文](../../zh/api/queue.md)

## Queue 句柄

`GCD\Queue` 是无方法的句柄对象，通过 `queue_create`、`global_queue`、`current_queue` 或 `main_queue` 获得。也可以直接 `new GCD\Queue()`，得到一个无标签的串行队列（宽度 1），但工厂函数才是预期用法。

## queue_create

**签名**

```text
GCD\queue_create(string $label, int $width = 1): GCD\Queue
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$label` | `string` | 队列名（必填）；仅存储，当前无 API 暴露 |
| `$width` | `int` | 最大并发任务数；小于 1 的值按 1 处理 |

**返回值** —— 新建的 `GCD\Queue`。宽度 1 为串行（FIFO）；大于 1 为并发。

**错误与异常** —— 参数类型错误时抛标准 `TypeError`。

**备注** —— label 被存储但无 API 暴露；不会把队列注册到任何地方。

**示例**

```php
$serial = GCD\queue_create('jobs', 1);
$out = tempnam(sys_get_temp_dir(), 'gcd_q_');
GCD\async($serial, function (string $f) { file_put_contents($f, 'done'); }, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## global_queue

**签名**

```text
GCD\global_queue(int $qos = 0, int $pool_size = 0): GCD\Queue
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$qos` | `int` | v1 接受但忽略 |
| `$pool_size` | `int` | v1 接受但忽略 |

**返回值** —— 标签为 `global`、固定宽度 64 的并发队列。

**错误与异常** —— 参数类型错误时抛标准 `TypeError`。

**备注** —— v1 不应用这两个参数；每次调用返回配置相同的新句柄。

**示例**

```php
$g = GCD\global_queue();
$out = tempnam(sys_get_temp_dir(), 'gcd_g_');
GCD\async($g, function (string $f) { file_put_contents($f, 'global'); }, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## current_queue

**签名**

```text
GCD\current_queue(): ?GCD\Queue
```

**参数** —— 无。

**返回值** —— 当前任务所在的队列（串行或并发）；主线程与任务之外返回 `null`。

**错误与异常** —— 无。

**备注** —— 线程局部：每个 worker 报告其任务所在的队列。`current_queue()` + `queue_get_specific()` 模式在任何任务内有效。

**示例**

```php
$q = GCD\queue_create('ctx', 1);
GCD\queue_set_specific($q, 'role', 'worker');
$out = tempnam(sys_get_temp_dir(), 'gcd_cq_');
GCD\async($q, function (string $f) {
    $cur = GCD\current_queue();
    file_put_contents($f, (string) GCD\queue_get_specific($cur, 'role'));
}, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## main_queue

**签名**

```text
GCD\main_queue(): GCD\Queue
```

**参数** —— 无。

**返回值** —— 进程级主队列：懒创建的串行队列（宽度 1，标签 `main`）。

**错误与异常** —— 无。

**备注** —— 单例：每次调用返回同一队列。派发到此处的工作在循环 drain 时执行（见 [main/drain](loop.md)）。

**示例**

```php
$mq = GCD\main_queue();
$out = tempnam(sys_get_temp_dir(), 'gcd_mq_');
GCD\async($mq, function (string $f) { file_put_contents($f, 'main'); }, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## queue_suspend

**签名**

```text
GCD\queue_suspend(GCD\Queue $queue): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$queue` | `GCD\Queue` | 要挂起的队列 |

**返回值** —— 无。

**错误与异常** —— 参数不是 Queue 时抛 `TypeError`（"expected GCD\Queue"）。

**备注** —— 挂起的队列不再启动新任务；已在运行的任务不被打断。调用 `queue_resume` 继续。挂起是标志位而非计数器：挂起两次仍只需一次 resume。

**示例**

```php
$q = GCD\queue_create('gate', 1);
$out = tempnam(sys_get_temp_dir(), 'gcd_s_');
GCD\queue_suspend($q);
GCD\async($q, function (string $f) { file_put_contents($f, 'released'); }, [$out]);
GCD\queue_resume($q);
GCD\drain();
echo file_get_contents($out);
unlink($out);
```

## queue_resume

**签名**

```text
GCD\queue_resume(GCD\Queue $queue): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$queue` | `GCD\Queue` | 要恢复的队列 |

**返回值** —— 无。

**错误与异常** —— 参数不是 Queue 时抛 `TypeError`（"expected GCD\Queue"）。

**备注** —— 清除挂起标志并唤醒等待的 worker。恢复未挂起的队列无副作用。

**示例** —— 见 [`queue_suspend`](#queue_suspend)。

## queue_set_specific

**签名**

```text
GCD\queue_set_specific(GCD\Queue $queue, string $key, mixed $value): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$queue` | `GCD\Queue` | 目标队列 |
| `$key` | `string` | 上下文键 |
| `$value` | `mixed` | 仅 null/bool/int/float/string 及它们的数组（深拷贝；对象抛 SerializationException） |

**返回值** —— 无。

**错误与异常** —— 不是 Queue 时抛 `TypeError`；值不可序列化时抛 `GCD\SerializationException`（"value is not serializable"）。

**备注** —— 队列上下文随队列传递；任务内用 `current_queue()` + `queue_get_specific` 读取。

**示例**

```php
$q = GCD\queue_create('cfg', 1);
GCD\queue_set_specific($q, 'retries', 3);
var_export(GCD\queue_get_specific($q, 'retries'));
echo "\n";
var_export(GCD\queue_get_specific($q, 'missing'));
```

## queue_get_specific

**签名**

```text
GCD\queue_get_specific(GCD\Queue $queue, string $key): mixed
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$queue` | `GCD\Queue` | 来源队列 |
| `$key` | `string` | 上下文键 |

**返回值** —— 存储值的副本；键不存在时返回 `null`。

**错误与异常** —— 参数不是 Queue 时抛 `TypeError`（"expected GCD\Queue"）。

**备注** —— 返回副本，修改它不影响存储值。

**示例** —— 见 [`queue_set_specific`](#queue_set_specific)。

## queue_set_target

**签名**

```text
GCD\queue_set_target(GCD\Queue $queue, GCD\Queue $target): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$queue` | `GCD\Queue` | 要设置目标的队列 |
| `$target` | `GCD\Queue` | 目标队列 |

**返回值** —— 无。

**错误与异常** —— 任一参数不是 Queue 时抛 `TypeError`；自指向抛 `ValueError`（"queue cannot target itself"）；成环抛 `ValueError`（"target queue cycle"）。

**备注** —— QoS 解析沿目标链进行（见 `queue_get_qos`）。目标被持有引用，unset 目标变量后链仍有效。

**示例**

```php
$parent = GCD\queue_create('parent', 2);
GCD\queue_set_qos($parent, GCD\QOS_USER_INITIATED);
$child = GCD\queue_create('child', 1);
GCD\queue_set_target($child, $parent);
echo GCD\queue_get_qos($child) === GCD\QOS_USER_INITIATED ? 'inherit-ok' : 'bad';
```

## queue_set_qos

**签名**

```text
GCD\queue_set_qos(GCD\Queue $queue, int $qos): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$queue` | `GCD\Queue` | 目标队列 |
| `$qos` | `int` | [QoS 常量](#qos-常量)之一 |

**返回值** —— 无。

**错误与异常** —— 参数不是 Queue 时抛 `TypeError`（"expected GCD\Queue"）。

**备注** —— v1 的 QoS 为 best-effort（见 `qos_class_self`）；值被记录并沿目标链解析。

**示例**

```php
$q = GCD\queue_create('q', 2);
GCD\queue_set_qos($q, GCD\QOS_BACKGROUND);
echo GCD\queue_get_qos($q);
```

## queue_get_qos

**签名**

```text
GCD\queue_get_qos(GCD\Queue $queue): int
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$queue` | `GCD\Queue` | 要查询的队列 |

**返回值** —— 生效的 QoS 等级：沿目标链第一个显式设置的值；都未设置时为 `GCD\QOS_DEFAULT`（2）。

**错误与异常** —— 参数不是 Queue 时抛 `TypeError`（"expected GCD\Queue"）。

**备注** —— 未设自身 QoS 的队列递归继承目标的 QoS。

**示例** —— 见 [`queue_set_target`](#queue_set_target)。

## qos_class_self

**签名**

```text
GCD\qos_class_self(): int
```

**参数** —— 无。

**返回值** —— 调用上下文的 QoS 等级。v1 恒返回 `GCD\QOS_DEFAULT`（2）。

**错误与异常** —— 无。

**备注** —— Linux best-effort：v1 不从执行 worker 推导等级。

**示例**

```php
echo GCD\qos_class_self() >= 0 ? 'ok' : 'bad';
```

## QoS 常量

| 常量 | 值 |
|---|---|
| `GCD\QOS_USER_INTERACTIVE` | 4 |
| `GCD\QOS_USER_INITIATED` | 3 |
| `GCD\QOS_DEFAULT` | 2 |
| `GCD\QOS_UTILITY` | 1 |
| `GCD\QOS_BACKGROUND` | 0 |