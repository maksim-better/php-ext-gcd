# ext-gcd 正确性与语义修复设计

- 日期：2026-11-27
- 状态：待批准
- 范围：用户选定「连语义一起补」

## 意图与背景（为什么做）

双语文档实现期间的整分支审查核实出一批 C 缺陷：4 个崩溃/致命类、若干正确性类，
以及两处与真 GCD 不符的语义。文档目前如实警示这些行为，但根因在扩展本身。
本次修复让扩展安全正确，消除崩溃，补齐 barrier 顺序与 current_queue 语义，
随后同步文档去掉已修复的限制警示。

## 目标

**崩溃/致命（必须消除）**
- D1 argv 含对象 → 抛 `GCD\SerializationException`，不再 SIGSEGV。
- D2 `Set::add/contains/remove` 含闭包/资源 → 抛 `GCD\SerializationException`，不再 SIGSEGV。
- D3 线程池已启动后 resume SIGNAL 源 → 进程不再被信号终止；handler 正常触发。
- D4 任务调用脚本级函数/类/常量 → 抛可捕获 `Error`（经上报层），不再 SIGBUS。

**正确性**
- D5 `group_notify` 回调任务不再递减 group 计数；group 可跨批次复用。
- D6 `queue_set_target` 持有目标引用；unset 目标变量后 QoS 链仍有效。
- D7 错误处理器对 `Error` 系异常（含三个 GCD 异常）收到真实 `$message`。
- D8 全局 `set_error_handler` 对不支持的闭包抛 `CaptureException`，不再静默置空。
- D9 `source_set_timer` 省略 interval → 默认 0 = 一次性触发（不再未初始化栈值）。
- D10 `copy.c` 过时的 try/catch 拒绝消息更正（v1 实际接受 try/catch）。

**语义补齐**
- S1 barrier 真 GCD 顺序：barrier 之后提交的任务排在 barrier 之后执行。
- S2 `current_queue()` 在并发队列任务内也返回该队列（不再仅串行）。

**文档同步**：修复落地后更新 en/zh 文档，移除已修复限制的警示，重跑全部门禁。

## 非目标

- 不实现契约中缺席的其他功能（after、dispatch_io、Block 等）。
- 不改变 try/catch 的「接受」现状（只修消息，与文档一致）。
- 不重构调度器整体架构；只做必要的最小结构性改动。
- 不推送远程（用户工作流）。

## 反目标

- 修复引入新的数据竞争（仓库有 TSan 套件，必须保持 0 警告）。
- 破坏现有 29 个 phpt 测试。
- 修复后文档与实现再次漂移。
- 为修崩溃而吞掉错误（应抛可观察的异常，而非静默成功）。

## 约束

- ZTS PHP 8.4–8.6；macOS（本机 8.6 实测）/ Linux。
- 现有测试套件 29/29 必须保持绿；TSan 套件 0 警告。
- 异常类型沿用现有三个（Capture/Serialization/Deadlock，均继承 Error）+ 标准 TypeError/ValueError/Error。
- 提交不加 Co-Authored-By；规格文档放 specs/003-correctness-semantics/。

## 方案对比（关键实现取舍）

### S1 barrier 顺序

| 方案 | 做法 | 取舍 |
|---|---|---|
| **A 提交序号 admission（选定）** | 任务提交时 under width_mutex 分配单调 seq；gate 按 seq 顺序准入（`t->seq == q->next_admit` 才进入），barrier 额外等 inflight==0 | 改动集中在 gate；保持并发执行；串行队列已有 FIFO 不受影响 |
| B 队列级 FIFO 分发器 | 提交入队列本地链表，专门分发逻辑按序准入 | 重构更大，与现有 pool 提交路径冲突 |

选 A：最小改动达成顺序保证。

### D3 信号掩码

| 方案 | 做法 | 取舍 |
|---|---|---|
| **A 全局源信号集 + worker 启动/任务间应用（选定）** | 维护进程级 `g_source_sigset`；SIGNAL resume 时加入并阻塞调用线程；worker 启动时应用，任务间按 generation 计数重新应用 | 覆盖既有 worker；任务间窗口极小 |
| B MINIT 进程级阻塞候选信号 | 启动即阻塞 SIGUSR1/2 等 | 改变不用源的用户行为（Ctrl-C 失效），不可接受 |

选 A。残余窗口（信号恰在 worker 应用前到达）以测试验证主路径；如需绝对保证可后续加内核级处理，不在本次。

### D4 脚本符号崩溃

先以 lldb 定位 SIGBUS 根因（疑似 copy.c 对 op_array 字面量/缓存的浅拷贝指向主线程内存）。
目标行为：不可用符号 → 可捕获 Error。若根因是拷贝缺陷则修拷贝；若是 worker 函数表缺失，
确保查找走正常 zend 路径抛 Error 而非解引用野指针。实现期以调试任务推进。

## 逐项修复设计（文件级）

- **D1** `src/runtime/task.c` `gcd_task_build`：检查 `gcd_zval_copy_persistent(&pargv, argv)` 返回值，FAILURE 时抛 `gcd_serialization_ce` 并返回 NULL（调用方已处理 RETURN_THROWS）。
- **D2** `src/container/set.c` `make_key`：先做类型预检——资源（IS_RESOURCE）会被 php_var_serialize 序列化成 "i:0;" 而非失败，必须显式拒绝；闭包等序列化失败后 `buf.s` 为 NULL。两类均抛 `gcd_serialization_ce`。`add/contains/remove` 传播异常（返回前检查 EG(exception)）。
- **D3** `src/source/source.c` + `src/runtime/thread_pool.c`：全局 sigset + generation；resume 加入 signo、阻塞调用线程、bump generation；worker boot 与任务循环间 `pthread_sigmask(SIG_BLOCK, g_source_sigset)`。**必须覆盖空闲 worker**：parked 在 pthread_cond_wait 的 worker 到不了任务间应用点，需唤醒并重新掩码；非信号源线程（timer/poll）同样纳入。cancel/销毁后信号**保持阻塞**（与现状主线程行为一致，最小范围）；文档保留「源信号一旦启用即进程级阻塞」警示。按 signo 引用计数的恢复逻辑列为后续可选。
- **D4** `src/runtime/copy.c` / `task.c gcd_invoke`：正常 VM 路径对未定义函数本就会抛可捕获 Error（zend_vm_execute.h zend_throw_error），SIGBUS 说明拷贝后的 op_array 在到达该路径前崩溃。评审定位的具体嫌疑：(a) copy.c:131 对所有拷贝（含嵌套 dynamic_func_defs，copy.c:152-168）置空 run_time_cache__ptr，但 gcd_invoke 只为顶层 op_array 分配缓存——嵌套闭包执行任何带缓存 opcode（如 INIT_FCALL）解引用 NULL；(b) arg_info 类型指针浅拷贝（copy.c:119-127）；(c) IS_CONSTANT_AST 字面量拷贝失败。以 lldb + vendored PHP 源码定位后修拷贝/缓存初始化；补回归测试（任务调脚本函数 → handler 收到 Error，进程存活）。
- **D5** `src/queue/group.c` notify 任务创建：notify 任务**不递减计数**但**保留生命周期引用**——加 no-leave 标志（或独立 lifetime 字段），完成时仍 release group 以防 group_wait 返回后用户 unset group 造成 UAF（handler 为借用 owns_oa=0，由 group notify 列表持有）。notify **一次性**：触发时消费 notify 列表（真 GCD dispatch_group_notify 语义），后续批次不再重 fired；group 本身可跨批次复用。
- **D6** `src/queue/queue.c` `queue_set_target`：`gcd_queue_retain(t)`；替换/释放旧 target 时 release；queue free 时 release target。环检查保证目标图无环，release 级联终止。注：get_qos 无锁遍历 vs worker 可调 set_target 是既有竞争，D6 不修也不恶化，明确排除在本次范围外。
- **D7** `src/runtime/error.c:21`：用 `ex->ce`（或 Error 基类）读 message 属性，而非固定 `zend_ce_exception`。
- **D8** `src/runtime/error.c` `gcd_error_set_global` + `PHP_FUNCTION(set_error_handler)`：检查拷贝结果，FAILURE 抛 `gcd_capture_ce`（与 queue 版一致）。
- **D9** `src/source/source.c` `source_set_timer`：`interval` 默认 0；`source_thread` TIMER 分支 interval<=0 时触发一次后退出循环。
- **D10** `src/runtime/task.c:169`（评审更正：消息在 task.c 非 copy.c）：消息去掉 try-catch 字样，改为 captures/statics。
- **S1** `src/runtime/task.c` gate + 全部并发提交路径：gcd_task 加 seq；gcd_queue 加 next_seq/next_admit。**取号与入池必须原子**：持 width_mutex 跨越 gcd_pool_submit（锁序 width_mutex→g_mutex 安全：worker 先放 g_mutex 再取 width_mutex），否则 notify/source 线程的并发提交会 invert 池序与 seq 序，固定 worker 池下可致硬死锁。所有并发提交点统一走带序号的提交 helper：dispatch.c（async/sync/barrier_*/apply）、group.c（group_async + notify）、source.c（dispatch_handler + DATA_ADD）；width-1 drain job  bypass gate 不受影响。gate 按序准入（`t->seq == q->next_admit`），**每次准入都 broadcast width_cond**（不仅完成时）；barrier 额外等 inflight==0。
- **S2** `src/runtime/task.c:128`：`if (q) gcd_current_queue = q;`（去掉 width==1 条件）。sync 死锁保护仍仅 width==1，不变。

## 测试策略

- 每个 D/S 项新增 phpt 回归测试（tests/ 下按模块），先红后绿：
  - D1/D2：对象/闭包入 argv/Set → 期望 SerializationException（进程存活）。
  - D3：先 boot pool 再 resume SIGNAL → 发信号 → handler 计数 ≥1，进程存活。
  - D4：任务调脚本函数 → handler 收到 Error 类名，进程存活。
  - D5：notify 后复用 group → 第二批 group_wait 真正等待。
  - D6：unset target 后 get_qos 仍返回继承值。
  - D7：任务抛 Error/GCD 异常 → handler 收到非空 message。
  - D8：捕获闭包注册全局处理器 → CaptureException。
  - D9：省略 interval → 恰好触发一次。
  - S1：barrier 后提交的任务不早于 barrier（多次运行稳定）。
  - S2：并发队列任务内 current_queue 非 null。
- 现有 29 测试保持绿；TSan 套件 0 警告（docker 路线）。
- 文档门禁：132 示例、20/20 parity、锚点、禁捕获、链接。

## 评审决议（builder-check 四问，均由实现者裁定）

1. **S1 原子提交**：持 width_mutex 跨 gcd_pool_submit，全部并发提交点走带序号 helper，准入即 broadcast（见 S1）。
2. **D5 生命周期**：no-leave 标志保留 lifetime 引用，防 UAF（见 D5）。
3. **D5 notify 一次性**：触发消费列表，匹配真 GCD（用户「连语义一起补」意图）；group 可复用。
4. **D3 cancel 处置**：信号保持阻塞（匹配现状主线程行为，最小范围）；按 signo 引用计数恢复列为后续可选。

评审更正已并入：D2 资源需类型预检；D10 消息在 task.c:169；D3 须覆盖空闲 worker 与源线程；D4 具体嫌疑（嵌套 run_time_cache 置空等）。

## 留给实现者的决定

- D4 的具体根因与修法（lldb 结论驱动）。
- S1 序号字段类型/溢出处理（uint64 足够）。
- D3 generation 同步细节与 worker 循环插入点。
- 各 phpt 的断言措辞与目录归属。
- 文档同步的具体措辞（移除哪些警示、保留哪些仍成立的限制）。