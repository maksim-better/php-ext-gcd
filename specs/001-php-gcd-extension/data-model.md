# Data Model: PHP GCD 并发扩展

**Source**: [spec.md](./spec.md) Key Entities + Functional Requirements. 描述概念实体与约束（C 结构的具体布局属实现期）。

## 1. Dispatch Queue（派发队列）

调度上下文，与执行线程解耦。

| 字段 | 说明 | 约束 |
|------|------|------|
| kind | serial / concurrent | FR-001/002 |
| width | 并发宽度（serial 恒为 1） | concurrent ≥1；FR-002 |
| target_queue | 目标队列（层级） | 最终指向全局根队列；FR-022 |
| qos | QoS 档位 | Linux best-effort；FR-021 |
| state | active / suspended | suspend 期间不派发；FR-013 |
| specific | 队列专属上下文键值 | FR-023 |
| items | 待执行任务 FIFO 链表 | drain lock 保护 |
| error_handler | 可选 per-queue 错误回调 | 缺省走全局；FR-030 |

**状态转移**: active ⇄ suspended（suspend/resume）；drain 中由抢到 drain lock 的线程排空（serial 一次一个）。
**关系**: 持有 Task 链表；target 指向另一 Queue；绑定 Thread Pool 执行。

## 2. Task / Block（任务/块）

工作单元。

| 字段 | 说明 | 约束 |
|------|------|------|
| closure | PHP 闭包 | 不得捕获外部变量（投递时校验拒绝）；FR-029 |
| argv | 可序列化参数数组 | 标量/数组；对象序列化拷贝 |
| qos | 任务级 QoS | 可覆盖队列 QoS |
| cancelled | 协作式取消标志 | 任务主动检查；FR-017 |
| group | 所属 group（可选） | enter/leave 计数 |
| kind | async / sync / barrier / after | FR-005/006/009/014 |

**状态转移**: pending → running → done / cancelled / failed（异常被接住）。
**验证**: 闭包捕获检测（use/static vars）→ 拒绝；argv 可序列化校验。

## 3. Dispatch Group（任务组）

| 字段 | 说明 |
|------|------|
| count | 原子计数（enter+/leave-） |
| notifications | 归零时投递的通知任务链表 |
| errors | 聚合的成员任务异常 |

**状态转移**: count>0 → count==0 触发 notify；wait 阻塞至归零（可超时）；FR-010。

## 4. Semaphore（信号量）

| 字段 | 说明 |
|------|------|
| value | 计数值（原子） |

**操作**: signal(+1)；wait(-1 或阻塞，可超时)；FR-011。

## 5. Dispatch Source（事件源）

| 字段 | 说明 | 约束 |
|------|------|------|
| type | timer/read/write/signal/proc/vnode/data_add·or·replace | 跨平台子集，排除 Darwin 专属（Mach/内存压力）；proc/vnode Linux 经 pidfd/inotify；FR-015/035 |
| handle | 监听对象（fd/信号号/pid/路径） | 按 type |
| mask | 事件掩码/聚合方式 | data 源累加/或/替换 |
| handlers | event / cancel / registration 回调 | FR-016 |
| state | suspended / active / cancelled | resume 后开始监听 |
| backend | epoll(Linux) / kqueue(macOS) | 平台映射 |

**状态转移**: created →(resume) active →(cancel) cancelled；事件到达触发 event handler。

## 6. Dispatch Data（不可变字节缓冲）

| 字段 | 说明 |
|------|------|
| bytes | 不可变字节序列 |
| regions | 区域列表（支持拼接/子范围/映射） |

**约束**: 不可变；concat/subrange/map/apply 产生新对象；FR-019。

## 7. Shared Container（共享容器）

线程安全，值限可序列化（标量/数组；对象序列化拷贝；拒绝资源）。FR-024–028。

| 容器 | 操作 | 语义 |
|------|------|------|
| ConcurrentQueue | push / pop(timeout) / tryPop | MPMC；阻塞 pop 超时返回空，tryPop 立即返回空 |
| ConcurrentSet | add / contains | 去重，幂等 add |
| AtomicCounter | inc / dec / get | 原子，无丢失更新 |
| ConcurrentMap | set / get / delete | 键值，线程安全 |

**验证**: 存入资源/不可序列化值 → 拒绝报错（FR-028）。
**并发约束**: ≥64 线程高压零竞态、零丢失更新（SC-002）。

## 8. Thread Pool / Worker Thread（线程池/工作线程）

| 字段 | 说明 |
|------|------|
| workers | 工作线程集合 |
| size | 池大小（可配，全局队列） |
| php_context | 每线程独立 Zend 运行时上下文 |

**约束**: 每线程独立 PHP 上下文，任务间不共享活对象/资源（FR-032）；致命错误→进程终止+外部监督（FR-036）。

## 9. Error Reporting（错误上报，三层）

| 层 | 说明 |
|----|------|
| global handler | 全局默认兜底，确保不静默吞 |
| per-queue handler | 可选覆盖 |
| group aggregation | 批量任务异常聚合 |

**规则**: 可捕获异常经三层上报，不崩进程；致命错误走 FR-036（FR-030）。

## 10. Main Loop（主循环，双模型）

| 模型 | 说明 |
|------|------|
| dispatch_main | 阻塞主线程，回调（source/timer/async）驱动生产 |
| manual drain/step | 开发者自定义 while 循环，非阻塞推进派发 |

**约束**: 二者并存，均支持外部队列消费+进程内生产（FR-031）。

## 实体关系概览

```text
Thread Pool ──执行──> Dispatch Queue ──持有──> Task/Block
                          │ target                │ group
                          ▼                        ▼
                     Dispatch Queue          Dispatch Group ──聚合──> errors
Dispatch Source ──事件回调──> Task 生产
Shared Container <──读写── Task（跨任务协作）
Error Reporting <──异常── Task
Main Loop ──驱动──> Dispatch Queue / Source
```