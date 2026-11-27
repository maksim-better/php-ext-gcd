# 错误

[English](../../en/api/errors.md) | [中文](../../zh/api/errors.md)

**任务内**抛出的异常永远不会到达派发调用方——`sync` 死锁保护也只能在任务内触发，属于此类。它们走上报层：优先队列级处理器，其次全局处理器，都没有则静默丢弃。处理器接收 `(string $message, string $className)`，在失败任务的 worker 上下文运行；返回值被忽略，处理器自身抛出的异常也被丢弃。

**派发期**抛出的异常（句柄错误、捕获检查、序列化检查）是普通同步异常——在调用脚本里捕获。

## set_error_handler

**签名**

```text
GCD\set_error_handler(Closure $handler): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$handler` | `Closure` | `function (string $message, string $className)`；v1 闭包规则（无捕获/静态；允许 try/catch）。不支持的闭包抛 `GCD\CaptureException` |

**返回值** —— 无。

**错误与异常** —— 非用户闭包抛 `TypeError`（"handler must be a user-function Closure"）。

**备注** —— 进程级兜底，作用于所有未设自身处理器的队列。替换先前的全局处理器。在 worker 中运行，结果通过共享容器记录。捕获变量或声明静态的处理器抛 `GCD\CaptureException`。

**示例**

```php
$errors = new GCD\Atomic(0, 'err');
GCD\set_error_handler(function (string $msg, string $cls) {
    GCD\Atomic::open('err')->inc();
});
$q = GCD\queue_create('w', 4);
$g = GCD\group_create();
for ($i = 0; $i < 5; $i++) {
    GCD\group_async($g, $q, function () { throw new RuntimeException('boom'); });
}
GCD\group_wait($g);
GCD\drain();
echo $errors->get();   // 5
```

## queue_set_error_handler

**签名**

```text
GCD\queue_set_error_handler(GCD\Queue $queue, Closure $handler): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$queue` | `GCD\Queue` | 要设置处理器的队列 |
| `$handler` | `Closure` | 形状与规则同全局处理器 |

**返回值** —— 无。

**错误与异常** —— `TypeError`（"expected GCD\Queue"）；`TypeError`（"handler must be a user-function Closure"）；`GCD\CaptureException`（"handler closure unsupported in v1"）。

**备注** —— 仅对该队列上的任务覆盖全局处理器。替换先前的队列处理器。

**示例**

```php
$global = new GCD\Atomic(0, 'g-err');
$local = new GCD\Atomic(0, 'q-err');
GCD\set_error_handler(function (string $m, string $c) { GCD\Atomic::open('g-err')->inc(); });
$q = GCD\queue_create('w', 1);
GCD\queue_set_error_handler($q, function (string $m, string $c) { GCD\Atomic::open('q-err')->inc(); });
GCD\sync($q, function () { throw new RuntimeException('x'); });
GCD\drain();
echo $global->get(), "\n";   // 0 —— 队列处理器优先
echo $local->get();          // 1
```

## CaptureException

**继承** —— `Error`。

**触发条件** —— 闭包无法拷贝到持久存储：捕获变量或声明静态变量（try/catch 实际被接受）。派发期由 `async`/`sync`/`barrier_*`/`apply`/`group_async` 抛出；注册 notify/源/错误处理器（全局与队列级）时同样检查。

**消息** —— `"closure captures variables or uses statics (unsupported in v1)"`（处理器变体措辞略有不同）。

**示例**

```php
$q = GCD\queue_create('w', 1);
try {
    GCD\async($q, function () { static $x = 0; });
} catch (GCD\CaptureException $e) {
    echo "capture rejected\n";
}
```

## SerializationException

**继承** —— `Error`。

**触发条件** —— 值无法深拷贝到持久存储：`queue_set_specific`、`Map::set`、`ConcurrentQueue::push`。

**消息** —— `"value is not serializable"`。

**示例** —— 见 [Map::set](containers.md#set-1)。

## DeadlockException

**继承** —— `Error`。

**触发条件** —— `sync` 的目标是当前任务正在执行的宽度 1 队列。在任务内抛出，因此经由[上报层](#set_error_handler)上报，而非外层调用点。

**消息** —— `"sync dispatch to current serial queue would deadlock"`。

**示例** —— 见[派发指南](../guide/03-dispatch.md#async-还是-sync)。