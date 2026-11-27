# 实现备注：契约 vs 实现差异记录

用户文档实现期间（2026-11-27）发现的 `specs/001-php-gcd-extension/contracts/php-api.md`
（设计期提案）与 v1 实现的差异。文档一律以 C 源码为准；本表供后续修订契约参考。

## 契约中不存在于 v1 的功能

来源：gcd.c:237-280 函数表（43 函数）与各类 INIT_CLASS_ENTRY 对照。

- `after` / `after_walltime`（延时派发）
- `block_*` 与 `GCD\Block` 类
- `io_*` / `read` / `write` 与 `GCD\IO` 类（dispatch_io 通道）
- `async_f` / `sync_f`（函数名派发变体）
- `assert_queue`
- `source_suspend` / `source_set_cancel_handler` / `source_set_registration_handler`
- `run_once`（实现为 `once`，且语义为调用线程同步执行）
- `queue_attr_make_with_qos` 与 `QueueAttr`
- `data_apply`
- `TimeoutException`（超时以返回 `false`/`null` 表达）
- `Queue::push/pop/tryPop/count`（实现为独立的 `ConcurrentQueue` 类）
- 契约列 9 种源类型；实现 6 种（source.c:16-21）

## 语义差异（以源码为准）

| 主题 | 契约/README 说法 | v1 实现 | 来源 |
|---|---|---|---|
| main() | 回调驱动主循环 | 等价 drain(-1)，排空即返 | src/loop.c:21-27 |
| sync 返回值 | 隐含返回任务结果 | 恒 null，任务返回值丢弃 | src/queue/dispatch.c:47 |
| global_queue 参数 | qos/pool_size 生效 | 接受但忽略，固定 width 64 | src/queue/queue.c:246-259 |
| 容器共享 | Queue 上 push/pop | 独立容器类 + 静态 open() 进程级命名注册 | src/container/*.c |
| 异常父类 | 未明确 | 三个异常均继承 Error | src/queue/queue.c:203-211 |
| QoS | 平台优先级 | best-effort；qos_class_self 恒 DEFAULT(2) | src/queue/queue.c:185-190 |
| source mask/leeway | 生效 | 接受但不应用；interval 省略时不初始化 | src/source/source.c:202-275 |
| data_create_map | 懒映射回调 | 返回整块连续副本 | src/io/data.c:124-133 |
| 句柄类可实例化性 | 设计假设不可实例化 | new GCD\Queue() 得到合法 width-1 串行队列 | src/queue/queue.c:22-40 |
| argv 不可序列化 | 隐含抛异常 | task_build 忽略拷贝返回值（文档只写规则） | src/runtime/task.c:172-177 |
| READ/WRITE 源 | fd 集成 | 需原生整型 fd，纯用户态无从流中提取 | src/source/source.c:133-141 |

## 文档侧决定

- READ/WRITE 与 fd I/O 模式用 ```text 示意（无可运行示例），见 Task 5 Ruling。
- 任务内异常经「队列处理器 → 全局处理器 → 静默丢弃」上报，不达调用方（src/runtime/error.c:16-41）。
- drain 只等已在途工作，不等未来源事件（src/runtime/thread_pool.c:131-152）。