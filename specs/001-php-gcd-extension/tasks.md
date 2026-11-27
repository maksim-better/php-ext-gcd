---
description: "Task list for PHP GCD 并发扩展 implementation"
---

# Tasks: PHP GCD 并发扩展

**Input**: Design documents from `/specs/001-php-gcd-extension/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/php-api.md, quickstart.md

**Tests**: 包含测试任务——spec 明确要求（SC-002 压力测试、SC-007 100% API 覆盖、quickstart V1–V9）。PHPT 语义测试 + 并发压力测试。

**Organization**: 按 user story 分组，每个 story 独立可实施/可测试。研究 spike（S1–S4）作为阻塞性前置任务。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行（不同文件、无未完成依赖）
- **[Story]**: 所属 user story（US1–US6）；Setup/Foundational/Polish 无 story 标签
- 每个任务含确切文件路径

## Path Conventions

单一 C 扩展项目：`src/`、`tests/`、`examples/`、`spikes/`、`docker/`、`benchmarks/` 于仓库根（见 plan.md 结构）。

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: 项目初始化与构建/CI 基础

- [ ] T001 创建扩展骨架 config.m4 / php_gcd.h / gcd.c（zend_module_entry、MINIT/MSHUTDOWN）于仓库根
- [ ] T002 [P] ZTS 检测并拒绝 NTS 构建于 config.m4
- [ ] T003 [P] ZTS PHP 构建镜像（Linux --enable-zts，PHP 8.2/8.3/8.4）于 docker/
- [ ] T004 [P] CI 矩阵 Linux+macOS ZTS 于 .github/workflows/ci.yml
- [ ] T005 [P] 许可证与 notices：LICENSE（PHP License 3.01）+ NOTICE（Apache-2.0 借鉴代码保留）于仓库根
- [ ] T006 [P] 钉死目标 PHP 版本并记录于 README.md（research R1 版本漂移风险）
- [ ] T007 [P] 测试 harness：run-tests.php 集成、tests/ 子目录、sanitizer 构建标志于 config.m4

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 核心运行时 + 可行性 spike，所有 user story 的前置

**⚠️ CRITICAL**: 本阶段完成前任何 user story 不得开工。spike 先行验证最高风险（research Top Risks）。

### 可行性 Spikes（先验证再实现）

- [ ] T008 S1 spike：目标版本最小 ZTS 每线程 boot（ts_resource(0)+TSRMLS_CACHE_UPDATE()+php_request_startup+跑闭包）于 spikes/s1_zts_boot.c
- [ ] T009 [P] S2 spike：N 线程 curl 压测（crash/leak/SIGPIPE/resolver/CURL_VERSION_THREADSAFE）于 spikes/s2_curl_threads.c
- [ ] T010 [P] S3 spike：跨线程深拷贝边界（COW/refcount、循环回收、拒绝谓词）于 spikes/s3_value_copy.c
- [ ] T011 [P] S4 spike：信号所有权与宿主 SAPI 共存（SIGSEGV/SIGCHLD/SIGPIPE）于 spikes/s4_signals.c

### 核心运行时

- [ ] T012 实现每线程 PHP 上下文 boot/teardown（一次 request/线程）于 src/runtime/php_context.c（依赖 T008）
- [ ] T013 实现线程池（create/size/submit/shutdown，稳定 thread↔runtime 绑定）于 src/runtime/thread_pool.c（依赖 T012）
- [ ] T014 [P] 实现值序列化/深拷贝边界（persistent↔thread，拒绝资源）于 src/serialize.c（依赖 T010）
- [ ] T015 实现任务模型（闭包+argv、捕获拒绝 CaptureException、异常接住）于 src/runtime/task.c（依赖 T012, T014）
- [ ] T016 [P] 实现三层错误上报基础（全局/per-queue/group）于 src/error.c
- [ ] T017 [P] 实现信号所有权策略于 src/runtime/signal.c（依赖 T011）
- [ ] T018 实现主循环双模型（GCD\main 阻塞 + drain/run_once 非阻塞）于 src/loop.c（依赖 T013）
- [ ] T019 [P] 基础 smoke PHPT：工作线程跑闭包于 tests/foundational/boot.phpt
- [ ] T020 注册 GCD 命名空间函数/类骨架 + 异常类（CaptureException/DeadlockException/SerializationException）于 gcd.c

**Checkpoint**: 运行时就绪——能在受控线程池上跑隔离闭包；user story 可开工

---

## Phase 3: User Story 1 - 并行消费任务 (Priority: P1) 🎯 MVP

**Goal**: 队列（串行/并发）+ async/sync + 全局/主队列 + 常驻循环 → 并行任务消费

**Independent Test**: 宽度 4 并发队列在途 ≤4 且并行；串行严格 FIFO；向当前串行队列 sync 抛 DeadlockException；双主循环模型常驻运行

### Tests for User Story 1 ⚠️（先写并确认 FAIL）

- [ ] T021 [P] [US1] PHPT 串行队列 FIFO 保序于 tests/queue/serial_order.phpt
- [ ] T022 [P] [US1] PHPT 并发队列并行度/宽度上限于 tests/queue/concurrent_width.phpt
- [ ] T023 [P] [US1] PHPT async 返回 void / sync 阻塞于 tests/queue/async_sync.phpt
- [ ] T024 [P] [US1] PHPT 向当前串行队列 sync 死锁检测于 tests/queue/sync_deadlock.phpt
- [ ] T025 [P] [US1] PHPT 双主循环模型（dispatch_main + 手动 drain）于 tests/queue/loop_models.phpt

### Implementation for User Story 1

- [ ] T026 [P] [US1] 实现队列结构（serial/concurrent、width、drain lock、items FIFO）于 src/queue/queue.c
- [ ] T027 [US1] 实现 dispatch async/sync（含 _f）+ 死锁检测于 src/queue/dispatch.c（依赖 T026）
- [ ] T028 [US1] 实现全局队列（pool_size 可配）+ 主队列于 src/queue/queue.c（依赖 T026）
- [ ] T029 [US1] 队列调度接入线程池 + loop drain于 src/queue/dispatch.c（依赖 T027, T018）
- [ ] T030 [US1] 注册 queue/dispatch/loop PHP API（queue_create/global_queue/main_queue/async/sync/async_f/sync_f/main/drain/run_once）于 gcd.c
- [ ] T031 [US1] 示例 parallel_consume.php / loop_callback.php / loop_manual.php 于 examples/

**Checkpoint**: US1 独立可用——并行任务消费 MVP 成立

---

## Phase 4: User Story 2 - 任务间共享状态 (Priority: P2)

**Goal**: 线程安全共享容器（队列/集合/原子计数器/map）

**Independent Test**: 64 线程计数器零丢失更新；集合去重；MPMC 队列恰消费一次；阻塞 pop 超时/tryPop；存入资源抛 SerializationException

### Tests for User Story 2 ⚠️

- [ ] T032 [P] [US2] PHPT 并发队列 MPMC + 阻塞 pop/tryPop于 tests/container/queue.phpt
- [ ] T033 [P] [US2] PHPT 集合去重于 tests/container/set.phpt
- [ ] T034 [P] [US2] PHPT 原子计数器无丢失更新于 tests/container/atomic.phpt
- [ ] T035 [P] [US2] PHPT map set/get/delete于 tests/container/map.phpt
- [ ] T036 [P] [US2] PHPT 拒绝资源/不可序列化值于 tests/container/illegal_value.phpt
- [ ] T037 [P] [US2] 64 线程容器压力测试（TSan）于 tests/stress/container_stress.php

### Implementation for User Story 2

- [ ] T038 [P] [US2] 实现 MPMC 并发队列（mutex+condvar，pop 超时/tryPop）于 src/container/queue.c
- [ ] T039 [P] [US2] 实现并发集合于 src/container/set.c
- [ ] T040 [P] [US2] 实现原子计数器于 src/container/atomic.c
- [ ] T041 [P] [US2] 实现并发 map于 src/container/map.c
- [ ] T042 [US2] 容器值序列化边界 + 拒绝谓词接入于 src/container/queue.c（依赖 T014）
- [ ] T043 [US2] 注册 GCD\Queue/Set/Atomic/Map 类于 gcd.c

**Checkpoint**: US1+US2 独立可用——有状态并发应用（爬虫基础）成立

---

## Phase 5: User Story 3 - 协调一批任务 (Priority: P3)

**Goal**: group / barrier / semaphore / once / suspend·resume / queue-specific / assert

**Independent Test**: group 全 leave 后 wait/notify；初值 3 信号量 ≤3 在执行段；barrier 排空独占；once 恰一次；suspend 停派发

### Tests for User Story 3 ⚠️

- [ ] T044 [P] [US3] PHPT group enter/leave/wait/notify于 tests/sync/group.phpt
- [ ] T045 [P] [US3] PHPT 信号量并发上限于 tests/sync/semaphore.phpt
- [ ] T046 [P] [US3] PHPT barrier 排空独占于 tests/sync/barrier.phpt
- [ ] T047 [P] [US3] PHPT once 恰一次于 tests/sync/once.phpt
- [ ] T048 [P] [US3] PHPT suspend/resume + queue-specific + assert于 tests/sync/queue_control.phpt

### Implementation for User Story 3

- [ ] T049 [P] [US3] 实现 group（原子计数 + notify 链表 + 异常聚合）于 src/queue/group.c
- [ ] T050 [P] [US3] 实现 semaphore（计数 + wait 超时）于 src/queue/semaphore.c
- [ ] T051 [US3] 实现 barrier async/sync（排空在途后独占）于 src/queue/dispatch.c（依赖 T026）
- [ ] T052 [P] [US3] 实现 once（按 key 原子门）于 src/queue/once.c
- [ ] T053 [US3] 实现 suspend/resume + queue-specific + assert_queue于 src/queue/queue.c
- [ ] T054 [US3] 注册 group/semaphore/barrier/once/suspend/resume/specific/assert PHP API于 gcd.c
- [ ] T055 [US3] 示例 coordination.php 于 examples/

**Checkpoint**: 并发可协调/限流/同步

---

## Phase 6: User Story 4 - 定时与事件驱动 (Priority: P4)

**Goal**: after / dispatch_source（epoll/kqueue 后端）/ block 对象取消

**Independent Test**: after 不早于延时；timer source 周期触发；cancel 后任务不执行且 test_cancel 真；proc source 进程退出回调

### Tests for User Story 4 ⚠️

- [ ] T056 [P] [US4] PHPT after 延时（相对/墙钟）于 tests/source/after.phpt
- [ ] T057 [P] [US4] PHPT timer source 周期于 tests/source/timer.phpt
- [ ] T058 [P] [US4] PHPT read/write/signal/proc/vnode/data 源于 tests/source/sources.phpt
- [ ] T059 [P] [US4] PHPT block cancel/test_cancel/wait/notify于 tests/source/block.phpt

### Implementation for User Story 4

- [ ] T060 [P] [US4] 实现事件后端抽象接口于 src/source/source.c
- [ ] T061 [P] [US4] 实现 epoll 后端（epoll+eventfd+signalfd+timerfd+muxnote）于 src/source/backend_epoll.c
- [ ] T062 [P] [US4] 实现 kqueue 后端（macOS）于 src/source/backend_kqueue.c
- [ ] T063 [US4] 实现 source 生命周期（create/resume/suspend/cancel/handlers）于 src/source/source.c（依赖 T060）
- [ ] T064 [US4] 实现 proc（pidfd/waitpid）+ vnode（inotify）Linux 自定义后端于 src/source/backend_epoll.c（依赖 T061）
- [ ] T065 [P] [US4] 实现 timer source + after/after_walltime于 src/source/timer.c
- [ ] T066 [P] [US4] 实现 block 对象（create/wait/notify/cancel/test_cancel）于 src/queue/block.c
- [ ] T067 [US4] 事件后端接入主循环 drain于 src/loop.c（依赖 T063, T018）
- [ ] T068 [US4] 注册 after/source/block PHP API + source 类型常量（排除 MEMORYPRESSURE）于 gcd.c
- [ ] T069 [US4] 示例 events.php 于 examples/

**Checkpoint**: 事件驱动常驻应用成立

---

## Phase 7: User Story 5 - 并行迭代与字节 IO (Priority: P5)

**Goal**: apply / dispatch_data / dispatch_io

**Independent Test**: apply 并行 N 次迭代且调用线程等待全部；data 不可变 concat/subrange 正确；io 读写 fd 正确

### Tests for User Story 5 ⚠️

- [ ] T070 [P] [US5] PHPT apply 并行迭代于 tests/io/apply.phpt
- [ ] T071 [P] [US5] PHPT dispatch_data 不可变操作于 tests/io/data.phpt
- [ ] T072 [P] [US5] PHPT dispatch_io stream/random 读写于 tests/io/io.phpt

### Implementation for User Story 5

- [ ] T073 [US5] 实现 apply（并行 for，调用线程参与+等待）于 src/queue/dispatch.c（依赖 T026）
- [ ] T074 [P] [US5] 实现 dispatch_data（不可变缓冲，concat/subrange/map/apply）于 src/io/data.c
- [ ] T075 [P] [US5] 实现 dispatch_io（stream/random，read/write/close/barrier）于 src/io/io.c
- [ ] T076 [US5] 注册 apply/data/io PHP API + IO 类型常量于 gcd.c

**Checkpoint**: 全量 GCD 并行计算与底层 IO 补齐

---

## Phase 8: User Story 6 - 优先级与队列层级 (Priority: P6)

**Goal**: QoS / target queue 层级 / queue attr

**Independent Test**: 高 QoS 队列优先调度（Linux best-effort）；target queue 层级 QoS 传播

### Tests for User Story 6 ⚠️

- [ ] T077 [P] [US6] PHPT QoS 优先调度于 tests/qos/priority.phpt
- [ ] T078 [P] [US6] PHPT target queue 层级 + QoS 传播于 tests/qos/hierarchy.phpt

### Implementation for User Story 6

- [ ] T079 [P] [US6] 实现 QoS 档位 + Linux best-effort（pthread sched）于 src/qos.c
- [ ] T080 [US6] 实现 target queue 层级 + QoS 传播于 src/queue/queue.c（依赖 T026）
- [ ] T081 [US6] 实现 queue attr（qos+relative priority）+ qos_class_self于 src/qos.c
- [ ] T082 [US6] 注册 QoS/target/attr PHP API + QoS 常量于 gcd.c

**Checkpoint**: 全量跨平台 GCD 表面完成

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: 跨 story 验证与收尾

- [ ] T083 [P] 常驻爬虫 demo（外部队列消费+内部链接生产+并发爬取+共享去重/frontier/计数）于 examples/crawler/crawler.php
- [ ] T084 [P] 致命错误重启验证监督配置于 examples/crawler/supervisord.conf
- [ ] T085 [P] I/O 吞吐基准（对比 ext-parallel/swoole 协程）于 benchmarks/io_throughput.php
- [ ] T086 24h 爬虫长跑 + ASan/valgrind 泄漏验证（SC-004）于 examples/crawler/
- [ ] T087 跨平台 CI 绿（Linux+macOS，SC-006）+ 100% API 覆盖审计（SC-007）于 .github/workflows/ci.yml
- [ ] T088 [P] 错误上报三层集成测试于 tests/error/reporting.phpt
- [ ] T089 [P] 文档：README、API 文档、ZTS 构建指南于 docs/
- [ ] T090 运行 quickstart.md V1–V9 端到端验证于 specs/001-php-gcd-extension/quickstart.md

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 无依赖，立即开始
- **Foundational (Phase 2)**: 依赖 Setup；**阻塞所有 user story**。spike（T008–T011）先于核心运行时
- **User Stories (Phase 3+)**: 均依赖 Foundational
  - **US2 仅依赖 Foundational**（容器只需运行时+序列化），可与 US1 并行
  - **US3–US6 依赖 US1 的队列层**（T026）：barrier/suspend/source/apply/target 均操作队列
- **Polish (Phase 9)**: 依赖所需 user story 完成

### User Story Dependencies

- **US1 (P1)**: Foundational 后即可开始，无其他 story 依赖
- **US2 (P2)**: Foundational 后即可开始，独立可测（与 US1 并行）
- **US3 (P3)**: 依赖 US1 队列层（T026）
- **US4 (P4)**: 依赖 US1（队列/loop）+ Foundational
- **US5 (P5)**: 依赖 US1 队列层（T026）
- **US6 (P6)**: 依赖 US1 队列层（T026）

### Within Each User Story

- 测试先写并确认 FAIL → 实现
- 结构/模型 → 调度/服务 → API 注册 → 示例
- 同文件任务串行（如 queue.c/dispatch.c/gcd.c 跨阶段顺序编辑）

### Parallel Opportunities

- Setup 所有 [P] 任务并行
- Foundational spike T009–T011 并行（T008 先行钉版本）；核心运行时 T014/T016/T017/T019 并行
- Foundational 完成后 **US1 与 US2 可并行**
- 各 story 内 [P] 测试并行、不同文件的实现任务并行
- US3–US6 在 US1 完成后可并行（不同子系统文件）

---

## Parallel Example: User Story 1

```bash
# 并行写 US1 测试：
Task: "PHPT 串行保序 tests/queue/serial_order.phpt"
Task: "PHPT 并发宽度 tests/queue/concurrent_width.phpt"
Task: "PHPT async/sync tests/queue/async_sync.phpt"
Task: "PHPT 死锁检测 tests/queue/sync_deadlock.phpt"
Task: "PHPT 双循环 tests/queue/loop_models.phpt"
```

## Parallel Example: Foundational Spikes

```bash
# T008 钉版本后，并行验证其余高风险：
Task: "S2 curl 多线程 spikes/s2_curl_threads.c"
Task: "S3 值拷贝边界 spikes/s3_value_copy.c"
Task: "S4 信号所有权 spikes/s4_signals.c"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 1 Setup
2. Phase 2 Foundational（**关键**：spike 验证 + 核心运行时，阻塞所有 story）
3. Phase 3 US1
4. **STOP & VALIDATE**：独立测试 US1（并行消费）
5. 可演示 MVP

### Incremental Delivery

1. Setup + Foundational → 运行时就绪
2. US1 → 独立测试 → 并行消费 MVP
3. US2 → 独立测试 → 有状态并发（爬虫基础）
4. US3 → 协调/限流
5. US4 → 事件驱动
6. US5 → 并行 IO
7. US6 → QoS 层级（全量表面完成）
8. Polish → 爬虫长跑/基准/跨平台验证

### Parallel Team Strategy

1. 团队共同完成 Setup + Foundational
2. Foundational 后：Dev A → US1，Dev B → US2（并行）
3. US1 完成后：US3/US4/US5/US6 可分派并行
4. 各 story 独立集成

---

## Notes

- [P] = 不同文件、无未完成依赖；同文件任务串行
- [Story] 标签映射 traceability；Setup/Foundational/Polish 无标签
- 测试先写并确认 FAIL（spec 要求测试覆盖）
- 每任务/逻辑组后提交；checkpoint 处可独立验证
- **最高风险优先**：spike S1（版本 API）、S3（值拷贝边界）是新颖风险面，务必先验证
- 避免： vague 任务、同文件冲突、破坏独立性的跨 story 依赖