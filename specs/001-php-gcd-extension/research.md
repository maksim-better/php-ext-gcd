# Research: PHP GCD 并发扩展

**Phase 0 output** — 解析 Technical Context 未知项。事实经研究代理对照一手来源（php-src、krakjoe/parallel、swift-corelibs-libdispatch、curl 文档）核验；置信度 HIGH（读了源码/文档）/ MEDIUM（强推断）/ LOW（不确定）。

> **版本警示**：php-src `master` 与 parallel `develop` 在 2025–2026 重构过；稳定分支（8.1–8.4）API 与 master 不同。**须钉死目标 PHP 版本**，勿假设 master 符号存在于 8.x。

## R1 — ZTS 每线程 PHP 运行时启动

- **Decision**: 自定义 pthread 线程池；每工作线程 `ts_resource(0)` → `TSRMLS_CACHE_UPDATE()` 建 TSRM 上下文 → `php_request_startup()`（**一次/线程**，非每任务）→ `zend_execute_ex` 跑拷贝闭包。退出：`gc_collect_cycles()` → `php_request_shutdown(NULL)` → `ts_free_thread()` → `pthread_exit()`。主线程镜像 embed SAPI：`php_tsrm_startup` → `zend_signal_startup` → `sapi_startup` → `php_module_startup`（MINIT）。
- **Rationale**: parallel `src/scheduler.c` 验证此生命周期。MINIT 仅主线程一次；worker 仅 RINIT（`zend_activate_modules`），故进程级 init（如 curl_global_init）不在 worker 重复。
- **Correction（HIGH）**: **不存在** `tsrm_new_thread_context()` / `zend_new_thread()`。真实机制是 `ts_resource_ex(id, th_id)` / 宏 `ts_resource(id)` 惰性分配 + `TSRMLS_CACHE_UPDATE()`。
- **Alternatives considered**: 每任务 request startup/shutdown（更重：重置请求内存管理器/interned strings/output/signal）——v1 不采用，留作隔离增强。
- **Version risk**: `TSRMLS_CACHE` 在 PHP-8.3 是 `void*`（`_tsrm_ls_cache`），master 重构为 struct（`ZEND_TSRMLS_CACHE_T`）。须按目标版本 spike。
- **Spikes**: S1 目标版本最小 ZTS 线程 boot+跑闭包；S5 per-thread vs per-task 成本/泄漏。
- **Sources**: php-src TSRM/TSRM.h（master & PHP-8.3）、parallel src/scheduler.c·runtime.c、sapi/embed/php_embed.c、main/main.c。

## R2 — 任务模型与闭包捕获

- **Decision**: v1 **拒绝捕获外部变量的闭包**（抛 `GCD\CaptureException`），状态经 argv（可序列化）+ 共享容器传递。
- **Rationale**: 最简、最低风险；跨线程值边界是最难代码（R3）。
- **Alternatives considered**: 现代 parallel `develop` 实际**按值拷贝** `use` 变量（引用被切断）、隔离 statics、允许自引用闭包（PR #339/#366，2025–2026 改；旧 1.x 才拒绝）。更人体工学但需深拷贝器+COW+循环回收，风险高 → 留作后续增强。
- **Note**: spec FR-029"拒绝捕获"是 v1 简化决策，**非 parallel 当前行为**；研究证实拷贝可行但昂贵。
- **Confidence**: HIGH（parallel src/copy.c·copy.h、commit e8697fb、PR #339/#366）。
- **Spikes**: S3 拒绝谓词（哪些 zval 类型非法）；若做拷贝须复现共享不可变数组 COW/refcount=2 技巧。

## R3 — 跨线程值边界 / 共享容器实现

- **Decision**: 共享容器存可序列化值；边界用**深拷贝**（persistent `pemalloc` ↔ thread `emalloc`），参考 parallel `PARALLEL_ZVAL_COPY`；拒绝资源/不可序列化值。容器本体：MPMC 队列用 mutex+condvar，计数器用原子，set/map 用锁。
- **Rationale**: parallel 证明"named link + monitor + 每次 send/recv 深拷贝"是跨线程共享原语（**非共享 zval 内存**）。
- **Alternatives considered**: lock-free 容器（更高吞吐但更难正确）→ v1 用锁，基准后优化；PHP `serialize()` 作边界（简单但慢）vs 自定义深拷贝（parallel 路线，快但难）。
- **Risk（最难代码）**: parallel 2025–2026 多次修 crash/leak；共享不可变数组需 COW/refcount=2；自引用需 `gc_collect_cycles`。
- **Confidence**: HIGH。
- **Spikes**: S3 拷贝谓词、COW 正确性、循环泄漏。
- **Sources**: parallel src/copy.c·copy.h·channel.c·future.c·link.c。

## R4 — curl / 并发 HTTP（爬虫硬风险）

- **Decision**: 扩展**调度-only**；HTTP 由开发者自带，**每工作线程独立 curl handle**（`curl_init`），不跨线程并发使用同一 handle。依赖 PHP curl MINIT 已做的单次 `curl_global_init`（主线程），worker 不重复 global init。
- **Rationale**: libcurl 线程安全但无内部同步（handle 不可并发使用）；PHP curl MINIT 调 `curl_global_init(CURL_GLOBAL_DEFAULT)` 一次；**ZTS PHP 自动设 `CURLOPT_NOSIGNAL=1`**（ext/curl/interface.c，commit d81f2e5）。**模型可行——硬风险可控**。
- **Constraints**: 需 threaded resolver 或 c-ares（NOSIGNAL 下同步 resolver DNS 超时失效）；进程级处理 SIGPIPE；验证 `CURL_VERSION_THREADSAFE`（≥7.84.0）；避免跨线程 persistent share handle；DNS/SSL 共享需 share handle+锁，**连接池/HSTS 不可共享**。
- **Confidence**: HIGH（curl threadsafe.html、php-src ext/curl/interface.c）。
- **Spikes**: S2 N 线程 curl_init+curl_exec 压测 crash/leak/SIGPIPE；目标 libcurl features/resolver；ext/curl/share.c 锁。
- **Sources**: curl.se/libcurl/c/threadsafe.html·curl_global_init.html、CURLOPT_NOSIGNAL.md、php-src ext/curl/interface.c。

## R5 — libdispatch 可移植性与调度层选型

- **Decision**: **自定义 pthread 线程池 + 自实现 GCD 队列层**（借鉴 libdispatch 机制），**不链接 libdispatch 做 worker 调度**。
- **Rationale（关键架构）**: libdispatch 拥有并动态管理其 workqueue 线程（userspace pthread 池，PR #206），无法在其池线程上可靠 boot 每线程 PHP 运行时；parallel 需稳定 thread↔runtime 绑定。**链接 libdispatch 跑 PHP 闭包是最大架构陷阱**。
- **Source 可移植性（HIGH，source.h `DISPATCH_LINUX_UNAVAILABLE`）**:
  - 便携：DATA_ADD/OR/REPLACE、READ、WRITE、SIGNAL、TIMER。
  - libdispatch 标 Linux-unavailable：MACH_SEND/RECV、MEMORYPRESSURE、PROC、VNODE。
  - **但 PROC/VNODE 我们可自实现 Linux 后端**（pidfd/waitpid、inotify），不受 libdispatch 选择限制 → 保留为跨平台。
  - **MEMORYPRESSURE 无干净 Linux 等价 → 归 Darwin-only，排除**（与 FR-035 一致；已据此修订 spec/contract/data-model）。
- **Linux backend**: `epoll_create1` + `eventfd` 唤醒 + 专用 manager 线程；`signalfd`（信号需进程级 block）；`timerfd`（定时器）；muxnote 按 fd 去重（epoll 仅按 fd uniquing）；**fork after init 不安全**。
- **QoS Linux**: best-effort，userspace 分桶，无内核强制（README 明示）。
- **License**: Apache-2.0；借鉴代码须保留 notices/NOTICE、声明改动；PHP 扩展惯例 PHP License 3.01，**确认项目许可意图**。
- **Confidence**: HIGH（dispatch/source.h、src/event/event_epoll.c·workqueue.c、PR #206、README、LICENSE）。
- **Spikes**: S6 Linux timerfd 细节、QoS setpriority/nice 行为；若链接 libdispatch 须原型验证 thread 绑定（likely 不行）。

## R6 — 事件后端抽象

- **Decision**: 后端抽象层 `backend_epoll.c`(Linux) / `backend_kqueue.c`(macOS)；source 按平台映射：signal→signalfd/kqueue EVFILT_SIGNAL、proc→pidfd·waitpid/kqueue EVFILT_PROC、vnode→inotify/kqueue EVFILT_VNODE、timer→timerfd/kqueue EVFILT_TIMER、read/write→epoll/kqueue。
- **Rationale**: 统一 source API，平台差异隔离在后端。
- **Alternatives considered**: 链接系统 libdispatch 做事件源（专用线程）——可选但增依赖与 thread 所有权复杂度；v1 自实现。
- **Confidence**: HIGH（event_epoll.c）+ 设计判断。

## R7 — 主循环双模型

- **Decision**: `dispatch_main()`（`GCD\main`）阻塞主线程 drain 主队列+事件后端（回调驱动）；`drain(timeout)`/`run_once()` 非阻塞单步推进，供自定义 while 循环。
- **Rationale**: Clarification Q1=C；爬虫外部队列消费用手动循环自然，事件驱动应用用 dispatch_main。
- **Confidence**: 设计决策（Clarification）。

## R8 — 错误上报与信号所有权

- **Decision**: 三层错误上报（全局/per-queue/group）接住可捕获异常；致命错误进程终止+外部监督（FR-036）。**明确信号所有权策略**：扩展安装 SIGSEGV/SIGCHLD/SIGPIPE 处理须与宿主 SAPI/PHP runtime/libcurl 协调（参考 parallel `php_parallel_sigsegv_handler` + embed `signal(SIGPIPE, SIG_IGN)`）。
- **Rationale**: 信号进程级，多 runtime/SAPI 冲突是真实风险（research risk #4）。
- **Confidence**: HIGH（parallel 信号处理）+ 设计。
- **Spikes**: S4 信号所有权与宿主 SAPI 共存。

## R9 — 构建与 CI

- **Decision**: phpize + config.m4（autoconf），**ZTS 检测（拒绝 NTS 构建）**；docker/ ZTS PHP 镜像（`--enable-zts`）复现；CI 矩阵 Linux+macOS ZTS；sanitizer 构建（TSan/ASan）。
- **Rationale**: 标准 PHP 扩展构建；ZTS 是硬约束须构建期校验。
- **Confidence**: HIGH（PHP 扩展惯例）。

## R10 — 序列化边界

- **Decision**: argv/容器值经深拷贝（R3）或 PHP serialize；v1 容器值限可序列化类型，对象序列化拷贝。
- **Rationale**: 跨线程不能共享活 zval。
- **Alternatives considered**: parallel 自定义深拷贝（快但难）vs serialize（简单但慢）。
- **Confidence**: HIGH。

## Top Risks（排序）

1. **PHP 版本 API 漂移**（HIGH）：钉版本 + S1 spike。
2. **thread↔runtime 所有权 vs libdispatch**（HIGH）：自定义池必需。
3. **跨线程值边界正确性**（HIGH）：最难代码，S3。
4. **信号处理所有权**（MED-HIGH）：S4。
5. **扩展 ZTS 安全性**（MED-HIGH）：仅 TSRM 全局是每线程；curl 自身安全但验 resolver/THREADSAFE。
6. **per-task vs per-thread request 生命周期**（MED）：S5。
7. **许可/notices**（LOW-MED）：Apache-2.0 保留 notices。

## Spike 清单（实现前/早期必做）

- **S1**: 目标 PHP 版本最小 ZTS 线程 boot（ts_resource + TSRMLS_CACHE_UPDATE + php_request_startup + 跑闭包）。
- **S2**: N 线程 curl 压测（crash/leak/SIGPIPE/resolver/THREADSAFE）。
- **S3**: 跨线程深拷贝边界正确性（COW/循环/拒绝谓词）。
- **S4**: 信号所有权与宿主 SAPI 共存。
- **S5**: per-thread vs per-task request 成本/泄漏。
- **S6**: Linux timerfd/QoS 细节。

> 结论：每线程 boot 模式与 curl 模型已被 parallel + PHP curl 验证可行；新颖风险面是 (a) 共享容器/值拷贝层 与 (b) 自定义池而非复用 libdispatch 调度。计划 grounded 于 parallel scheduler.c 生命周期与 copy.c 边界，而非猜测的 API 名。