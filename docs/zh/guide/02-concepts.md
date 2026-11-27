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

- 任务里的 `echo` 不会到达脚本的 STDOUT。结果通过文件、[`sync`](03-dispatch.md) 的返回值或[共享容器](07-containers.md)传回。
- 普通 PHP 全局变量与静态变量是每运行时各自的，不在任务间共享。
- 只有共享容器（Atomic、Map、Set、ConcurrentQueue）为跨线程访问而生。

## 任务规则

任务 = **闭包 + 可序列化参数数组**。v1 把闭包的 op_array 拷贝到持久存储；当闭包有以下情况时拷贝失败，抛 `GCD\CaptureException`：

- 捕获变量（`use` 构造），
- 声明静态变量，
- 包含 try/catch。

任务所需的一切通过 `$argv` 传递：

```php
$q = GCD\queue_create('rules', 1);
try {
    GCD\async($q, function () { static $counter = 0; $counter++; });
} catch (GCD\CaptureException $e) {
    echo "rejected: statics are not supported in v1 tasks\n";
}
```

参数值被深拷贝；保持可序列化（标量、数组、可序列化对象）。

## 句柄对象

`GCD\Queue`、`GCD\Group`、`GCD\Semaphore`、`GCD\Source` 是句柄：没有方法、指代内核侧资源的对象。通过工厂函数创建，再传回 API。`GCD\Data` 是只有一个 `size()` 方法的不可变缓冲；共享容器提供真正的方法。

## 下一步

- [派发任务](03-dispatch.md) —— async/sync、barrier、apply、once
- [共享容器](07-containers.md) —— 跨线程状态工具箱
- [队列与 QoS API](../api/queue.md)