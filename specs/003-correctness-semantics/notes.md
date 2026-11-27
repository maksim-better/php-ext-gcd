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

## 其余各项结论

（Task 11 汇总；D1/D2/D6/D7/D8/D9/D10/S2 已修复并回归绿。）