# API 参考索引

[English](../../en/api/README.md) | [中文](../../zh/api/README.md)

所有符号位于 `GCD` 命名空间。签名均已对照 C 实现核实。

## 函数分组

| 分组 | 函数 | 页面 |
|---|---|---|
| 队列与 QoS | `queue_create` `global_queue` `current_queue` `main_queue` `queue_suspend` `queue_resume` `queue_set_specific` `queue_get_specific` `queue_set_target` `queue_set_qos` `queue_get_qos` `qos_class_self` | [queue.md](queue.md) |
| 派发 | `async` `sync` `barrier_async` `barrier_sync` `apply` `once` | [dispatch.md](dispatch.md) |
| Group | `group_create` `group_async` `group_enter` `group_leave` `group_wait` `group_notify` | [group.md](group.md) |
| Semaphore | `semaphore_create` `semaphore_open` `semaphore_wait` `semaphore_signal` | [semaphore.md](semaphore.md) |
| 事件源 | `source_create` `source_set_event_handler` `source_set_timer` `source_resume` `source_cancel` `source_merge_data` `source_get_data` | [source.md](source.md) |
| 数据 | `data_create` `data_concat` `data_create_map` `data_subrange` | [data.md](data.md) |
| 主循环 | `main` `drain` | [loop.md](loop.md) |
| 错误处理 | `set_error_handler` `queue_set_error_handler` | [errors.md](errors.md) |

## 字母序索引

`apply` · `async` · `barrier_async` · `barrier_sync` · `current_queue` · `data_concat` · `data_create` · `data_create_map` · `data_subrange` · `drain` · `global_queue` · `group_async` · `group_create` · `group_enter` · `group_leave` · `group_notify` · `group_wait` · `main` · `main_queue` · `once` · `qos_class_self` · `queue_create` · `queue_get_qos` · `queue_get_specific` · `queue_resume` · `queue_set_error_handler` · `queue_set_qos` · `queue_set_specific` · `queue_set_target` · `queue_suspend` · `semaphore_create` · `semaphore_open` · `semaphore_signal` · `semaphore_wait` · `set_error_handler` · `source_cancel` · `source_create` · `source_get_data` · `source_merge_data` · `source_resume` · `source_set_event_handler` · `source_set_timer` · `sync`

## 类

| 类 | 种类 | 页面 |
|---|---|---|
| `GCD\Queue` | 句柄（无方法） | [queue.md](queue.md) |
| `GCD\Group` | 句柄（无方法） | [group.md](group.md) |
| `GCD\Semaphore` | 句柄（无方法） | [semaphore.md](semaphore.md) |
| `GCD\Source` | 句柄（无方法） | [source.md](source.md) |
| `GCD\Data` | 不可变缓冲（`size()`） | [data.md](data.md) |
| `GCD\Atomic` | 共享计数器 | [containers.md](containers.md) |
| `GCD\Map` | 共享映射 | [containers.md](containers.md) |
| `GCD\Set` | 共享集合 | [containers.md](containers.md) |
| `GCD\ConcurrentQueue` | 共享 MPMC 队列 | [containers.md](containers.md) |

## 常量

- QoS 等级（5 个）：`GCD\QOS_USER_INTERACTIVE` `GCD\QOS_USER_INITIATED` `GCD\QOS_DEFAULT` `GCD\QOS_UTILITY` `GCD\QOS_BACKGROUND` —— [queue.md](queue.md)
- 事件源类型（6 个）：`GCD\SOURCE_*` —— [source.md](source.md)

## 异常

`GCD\CaptureException` · `GCD\SerializationException` · `GCD\DeadlockException`（均继承 `Error`）—— [errors.md](errors.md)