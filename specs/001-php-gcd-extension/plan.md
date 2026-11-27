# Implementation Plan: PHP GCD 并发扩展

**Branch**: `001-php-gcd-extension` | **Date**: 2026-11-27 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-php-gcd-extension/spec.md`

## Summary

实现一个 PHP C 扩展，在 **ZTS PHP CLI** 上以**真 OS 线程池**提供**全量跨平台 GCD** 并发运行时：队列（串行/并发）与线程池分离、async/sync/barrier/after/apply、group/semaphore/once、事件源（epoll/kqueue 抽象）、dispatch_data/io、QoS（Linux best-effort）、target queue 层级，外加 **C 层线程安全共享容器**（队列/集合/原子计数器/map）。任务为闭包+可序列化 argv（不捕获外部变量）。双主循环模型（dispatch_main 回调驱动 + 手动 drain/step）。致命错误进程级崩溃+外部监督；可捕获异常三层上报。分 P1–P6 期交付，爬虫为首个验证场景。

## Technical Context

**Language/Version**: C（C99/C11）编写扩展；宿主运行时 PHP 8.2+ **ZTS** CLI（8.2/8.3/8.4）

**Primary Dependencies**: Zend/TSRM（PHP 内核 API）、POSIX pthread、自实现 GCD 调度层（借鉴 libdispatch 机制，不链接 Apple libdispatch）；事件后端 epoll（Linux）/ kqueue（macOS）；构建 phpize + config.m4（autoconf）。可选借鉴 swift-corelibs-libdispatch（Apache-2.0）的队列/source 模式

**Storage**: N/A（进程内 C 容器；外部消息队列如 Redis 属用户应用层，非扩展依赖）

**Testing**: PHPT（run-tests.php）覆盖 API 语义；C 层并发压力测试 + TSan/Helgrind/ASan；CI 矩阵 Linux+macOS ZTS

**Target Platform**: Linux（生产，epoll）+ macOS（开发，kqueue）；仅 ZTS CLI；不支持 NTS/FPM/Windows

**Project Type**: PHP 扩展（载入 PHP 的 C 库）

**Performance Goals**: I/O 并发吞吐与 ext-parallel/swoole 协程可比（有可复现基准，不保证胜出）；并发队列可观测并行度达配置宽度；≥64 线程容器压力零竞态

**Constraints**: 必须 ZTS（钉死目标 PHP 版本，API 漂移风险）；自定义 pthread 池（libdispatch 拥有其线程，不可托管 PHP 运行时）；闭包不捕获外部变量（v1）；容器值限可序列化；致命错误→进程崩溃+外部监督；QoS Linux best-effort；排除 Darwin 专属 source（Mach/内存压力）；async 返回 void

**Scale/Scope**: 全量跨平台 GCD 表面（36 条 FR），6 期（P1–P6），数人月工程

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

`.specify/memory/constitution.md` 当前为**未填充的占位模板**（无已批准原则），故无项目特定门禁可校验。采用通用工程门禁：

| 通用门禁 | 状态 | 说明 |
|----------|------|------|
| 可测试性（每条 FR 可独立验证） | PASS | 36 条 FR 均有可观测行为；SC-007 要求 100% 测试覆盖 |
| 库优先/自包含 | PASS | 扩展为自包含 C 库，独立可测 |
| 简单性/YAGNI | PASS（有说明） | 全量 GCD 为用户明确要求的范围，非过度设计；以 P1–P6 分期控制复杂度 |
| 可观测性 | DEFERRED | 日志/指标细节留待实现期；建议后续补充 |

**结论**：PASS（无已批准原则可违反；通用门禁满足）。**行动项**：建议运行 `/speckit-constitution` 正式批准项目原则。

**Phase 1 设计后复检**：PASS——data-model/contracts/quickstart 保持可测试性与自包含；source 可移植性按研究修订（排除 Darwin 专属 MEMORYPRESSURE，PROC/VNODE 自实现 Linux 后端），未引入新门禁违规。

## Project Structure

### Documentation (this feature)

```text
specs/001-php-gcd-extension/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (GCD PHP API 契约)
└── tasks.md             # Phase 2 output (/speckit-tasks - NOT created here)
```

### Source Code (repository root)

```text
ext-gcd/
├── config.m4                 # autoconf 构建（Unix）；ZTS 检测
├── php_gcd.h                 # 公共头、模块入口、zend_module_entry
├── gcd.c                     # MINIT/MSHUTDOWN、函数/类注册
├── src/
│   ├── runtime/              # P1：ZTS 每线程 PHP 上下文 + 线程池
│   │   ├── thread_pool.{c,h}
│   │   ├── php_context.{c,h} # 每线程 Zend context 启动/关闭
│   │   └── task.{c,h}        # 闭包+argv、捕获检测、异常接住
│   ├── queue/                # P1/P2/P3：GCD 队列与同步层
│   │   ├── queue.{c,h}       # serial/concurrent、drain lock、target queue
│   │   ├── dispatch.{c,h}    # async/sync/barrier/after/apply（含 _f）
│   │   ├── group.{c,h}
│   │   ├── semaphore.{c,h}
│   │   ├── once.{c,h}
│   │   └── block.{c,h}       # block 对象、协作式取消
│   ├── source/               # P4：事件源 + 后端抽象
│   │   ├── source.{c,h}
│   │   ├── backend_epoll.c   # Linux
│   │   ├── backend_kqueue.c  # macOS
│   │   └── timer.{c,h}
│   ├── io/                   # P5：dispatch_data / dispatch_io
│   │   ├── data.{c,h}
│   │   └── io.{c,h}
│   ├── container/            # P2：线程安全共享容器
│   │   ├── queue.{c,h}       # MPMC 并发队列（阻塞 pop+超时 / tryPop）
│   │   ├── set.{c,h}
│   │   ├── atomic.{c,h}
│   │   └── map.{c,h}
│   ├── qos.{c,h}             # P6：QoS，Linux best-effort
│   ├── loop.{c,h}            # P1：dispatch_main + 手动 drain/step
│   ├── error.{c,h}           # 三层错误上报（全局/per-queue/group）
│   └── serialize.{c,h}       # argv/容器值序列化边界
├── tests/                    # PHPT
│   ├── queue/  sync/  source/  container/  io/
│   └── stress/               # 并发压力 + sanitizer
├── examples/
│   └── crawler/              # 常驻爬虫 demo（SC-004 验证场景）
├── docker/                   # ZTS PHP 构建镜像（Linux 复现）
└── .github/workflows/        # CI 矩阵 Linux+macOS ZTS
```

**Structure Decision**: 单一 C 扩展项目；`src/` 按 GCD 子系统切分并与 P1–P6 分期对齐；`tests/` 含 PHPT 语义测试与并发压力测试；`examples/crawler/` 承载 SC-004 长跑验证；`docker/` 提供可复现 ZTS 构建。不链接 Apple libdispatch，自实现调度层以便每线程绑定独立 PHP 上下文。

## Complexity Tracking

> 宪法无已批准原则，无门禁违规需正当化。全量 GCD 范围为用户明确要求，复杂度以 P1–P6 分期缓解（每期独立可验证）。本节留空。