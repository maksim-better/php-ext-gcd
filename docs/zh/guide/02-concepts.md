# 核心概念

[English](../../en/guide/02-concepts.md) | [中文](../../zh/guide/02-concepts.md)

## 队列：串行与并发

队列是工作的目的地，**宽度**决定任务如何执行：

- **宽度 1 —— 串行。** 任务按提交顺序（FIFO）逐个执行。用它保护不允许并发访问的状态。
- **宽度 > 1 —— 并发。** 最多 `width` 个任务在线程池上并行。

```php
$q = GCD\queue_create('ordered', 1);
$out = tempnam(sys_get_temp_dir(), 'gcd_ord_');
file_put_contents($out, '');
for ($i = 0; $i < 5; $i++) {
    GCD\async($q, function (string $f, int $n) {
        file_put_contents($f, (string) $n, FILE_APPEND | LOCK_EX);
    }, [$out, $i]);
}
GCD\drain();
echo file_get_contents($out);   // 01234 —— FIFO 有保证
unlink($out);
```

队列是句柄：用 [`queue_create`](../api/queue.md#queue_create) 创建，或使用内置的 [`global_queue`](../api/queue.md#global_queue) 与 [`main_queue`](../api/queue.md#main_queue)。

## 线程模型

扩展运行一个自定义 pthread 线程池。每个 worker 线程启动一个**隔离的 PHP 运行时**——一线程一请求。由此必须这样设计：

- worker 输出无同步：任务的 `echo` 可能交错出现在 STDOUT。结果通过文件或[共享容器](07-containers.md)收集；[`sync`](03-dispatch.md) 只等待任务完成，返回 `null`。
- worker 运行时跨任务存续：全局变量与静态变量可能在落到同一 worker 的任务间泄漏，且绝不跨 worker 共享。不要用它们做跨任务状态。
- 只有闭包自身的代码会到达 worker。脚本里定义的函数、类、常量在任务内**不可用**——调用它们可能使进程崩溃。把辅助函数定义在任务闭包内，或在任务里 `require`。
- 只有共享容器（Atomic、Map、Set、ConcurrentQueue）为跨线程访问而生。

## 任务规则

任务 = **闭包 + 参数数组**。v1 把闭包的 op_array 拷贝到持久存储；当闭包有以下情况时拷贝失败，抛 `GCD\CaptureException`：

- 捕获变量（`use` 构造），
- 声明静态变量。

任务内允许 try/catch。任务所需的一切通过 `$argv` 传递：

```php
$q = GCD\queue_create('rules', 1);
try {
    GCD\async($q, function () { static $counter = 0; $counter++; });
} catch (GCD\CaptureException $e) {
    echo "rejected: statics are not supported in v1 tasks\n";
}
```

参数值被深拷贝。**仅支持 `null`、布尔、整数、浮点、字符串及它们的数组。**对象（含 `GCD\Data`）、闭包、资源不支持：传入可能使进程崩溃。对象数据以字符串传递，在任务内重建。

## 句柄对象

`GCD\Queue`、`GCD\Group`、`GCD\Semaphore`、`GCD\Source` 是句柄：没有方法、指代内核侧资源的对象。通过工厂函数创建，再传回 API。`GCD\Data` 是只有一个 `size()` 方法的不可变缓冲；共享容器提供真正的方法。

## 下一步

- [派发任务](03-dispatch.md) —— async/sync、barrier、apply、once
- [共享容器](07-containers.md) —— 跨线程状态工具箱
- [队列与 QoS API](../api/queue.md)