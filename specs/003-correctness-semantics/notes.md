# 正确性修复实现备注

## D4 脚本符号崩溃：根因已定位，本轮延期

**现象**：任务闭包调用脚本级函数（worker 函数表中不存在）→ SIGBUS（exit 138）。
闭包内定义的函数、内部函数（strlen 等）均正常。

**根因（lldb + vendored PHP 源码核实）**：
- 崩溃点 `ZEND_INIT_FCALL_SPEC_CONST_TAILCALL_HANDLER+184`，`ldr x8,[x25,#0x18]`，
  x25=0x2118037fe0（map_ptr 区坏偏移），栈：execute_ex ← zend_call_function ← gcd_invoke(task.c:93)。
- 编译器对「同一脚本内定义的函数」调用施加 TAILCALL 特化；该特化 handler 含
  `ZEND_ASSERT(func != NULL && "Function existence must be checked at compile time")`
  （docker/php-src/Zend/zend_vm_execute.h:4040 基 handler）。release 构建断言为空操作，
  worker 函数表缺少该脚本函数时查找失败 → 解引用野指针。
- 已排除：run_time_cache 初始化（插桩证实 cs=8、cache=func_rtc=clos_rtc 同为合法堆指针）；
  RT_CONSTANT 字面量寻址（64 位用 opline 偏移变体，copy.c 已保持距离）；
  字面量字符串哈希（补算后仍崩溃，已撤回）。

**为何延期**：修复需 VM 级工作（让特化 handler 对缺失函数优雅抛 Error，或按 PHP 版本
归一化 opcode 特化，或向 worker 拷贝脚本函数表），依赖 debug 构建定位且跨版本脆弱，
超出本轮安全范围。

**安全替代（文档已载明）**：任务内不要调用脚本级函数/类/常量；把辅助函数定义在任务
闭包内部，或在任务里 `require`/autoload。

**后续建议**：用 debug ZTS PHP（docker 路线）复现，评估在 gcd_op_array_copy_persistent
中归一化 INIT_FCALL 特化 opcode 到通用 handler（通用路径对未定义函数抛可捕获 Error）。

## 其余各项结论（Task 11 汇总）

| 项 | 状态 | 验证 |
|---|---|---|
| D1 argv 对象 | 已修复 | argv_object.phpt：抛 SerializationException，进程存活 |
| D2 Set 闭包/资源 | 已修复 | set_nonserializable.phpt：抛 SerializationException（资源类型预检 + 序列化失败检查） |
| D3 信号掩码 | 已修复 | signal_after_pool.phpt：池启动后 resume 信号源，进程存活、handler 触发；全局掩码 + generation + 空闲 worker 唤醒重掩码 + 源线程应用 |
| D4 脚本符号 | 延期 | 见上；文档保留警示 |
| D5 notify | 已修复 | group_notify_reuse.phpt：group 可复用、notify 一次性；no_leave 标志保留生命周期引用；UAF 压力 30 轮无崩溃 |
| D6 target 生命周期 | 已修复 | target_lifetime.phpt：unset 目标后 QoS 链仍有效；retain/release + 释放级联 |
| D7 处理器消息 | 已修复 | handler_message.phpt：Error 系收到真实 message（按 ex->ce 读属性） |
| D8 全局处理器 | 已修复 | global_handler_capture.phpt：捕获闭包抛 CaptureException |
| D9 timer 默认 | 已修复 | timer_oneshot.phpt：省略 interval 恰好触发一次 |
| D10 过时消息 | 已修复 | capture_message.phpt：消息不再含 try-catch |
| S1 barrier 顺序 | 已修复 | barrier_order.phpt：20 轮 0 违序；序号准入 + 原子提交 + 准入 broadcast；并发压力 3 轮无死锁 |
| S2 current_queue | 已修复 | current_queue_concurrent.phpt：并发队列任务内非 null |

**回归**：release 套件 40/40（29 原有 + 11 新增）；TSan 套件 36/36，src/ 数据竞争 0
（docker/tsan-test.sh 需先 `phpize --clean` 再生成容器内 Makefile，否则残留宿主路径——脚本既有问题）。
文档门禁：132 示例、20/20 parity、70/70 锚点、零捕获、零断链。