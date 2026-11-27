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

## 最终审查新增差异（2026-11-27，整分支审查后逐条核实）

> 更新：下表多数 C 缺陷已在 `specs/003-correctness-semantics/` 修复（D1/D2/D3/D5/D6/D7/D8/D9/D10/S1/S2），
> 文档警示已同步移除；仅 D4（脚本符号 SIGBUS）延期，警示保留。详见 003 notes。

文档已按以下核实行为修正；对应 C 缺陷列为后续修复任务（不在本文档计划范围）：

| 行为 | 核实结果 | C 后续建议 |
|---|---|---|
| argv 含对象 | SIGSEGV（exit 139）；copy.c 仅支持 null/bool/int/float/string/array，task.c 忽略拷贝失败 | gcd_task_build 拷贝失败应抛 SerializationException |
| Set add/contains/remove 含闭包/资源 | SIGSEGV；make_key 不检查 php_var_serialize 失败 | set.c make_key 检查失败并抛异常 |
| 池已启动后 resume SIGNAL 源 | 进程被信号终止（exit 158）；掩码仅阻塞调用线程 | 池启动前/进程级阻塞源信号 |
| 任务调用脚本级函数 | SIGBUS（exit 138）；仅闭包 op_array 被拷贝 | 未定义函数应抛可捕获 Error |
| try/catch 任务闭包 | 实际被接受（copy.c 拷贝 try_catch_array）；C 错误消息过时 | 更新 copy.c 消息或补检查 |
| 全局 set_error_handler 捕获闭包 | 静默置空（error.c 不检查拷贝结果） | 仿 queue 版抛 CaptureException |
| current_queue() | 仅 width-1 队列任务内设置（task.c:128-134） | 如需全队列支持另行实现 |
| 处理器 $message | Error 系异常为空（error.c:21 经 zend_ce_exception 读属性） | 按异常自身 ce 读 message |
| barrier 顺序 | 后续提交可先于 barrier 运行（task.c:108-126 仅等启动时在途） | 如需顺序保证另行实现 |
| group_notify 后复用 group | 计数减至 -1，group_wait 提前返回（notify 任务带 t->group） | notify 任务不应 leave group |
| queue_set_target 生命周期 | 不 retain 目标；unset 后 QoS 解析读已释放内存 | retain/release 目标 |
| echo / 全局变量 | worker 输出直达 STDOUT（无同步）；同 worker 的 async 任务间全局变量可泄漏 | 文档已警示 |
| source_create 显式 null queue | TypeError（Z_PARAM_OBJECT 非 nullable） | 文档已改为省略参数 |
| timer interval 省略 | 未初始化栈值，行为不确定 | C 侧给默认值 |