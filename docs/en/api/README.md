# API Reference Index

[English](../../en/api/README.md) | [中文](../../zh/api/README.md)

All symbols live in the `GCD` namespace. Signatures are verified against the C implementation.

## Function groups

| Group | Functions | Page |
|---|---|---|
| Queues & QoS | `queue_create` `global_queue` `current_queue` `main_queue` `queue_suspend` `queue_resume` `queue_set_specific` `queue_get_specific` `queue_set_target` `queue_set_qos` `queue_get_qos` `qos_class_self` | [queue.md](queue.md) |
| Dispatch | `async` `sync` `barrier_async` `barrier_sync` `apply` `once` | [dispatch.md](dispatch.md) |
| Groups | `group_create` `group_async` `group_enter` `group_leave` `group_wait` `group_notify` | [group.md](group.md) |
| Semaphores | `semaphore_create` `semaphore_open` `semaphore_wait` `semaphore_signal` | [semaphore.md](semaphore.md) |
| Event sources | `source_create` `source_set_event_handler` `source_set_timer` `source_resume` `source_cancel` `source_merge_data` `source_get_data` | [source.md](source.md) |
| Data | `data_create` `data_concat` `data_create_map` `data_subrange` | [data.md](data.md) |
| Main loop | `main` `drain` | [loop.md](loop.md) |
| Error handling | `set_error_handler` `queue_set_error_handler` | [errors.md](errors.md) |

## Alphabetical index

`apply` · `async` · `barrier_async` · `barrier_sync` · `current_queue` · `data_concat` · `data_create` · `data_create_map` · `data_subrange` · `drain` · `global_queue` · `group_async` · `group_create` · `group_enter` · `group_leave` · `group_notify` · `group_wait` · `main` · `main_queue` · `once` · `qos_class_self` · `queue_create` · `queue_get_qos` · `queue_get_specific` · `queue_resume` · `queue_set_error_handler` · `queue_set_qos` · `queue_set_specific` · `queue_set_target` · `queue_suspend` · `semaphore_create` · `semaphore_open` · `semaphore_signal` · `semaphore_wait` · `set_error_handler` · `source_cancel` · `source_create` · `source_get_data` · `source_merge_data` · `source_resume` · `source_set_event_handler` · `source_set_timer` · `sync`

## Classes

| Class | Kind | Page |
|---|---|---|
| `GCD\Queue` | handle (no methods) | [queue.md](queue.md) |
| `GCD\Group` | handle (no methods) | [group.md](group.md) |
| `GCD\Semaphore` | handle (no methods) | [semaphore.md](semaphore.md) |
| `GCD\Source` | handle (no methods) | [source.md](source.md) |
| `GCD\Data` | immutable buffer (`size()`) | [data.md](data.md) |
| `GCD\Atomic` | shared counter | [containers.md](containers.md) |
| `GCD\Map` | shared map | [containers.md](containers.md) |
| `GCD\Set` | shared set | [containers.md](containers.md) |
| `GCD\ConcurrentQueue` | shared MPMC queue | [containers.md](containers.md) |

## Constants

- QoS classes (5): `GCD\QOS_USER_INTERACTIVE` `GCD\QOS_USER_INITIATED` `GCD\QOS_DEFAULT` `GCD\QOS_UTILITY` `GCD\QOS_BACKGROUND` — [queue.md](queue.md)
- Source types (6): `GCD\SOURCE_*` — [source.md](source.md)

## Exceptions

`GCD\CaptureException` · `GCD\SerializationException` · `GCD\DeadlockException` (all extend `Error`) — [errors.md](errors.md)