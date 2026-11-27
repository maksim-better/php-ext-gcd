# Contract: GCD PHP 公共 API

**Type**: 库公共 API（PHP 扩展）。命名空间函数 `GCD\*` + 容器类（命名最终形态为提案默认，见 spec Assumptions）。
**Source**: [spec.md](../spec.md) FR-001–036 + Clarifications。

## 通用约定

- **超时参数** `float $timeout`：`-1` 永久阻塞，`0` 非阻塞，`>0` 秒数超时；超时返回 `false`/`null`。
- **任务** `Closure $task, array $argv = []`：闭包不得捕获外部变量（违反抛 `GCD\CaptureException`）；argv 须可序列化。
- **async 返回 void**（FR-005）；结果经容器/回调取回。
- **错误**：可捕获异常经三层上报（FR-030）；致命错误进程终止（FR-036）。
- **QoS 常量**：`GCD\QOS_USER_INTERACTIVE` / `QOS_USER_INITIATED` / `QOS_DEFAULT` / `QOS_UTILITY` / `QOS_BACKGROUND`。

## 队列创建与控制（FR-001–004, 013, 022, 023）

| 签名 | 说明 |
|------|------|
| `GCD\queue_create(string $label, int $width = 1, ?Queue $target = null, int $qos = QOS_DEFAULT): Queue` | width=1 串行，>1 并发 |
| `GCD\global_queue(int $qos = QOS_DEFAULT, int $pool_size = 0): Queue` | 全局并发队列，pool_size=0 用默认 |
| `GCD\main_queue(): Queue` | 绑定主线程 |
| `GCD\queue_set_target(Queue $q, Queue $target): void` | 层级 |
| `GCD\queue_suspend(Queue $q): void` / `GCD\queue_resume(Queue $q): void` | 挂起/恢复 |
| `GCD\queue_set_specific(Queue $q, string $key, mixed $val): void` / `GCD\queue_get_specific(Queue $q, string $key): mixed` | 专属上下文 |
| `GCD\assert_queue(Queue $q): void` | 断言当前队列 |
| `GCD\queue_set_error_handler(Queue $q, Closure $cb): void` | per-queue 错误回调 |

## 派发（FR-005–009, 014, 018）

| 签名 | 说明 |
|------|------|
| `GCD\async(Queue $q, Closure $task, array $argv = []): void` | 异步，立即返回 |
| `GCD\sync(Queue $q, Closure $task, array $argv = []): void` | 同步阻塞；当前串行队列 sync 抛 `GCD\DeadlockException` |
| `GCD\async_f(Queue $q, callable $fn, array $argv = []): void` / `GCD\sync_f(...)` | 函数变体 |
| `GCD\barrier_async(Queue $q, Closure $task, array $argv = []): void` / `GCD\barrier_sync(...)` | 栅栏独占 |
| `GCD\after(float $delay, Queue $q, Closure $task, array $argv = []): void` | 延时（相对秒） |
| `GCD\after_walltime(float $walltime, Queue $q, Closure $task, array $argv = []): void` | 绝对墙钟 |
| `GCD\apply(int $iterations, Queue $q, Closure $task): void` | 并行迭代，task 收 index，调用线程参与并等待 |

## Block 对象（FR-017）

| 签名 | 说明 |
|------|------|
| `GCD\block_create(Closure $task, array $argv = [], int $flags = 0): Block` | 可取消任务块 |
| `GCD\block_wait(Block $b, float $timeout = -1): bool` | 等待完成 |
| `GCD\block_notify(Block $b, Queue $q, Closure $cb): void` | 完成通知 |
| `GCD\block_cancel(Block $b): void` / `GCD\block_test_cancel(Block $b): bool` | 协作式取消 |

## 同步原语（FR-010–012）

| 签名 | 说明 |
|------|------|
| `GCD\group_create(): Group` | |
| `GCD\group_enter(Group $g): void` / `GCD\group_leave(Group $g): void` | 计数 |
| `GCD\group_wait(Group $g, float $timeout = -1): bool` | 阻塞至归零（可超时） |
| `GCD\group_notify(Group $g, Queue $q, Closure $cb): void` | 归零通知 |
| `GCD\group_async(Group $g, Queue $q, Closure $task, array $argv = []): void` | 入组异步 |
| `GCD\semaphore_create(int $value): Semaphore` | |
| `GCD\semaphore_signal(Semaphore $s): void` / `GCD\semaphore_wait(Semaphore $s, float $timeout = -1): bool` | 限流 |
| `GCD\once(string $key, Closure $task): void` | 按 key 恰好执行一次 |

## 事件源（FR-015, 016, 035）

| 签名 | 说明 |
|------|------|
| `GCD\source_create(int $type, int $handle, int $mask = 0, ?Queue $q = null): Source` | type 见常量；排除 Mach |
| `GCD\source_set_event_handler(Source $s, Closure $cb): void` | 事件回调 |
| `GCD\source_set_cancel_handler(Source $s, Closure $cb): void` / `GCD\source_set_registration_handler(Source $s, Closure $cb): void` | |
| `GCD\source_resume(Source $s): void` / `GCD\source_suspend(Source $s): void` / `GCD\source_cancel(Source $s): void` | |
| `GCD\source_set_timer(Source $s, float $start, float $interval, float $leeway = 0): void` | 定时器 |
| `GCD\source_get_data(Source $s): int` / `GCD\source_merge_data(Source $s, int $data): void` | data 源 |

**Source 类型常量**：`SOURCE_TIMER` / `SOURCE_READ` / `SOURCE_WRITE` / `SOURCE_SIGNAL` / `SOURCE_PROC` / `SOURCE_VNODE` / `SOURCE_DATA_ADD` / `SOURCE_DATA_OR` / `SOURCE_DATA_REPLACE`。（`SOURCE_MEMORYPRESSURE` 为 Darwin 专属，跨平台子集排除；PROC/VNODE 在 Linux 经 pidfd/inotify 自定义后端实现。）

## 数据与 IO（FR-019, 020）

| 签名 | 说明 |
|------|------|
| `GCD\data_create(string $bytes): Data` | 不可变缓冲 |
| `GCD\data_concat(Data ...$parts): Data` / `GCD\data_subrange(Data $d, int $offset, int $len): Data` | |
| `GCD\data_apply(Data $d, Closure $cb): void` / `GCD\data_create_map(Data $d): string` | 遍历/映射 |
| `GCD\io_create(int $type, $fd, ?Queue $q = null): IO` | type: `IO_STREAM`/`IO_RANDOM` |
| `GCD\io_read(IO $io, int $len, Closure $cb): void` / `GCD\io_write(IO $io, Data $d, Closure $cb): void` | |
| `GCD\io_close(IO $io): void` / `GCD\io_barrier(IO $io, Closure $cb): void` | |
| `GCD\read($fd, int $len, Queue $q, Closure $cb): void` / `GCD\write($fd, Data $d, Queue $q, Closure $cb): void` | 便利函数 |

## QoS（FR-021）

| 签名 | 说明 |
|------|------|
| `GCD\queue_attr_make_with_qos(int $qos, int $relative_priority = 0): QueueAttr` | 队列属性 |
| `GCD\qos_class_self(): int` | 当前 QoS（Linux best-effort） |

## 主循环（FR-031，双模型）

| 签名 | 说明 |
|------|------|
| `GCD\main(): void` | dispatch_main，阻塞主线程，回调驱动 |
| `GCD\drain(float $timeout = 0): void` | 手动单步推进派发（非阻塞），供自定义 while 循环 |
| `GCD\run_once(): void` | 单轮 drain 便利封装 |

## 错误上报（FR-030，三层）

| 签名 | 说明 |
|------|------|
| `GCD\set_error_handler(Closure $cb): void` | 全局默认兜底；cb 收 (Throwable $e, array $taskContext) |
| `GCD\queue_set_error_handler(Queue $q, Closure $cb): void` | per-queue 覆盖 |
| group 异常聚合 | 经 `GCD\group_wait`/notify 上报成员异常 |

## 共享容器（FR-024–028，类）

值限可序列化（标量/数组；对象序列化拷贝）；存入资源/不可序列化值抛 `GCD\SerializationException`。

| 类 / 方法 | 说明 |
|-----------|------|
| `GCD\Queue::push(mixed $v): void` | MPMC |
| `GCD\Queue::pop(float $timeout = -1): mixed` | 阻塞，超时返回 null |
| `GCD\Queue::tryPop(): mixed` | 非阻塞，空返回 null |
| `GCD\Queue::count(): int` | |
| `GCD\Set::add(mixed $v): bool` / `contains(mixed $v): bool` / `remove(mixed $v): bool` / `count(): int` | 去重 |
| `GCD\Atomic::__construct(int $v = 0)` / `inc(int $n = 1): int` / `dec(int $n = 1): int` / `get(): int` / `set(int $v): void` | 原子计数 |
| `GCD\Map::set(string $k, mixed $v): void` / `get(string $k): mixed` / `delete(string $k): bool` / `has(string $k): bool` / `count(): int` | 并发 map |

## 异常类型

`GCD\CaptureException`（闭包捕获）、`GCD\DeadlockException`（sync 死锁）、`GCD\SerializationException`（不可序列化值）、`GCD\TimeoutException`（可选）。