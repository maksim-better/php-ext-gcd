# Feature Specification: PHP GCD 并发扩展

**Feature Branch**: `001-php-gcd-extension`

**Created**: 2026-11-27

**Status**: Draft

**Input**: User description: ".specify/sp-brainstorm/php-gcd-extension/brief.md（PHP GCD 并发扩展头脑风暴简报）"

## Clarifications

### Session 2026-11-27

- Q: 常驻进程用什么方式持续生产任务？ → A: 两者并存——阻塞式 `dispatch_main`（回调驱动）+ 手动单步 drain/step API（自定义主循环），开发者按应用类型自选。
- Q: 工作线程致命错误（segfault/fatal）如何处理？ → A: 进程级崩溃 + 外部监督重启（systemd/supervisord）；不做线程级隔离复活。
- Q: 共享容器空 pop 及 semaphore/group 等待的阻塞语义？ → A: 阻塞(可超时) + 非阻塞双 API——blocking pop(timeout) 与 tryPop 并存，semaphore/group wait 带超时。
- Q: 任务异常的上报 API 形态？ → A: 全局默认 handler 兜底 + 可选 per-queue 覆盖 + group 聚合（三层并存，未设局部 handler 走全局）。
- Q: async 返回值形态？ → A: 仅 void（GCD 原味），结果走共享容器/回调；future 风格留作 v1 后增强。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - 并行消费任务 (Priority: P1)

PHP 开发者构建一个常驻 CLI 进程，把一批 I/O 任务投递到并发队列，任务在底层线程池上真正并行执行；进程通过常驻主循环持续运行，不断接收并派发任务。这是整个扩展的最小可用核心。

**Why this priority**: 没有"队列 + 线程池 + 并行派发 + 常驻循环"，其余一切无从谈起。单独实现这一条即交付一个可用的并行任务运行时。

**Independent Test**: 向并发队列投递 N 个带可观测标记的任务，验证它们并行执行（同时在途数达到配置宽度）、结果可回收；向串行队列投递任务验证严格保序。

**Acceptance Scenarios**:

1. **Given** 一个宽度为 4 的并发队列，**When** 开发者异步投递 8 个任务，**Then** 任务并行执行，任一时刻在途任务数不超过 4，且全部最终完成。
2. **Given** 一个串行队列，**When** 开发者按序投递任务 A、B、C，**Then** 执行顺序严格为 A→B→C，一次一个。
3. **Given** 一个常驻进程，**When** 开发者启动 `dispatch_main` 并持续投递任务，**Then** 进程不退出且任务持续被派发。
4. **Given** 当前正执行于某串行队列，**When** 开发者向该队列同步投递任务，**Then** 系统报错而非死锁挂起。
5. **Given** 一个自定义 while 主循环，**When** 开发者反复"拉外部队列→投递任务→调用 drain/step"，**Then** 任务被非阻塞地推进派发，循环可长期运行。

---

### User Story 2 - 任务间共享状态 (Priority: P2)

开发者使用扩展提供的线程安全共享容器，让并发任务协作：用并发队列做 URL frontier、并发集合做去重、原子计数器做进度统计、并发 map 做结果池——无需外置 Redis，进程内即可安全共享。

**Why this priority**: 这是目标爬虫场景区别于"无状态并行"的关键，也是相对 ext-parallel 的核心差异化价值。

**Independent Test**: 多线程高压向同一容器并发读写，验证无数据竞争、无丢失更新；用去重集合验证重复任务被拦截。

**Acceptance Scenarios**:

1. **Given** 一个并发集合，**When** 多个任务并发 add 同一元素，**Then** 集合中该元素仅出现一次，contains 返回一致结果。
2. **Given** 一个原子计数器初值 0，**When** 64 个任务各 inc 1000 次，**Then** 最终计数恰为 64000，无丢失更新。
3. **Given** 一个并发队列，**When** 多生产者多消费者并发 push/pop，**Then** 每个元素恰被消费一次，无重复无丢失。
4. **Given** 一个共享容器，**When** 开发者尝试存入资源或不可序列化值，**Then** 系统拒绝并报错。
5. **Given** 一个空的并发队列，**When** 一个任务调用阻塞 pop（超时 1s）且另一任务在 200ms 后 push，**Then** pop 在 push 后返回该元素；若改用 tryPop 则立即返回空。

---

### User Story 3 - 协调一批任务 (Priority: P3)

开发者用任务组聚合一批爬取任务并等待全部完成、用信号量限制同时进行的请求数、用栅栏任务在并发队列上独占访问共享状态、用 once 做一次性初始化。

**Why this priority**: 让并发从"散乱并行"变为"可协调、可限流、可同步"，是生产级并发应用的必需。

**Independent Test**: group wait/notify 在批量任务完成后触发；semaphore 将并发上限钳制在 N；barrier 执行期间无其他任务并行。

**Acceptance Scenarios**:

1. **Given** 一个任务组 enter 了 5 个任务，**When** 全部 leave 后，**Then** wait 返回、notify 任务被投递。
2. **Given** 一个初值 3 的信号量，**When** 10 个任务各 wait 后执行，**Then** 任一时刻最多 3 个任务处于执行段。
3. **Given** 一个并发队列上有在途任务，**When** 投递 barrier 任务，**Then** barrier 等待在途任务排空后独占执行，期间无并行。
4. **Given** 一个 once 块，**When** 多线程并发触发，**Then** 初始化恰好执行一次。

---

### User Story 4 - 定时与事件驱动 (Priority: P4)

开发者延时投递任务实现重试退避，用事件源监听定时器/fd 可读可写/信号/进程退出/文件变化等事件驱动任务生产，用可取消任务块撤销尚未执行的工作。

**Why this priority**: 支撑"进程内生产任务"与事件驱动的常驻应用，是爬虫持续运转与优雅控制的基础。

**Independent Test**: after 在指定延时后执行；timer source 周期触发；fd read source 在可读时回调；cancel 后任务不再执行。

**Acceptance Scenarios**:

1. **Given** 一个 after 延时 500ms 的任务，**When** 时间到达，**Then** 任务被投递执行，不早于延时。
2. **Given** 一个周期 timer source，**When** 启动主循环，**Then** 按周期触发事件回调。
3. **Given** 一个已投递但未执行的可取消任务块，**When** 开发者 cancel，**Then** 任务不再执行，test_cancel 返回真。
4. **Given** 一个监听进程退出的 source，**When** 目标进程退出，**Then** 事件回调被触发。

---

### User Story 5 - 并行迭代与字节 IO (Priority: P5)

开发者用并行迭代（apply）把循环分发到并发队列加速处理，用不可变字节缓冲与 fd 级 IO 做流式/随机读写。

**Why this priority**: 补齐 GCD 的并行计算与底层 IO 能力，覆盖全量表面；对爬虫属增强而非必需。

**Independent Test**: apply 对 N 次迭代并行执行且调用线程等待全部完成；dispatch_io 对 fd 读写结果正确。

**Acceptance Scenarios**:

1. **Given** 一个并发队列与 100 次迭代，**When** 开发者 apply，**Then** 迭代并行执行，调用线程在全部完成后返回。
2. **Given** 一个 dispatch_data 字节缓冲，**When** 拼接/取子范围/遍历，**Then** 内容正确且原缓冲不可变。
3. **Given** 一个 stream 类型 dispatch_io，**When** 读写某 fd，**Then** 数据按序正确传输。

---

### User Story 6 - 优先级与队列层级 (Priority: P6)

开发者为队列/任务设置 QoS 优先级，并通过目标队列形成层级，让高优先级工作优先获得调度。

**Why this priority**: 精细化调度控制，属全量表面的收尾；在 Linux 上为 best-effort。

**Independent Test**: 设置 QoS 后高优先级队列任务优先获得执行；target queue 层级下 QoS 正确传播。

**Acceptance Scenarios**:

1. **Given** 两个不同 QoS 的队列竞争线程池，**When** 同时投递任务，**Then** 高 QoS 队列任务优先获得调度（Linux 上 best-effort）。
2. **Given** 队列 A 目标指向队列 B，**When** 设置 B 的 QoS，**Then** A 继承传播的 QoS。

---

### Edge Cases

- 向当前串行队列同步投递 → 死锁检测报错，而非挂起。
- 任务内抛出未捕获异常 → 经全局/per-queue handler 或 group 聚合上报，不崩进程、不静默吞（FR-030）。
- 闭包捕获了外部变量 → 投递时被拒绝并报错。
- 向共享容器存入资源/不可序列化值 → 拒绝并报错。
- 取消一个正在长阻塞 I/O 中的任务 → 协作式取消，无法立即中断，需任务主动检查标志。
- 工作线程致命错误（segfault/不可捕获 fatal）→ 进程终止，由外部监督（systemd/supervisord）重启；不做线程级复活（FR-036）。
- 挂起的队列上投递任务 → 任务排队但不派发，恢复后继续。
- 信号量 wait 超时 / 容器空 pop → 阻塞 API 带超时返回空，非阻塞 tryPop 立即返回空（FR-024/010/011）。
- 主循环阻塞主线程时进程内如何生产任务 → 用回调驱动（事件源/定时器），或改用手动 drain/step 自定义循环（FR-031 双模型）。

## Requirements *(mandatory)*

### Functional Requirements

**队列与执行**

- **FR-001**: 开发者必须能够创建串行队列，投递到该队列的任务必须严格按 FIFO 顺序、一次一个执行。
- **FR-002**: 开发者必须能够创建并发队列并指定并发宽度，任务必须能并行执行，同时在途任务数不超过宽度上限。
- **FR-003**: 系统必须提供全局并发队列，并允许配置其底层线程池大小。
- **FR-004**: 系统必须提供绑定主线程的主队列。
- **FR-005**: 开发者必须能够异步投递任务，投递后立即返回 void、不等待执行；任务结果经共享容器/回调取回。
- **FR-006**: 开发者必须能够同步投递任务，阻塞调用者直到任务完成。
- **FR-007**: 系统必须检测向当前串行队列同步投递导致的死锁，报错而非挂起。
- **FR-008**: 系统必须提供异步/同步投递的函数指针变体（`_f`）。
- **FR-009**: 开发者必须能够在并发队列上投递栅栏任务（barrier async/sync），栅栏任务必须等待所有在途任务完成后独占执行，期间不与其他任务并行。

**同步原语**

- **FR-010**: 开发者必须能够创建任务组（group），enter/leave 维护计数，wait 阻塞至归零（可设超时），notify 在归零时投递通知任务。
- **FR-011**: 开发者必须能够创建计数信号量（semaphore），signal 增、wait 减或阻塞（可设超时），用于限制并发资源访问。
- **FR-012**: 系统必须提供一次性初始化原语（once），保证初始化块在并发下恰好执行一次。
- **FR-013**: 开发者必须能够挂起/恢复队列（suspend/resume），挂起期间队列不派发任务。

**时间与事件**

- **FR-014**: 开发者必须能够延时投递任务（after），支持相对时间与绝对墙钟时间。
- **FR-015**: 开发者必须能够创建事件源（source）监听以下跨平台事件：定时器、fd 可读、fd 可写、信号、进程退出（Linux 经 pidfd/waitpid）、文件系统节点变化（Linux 经 inotify）、数据累加/或/替换。内存压力源为 Darwin 专属，排除（FR-035）。
- **FR-016**: 事件源必须支持事件处理回调、取消回调、注册回调，以及挂起/恢复/取消。
- **FR-017**: 开发者必须能够创建可取消任务块（block 对象），支持 wait/notify/cancel/test_cancel；取消必须为协作式（任务主动检查取消标志）。

**并行与 IO**

- **FR-018**: 开发者必须能够并行迭代（apply），将迭代分发到并发队列，调用线程参与执行并等待全部完成。
- **FR-019**: 系统必须提供不可变字节缓冲（dispatch_data），支持创建、拼接、子范围、映射、遍历。
- **FR-020**: 系统必须提供 fd 级 IO（dispatch_io），支持 stream/random 类型的读写、关闭、barrier、间隔设置。

**QoS 与层级**

- **FR-021**: 开发者必须能够为队列/任务块设置 QoS 优先级档位；在 Linux 上以 best-effort 方式近似。
- **FR-022**: 开发者必须能够设置队列的目标队列（target queue）形成层级，QoS 沿层级传播。
- **FR-023**: 开发者必须能够在队列上设置/读取队列专属上下文（queue-specific），并断言当前执行队列（assert_queue）。

**共享容器**

- **FR-024**: 系统必须提供线程安全并发队列，支持多生产者多消费者 push，以及双取元素 API：阻塞 pop（可设超时，超时返回空）与非阻塞 tryPop（空时立即返回空）。
- **FR-025**: 系统必须提供线程安全并发集合，支持 add/contains，用于去重。
- **FR-026**: 系统必须提供线程安全原子计数器，支持 inc/dec/get。
- **FR-027**: 系统必须提供线程安全并发 map，支持 set/get/delete。
- **FR-028**: 共享容器必须仅接受可序列化值（标量/数组；对象以序列化拷贝进出），拒绝资源与不可序列化值。

**任务模型与运行时**

- **FR-029**: 任务必须为闭包 + 可序列化参数；闭包不得捕获外部变量，系统必须拒绝捕获了外部变量的闭包。
- **FR-030**: 系统必须接住任务内未捕获异常并经三层机制上报：全局默认错误 handler（兜底）、可选 per-queue handler 覆盖、group 异常聚合；不得静默吞掉或因可捕获异常导致进程崩溃。
- **FR-031**: 系统必须提供两种常驻运行模型且二者并存：(a) 阻塞式主循环 `dispatch_main`，由事件源/定时器/异步回调驱动任务生产；(b) 手动单步派发 API（drain/step），供开发者编写自定义主循环（拉外部队列→投递→drain）。两者均须支持外部队列消费与进程内生产。
- **FR-032**: 每个工作线程必须运行独立的 PHP 运行时上下文，任务间不得共享活的 PHP 对象/资源。

**平台**

- **FR-033**: 扩展必须在 Linux 与 macOS 上以一致语义运行。
- **FR-034**: 扩展必须构建于 ZTS PHP CLI（8.2+），不支持 NTS/FPM/Windows。
- **FR-035**: 系统必须排除 Darwin 专属事件源（Mach send/recv、内存压力）；全量 API 表面限定为跨平台子集。进程退出/文件节点源在 Linux 经自定义后端（pidfd/inotify）实现。
- **FR-036**: 工作线程发生致命错误（segfault、不可捕获 fatal）时，系统必须终止进程并交由外部监督重启，不做线程级隔离复活；可捕获异常仍按 FR-030 接住上报。

### Key Entities

- **派发队列（Dispatch Queue）**: 调度上下文，含串行/并发类型、并发宽度、目标队列、QoS、挂起状态、专属上下文；与执行线程解耦。
- **任务/块（Task/Block）**: 工作单元，含闭包与可序列化参数、QoS、可取消标志、所属队列/组。
- **任务组（Group）**: 一批任务的聚合，含原子计数与通知列表。
- **信号量（Semaphore）**: 计数同步原语，含计数值。
- **事件源（Source）**: 事件监听器，含类型、句柄、掩码、处理器、状态。
- **字节缓冲（Dispatch Data）**: 不可变字节序列，支持区域操作。
- **共享容器（Container）**: 线程安全的队列/集合/计数器/map，承载可序列化值，供跨任务协作。
- **线程池（Thread Pool）**: 工作线程集合，每线程一个独立 PHP 运行时上下文。

## Out of Scope

- Mach 专属事件源（MACH_SEND/RECV）与 Darwin 专属 QoS 内核语义。
- FPM / NTS / Windows 支持。
- 共享活的 PHP 对象 / 资源（数据库连接、curl handle 等跨线程共享）。
- CPU 并行性能保证（以 I/O 并发为主）。
- 与现有 PHP 事件库（ext-event / ev / uv）的互操作。
- 应用层 HTTP 客户端（扩展只提供调度 + 容器 + fd 级 IO）。

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 串行队列任务执行顺序与投递顺序 100% 一致；并发队列可观测并行度达到配置宽度。
- **SC-002**: 共享容器在 ≥64 线程高压并发下零数据竞争、零丢失更新（经并发压力测试与竞态检测验证）。
- **SC-003**: 任务内可捕获异常 100% 被接住并上报，不因可捕获异常崩溃；致命错误（segfault 等）按进程级崩溃 + 外部监督重启处理（FR-036），不计入"零崩溃"口径。
- **SC-004**: 常驻爬虫参考场景连续运行 ≥24h，无内存泄漏、无崩溃。
- **SC-005**: I/O 并发吞吐有可复现基准，与至少一个现有并发方案对比并公布数据。
- **SC-006**: 同一套 API 在 Linux 与 macOS 上行为一致，双平台测试套件 100% 通过。
- **SC-007**: 全量跨平台 GCD API 表面 100% 有对应自动化测试覆盖。

## Assumptions

- 目标用户为构建常驻 CLI 并发应用的 PHP 开发者；爬虫是首个验证场景，扩展定位为通用 GCD 运行时。
- 运行环境为专门编译的 ZTS PHP 8.2+ CLI（8.2/8.3/8.4），非 FPM/NTS/Windows。
- 目标平台为 Linux（生产）+ macOS（开发），走可移植 POSIX 路线。
- 生产部署须配置外部进程监督（systemd/supervisord 等），以在致命错误后拉起进程。
- 扩展职责边界：提供调度 + 共享容器 + fd 级 IO；应用层 HTTP 等由开发者自带（如每线程独立 curl handle），其 ZTS 多线程安全性由开发者/依赖方保证。
- 任务闭包不捕获外部变量；状态经参数 / 容器 / 返回值传递。
- 共享容器值限可序列化类型（标量/数组；对象以序列化拷贝进出）。
- async 返回 void（GCD 原味），结果走共享容器；future 风格返回值留作后续增强。
- QoS 在 Linux 为 best-effort（线程优先级近似），保真度有限。
- 全量 API 表面限定为跨平台子集，排除 Mach 专属项。
- 分期 P1–P6 交付，每期独立可验证。
- 主循环双模型、线程致命错误处理、容器阻塞/超时语义、错误上报三层机制、async 返回 void 均已在 Clarifications 敲定；其余次要项（PHP 版本精确下限、API 命名最终形态、性能基准参照）留待 plan。