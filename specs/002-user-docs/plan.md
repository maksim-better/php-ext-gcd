# ext-gcd 双语文档实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 ext-gcd 建成中英双语用户文档（9 章指南 + 10 页 API 参考 + 索引，共 41 个 Markdown 文件），签名与语义以 C 源码为准，全部示例在本机 PHP 8.6 ZTS 上可运行。

**Architecture:** `docs/en/` 与 `docs/zh/` 同构镜像，按模块分页。每个任务产出一个模块的 en+zh 页对（API 参考 + 对应指南章节），以「源码事实提取 → 写页 → 机械验证」为循环。示例验证脚本把「示例可运行」变成可重复门禁。

**Tech Stack:** Markdown（GitHub 渲染）；PHP 8.6 ZTS（/opt/homebrew/opt/php-zts/bin/php，gcd 已经 conf.d 启用）；一个 PHP 示例验证脚本；git。

**Spec:** `specs/002-user-docs/design.md`

## Global Constraints

- 版本承诺：ZTS PHP 8.4–8.6；8.6 本机实测 29/29；8.4 为 Docker 构建目标；8.5 理论兼容未验证；8.2/8.3 不承诺。
- 签名唯一来源：C arginfo + 函数体（arginfo 无类型信息，类型/默认值/返回类型必须读函数体）。
- 行为语义来源：C 源码 + tests/ + examples/crawler.php；specs/001 契约仅作历史参考（含约 15 个未实现功能，不可引用）。
- v1 任务闭包禁止捕获变量：示例与正文不得出现 `use (...)`；参数经可序列化 argv 传递。
- 只文档化已实现 API：43 函数、5 QoS 常量、6 SOURCE_* 常量、9 类、3 异常（均继承 Error）。
- 纯 Markdown、相对链接、GitHub 可渲染；en/zh 标题层级序列一致；页首语言互链。
- 示例验证解释器：`/opt/homebrew/opt/php-zts/bin/php`。
- 提交信息不加 Co-Authored-By 署名行；摘要用英文仓库惯例 `docs(scope): ...`。
- 容器类均有静态 `open()`（进程内命名注册）；`Data` 仅 `size()`；Queue/Group/Semaphore/Source 为无方法句柄类。

## Review Focus

1. **签名/默认值/返回类型与 C 函数体不符**（arginfo 无类型，最易漂移）→ Task 2–9 的提取步骤强制读函数体，评审对照源码抽查。
2. **示例出现捕获变量 `use (...)`**（v1 不支持，照抄即错）→ Task 2–9 及 Task 10 的 grep 门禁。
3. **sync/barrier 死锁与阻塞语义写错**（用户照做即死锁）→ Task 3 从 tests/error/ + tests/queue/ 提取 DeadlockException 触发条件并示例验证。
4. **命名容器 `open()` 共享/生命周期语义写错**（跨队列共享是核心用法）→ Task 7 对照 crawler.php + tests/container/，示例验证同名共享。
5. **中英结构漂移**（zh 缺节/多节）→ 每任务 parity diff + Task 10 全量扫描。

## File Structure

```
scripts/verify-doc-examples.php      # 示例验证脚本（Task 1）
tests/docs/{ok,bad,mixed}.md         # 脚本夹具（Task 1）
docs/README.md                       # 总入口：选语言（Task 1）
docs/{en,zh}/README.md               # 语言索引 + 目录（Task 1）
docs/{en,zh}/api/README.md           # API 总索引（Task 1 建全量 TOC）
docs/{en,zh}/guide/01-quickstart.md  # Task 1
docs/{en,zh}/guide/02-concepts.md    # Task 2（队列/线程模型/任务规则）
docs/{en,zh}/api/queue.md            # Task 2（12 函数 + 5 QoS 常量 + Queue 句柄）
docs/{en,zh}/guide/03-dispatch.md    # Task 3
docs/{en,zh}/api/dispatch.md         # Task 3（async/sync/barrier_async/barrier_sync/apply/once）
docs/{en,zh}/guide/04-sync.md        # Task 4
docs/{en,zh}/api/group.md            # Task 4（6 函数 + Group 句柄）
docs/{en,zh}/api/semaphore.md        # Task 4（4 函数 + Semaphore 句柄）
docs/{en,zh}/guide/05-sources.md     # Task 5
docs/{en,zh}/api/source.md           # Task 5（7 函数 + 6 SOURCE_* 常量 + Source 句柄）
docs/{en,zh}/guide/06-data.md        # Task 6
docs/{en,zh}/api/data.md             # Task 6（4 函数 + Data 类）
docs/{en,zh}/guide/07-containers.md  # Task 7
docs/{en,zh}/api/containers.md       # Task 7（Atomic/Map/Set/ConcurrentQueue 逐方法）
docs/{en,zh}/guide/08-errors.md      # Task 8
docs/{en,zh}/api/errors.md           # Task 8（2 处理器函数 + 3 异常）
docs/{en,zh}/guide/09-qos-mainloop.md# Task 9
docs/{en,zh}/api/loop.md             # Task 9（main/drain）
README.md                            # Task 10 刷新（版本 + docs 链接）
specs/002-user-docs/notes.md         # Task 10（契约 vs 实现差异记录）
```

每任务产出的页对共享同一组已验证示例；指南链接到对应 API 页锚点。

---

### Task 1: 骨架 + 索引 + 示例验证脚本 + 快速开始

**Files:**
- Create: `scripts/verify-doc-examples.php`, `tests/docs/ok.md`, `tests/docs/bad.md`, `tests/docs/mixed.md`
- Create: `docs/README.md`, `docs/{en,zh}/README.md`, `docs/{en,zh}/api/README.md`, `docs/{en,zh}/guide/01-quickstart.md`

**Interfaces:**
- Produces（后续所有任务消费）:
  - 验证命令：`/opt/homebrew/opt/php-zts/bin/php scripts/verify-doc-examples.php docs/en/<path> docs/zh/<path>`，退出 0 = 全部 ```php 块可运行。
  - parity 命令：`diff <(grep -oE '^#+ ' docs/en/<path>) <(grep -oE '^#+ ' docs/zh/<path>)`，无输出 = 标题层级序列一致。
  - 页首互链：`[English](../../en/<同路径>) | [中文](../../zh/<同路径>)`（相对当前文件）。
  - 参考页函数节固定六段：Signature / Parameters / Return / Errors / Notes / Example（zh：签名/参数/返回值/错误与异常/备注/示例）。
  - 围栏约定：可运行示例用 ```php；签名与非运行片段用 ```text（脚本只跑 php 块）。

- [ ] **Step 1: 写脚本夹具**

`tests/docs/ok.md`：一个 ```php 块 `echo "ok";`。
`tests/docs/bad.md`：一个 ```php 块 `gcd_no_such_function();`。
`tests/docs/mixed.md`：一个 ```text 块（含 `<?php syntax error here`）+ 一个 ```php 块 `echo 1;`。

- [ ] **Step 2: 运行验证脚本确认失败**

Run: `/opt/homebrew/opt/php-zts/bin/php scripts/verify-doc-examples.php tests/docs/ok.md`
Expected: FAIL（脚本文件不存在）。

- [ ] **Step 3: 实现 `scripts/verify-doc-examples.php`**

行为：参数为文件或目录（目录递归 `*.md`）；逐文件提取 ```php 围栏块，每块写入临时文件，用 `PHP_BINARY` 执行；退出码非 0 或输出含 `Fatal error`/`Parse error` 判失败，报告 `文件:块序号`；任一失败脚本退出 1，否则退出 0；非 php 围栏忽略。无参数时打印用法并退出 2。

- [ ] **Step 4: 运行验证脚本确认通过**

Run: `/opt/homebrew/opt/php-zts/bin/php scripts/verify-doc-examples.php tests/docs/ok.md tests/docs/mixed.md` → 退出 0；
`... tests/docs/bad.md` → 退出 1 且报告 bad.md:1。

- [ ] **Step 5: 写索引页（5 个）**

`docs/README.md`：项目一句话 + 选语言链接。`docs/{en,zh}/README.md`：指南 9 章 + API 10 页全量 TOC（按 File Structure 清单，链接先就位）。`docs/{en,zh}/api/README.md`：43 函数按组列表 + 字母序索引、类、常量、异常入口。

- [ ] **Step 6: 写快速开始页对**

安装路线取自 `docker/build.sh`、`scripts/build-zts-macos.sh` 与本会话已验证的 phpize 路线（`phpize && ./configure --enable-gcd && make && make install` + conf.d 启用）；版本承诺按 Global Constraints 原文标注验证状态。第一个示例：`GCD\queue_create` + `GCD\async` + `GCD\drain`，argv 传参（无捕获）。

- [ ] **Step 7: 验证**

Run: 验证脚本跑 `docs/en/guide/01-quickstart.md docs/zh/guide/01-quickstart.md` → 退出 0；parity 命令对该页对 → 无输出；`grep -nE '\buse\s*\(' docs/en/guide/01-quickstart.md docs/zh/guide/01-quickstart.md` → 无匹配。

- [ ] **Step 8: Commit**

```bash
git add scripts/verify-doc-examples.php tests/docs docs/README.md docs/en docs/zh
git commit -m "docs: skeleton, indexes, example verifier, quickstart (en+zh)"
```

---

### Task 2: 队列模块（api/queue + guide/02-concepts）

**Files:**
- Create: `docs/{en,zh}/api/queue.md`, `docs/{en,zh}/guide/02-concepts.md`
- Read: `gcd.c:237-280`（函数表）, `src/queue/queue.c`（函数体、常量 212-216、句柄类 195）, `tests/queue/`, `tests/foundational/`

**Interfaces:**
- Consumes: Task 1 的验证命令、六段格式、围栏与互链约定。
- Produces: `api/queue.md` 锚点（每函数一节，GitHub 自动 slug），guide/03/09 将链接；Queue 句柄类说明（由 queue_create/global_queue/current_queue/main_queue 创建，不可实例化）。

- [ ] **Step 1: 提取源码事实**

对 12 个函数（queue_create/global_queue/current_queue/main_queue/queue_suspend/queue_resume/queue_set_specific/queue_get_specific/queue_set_target/queue_set_qos/queue_get_qos/qos_class_self）：`grep -n "PHP_FUNCTION(gcd_<name>)" gcd.c src/queue/queue.c` 定位函数体，读 arginfo + 函数体记录精确签名（类型/默认值/返回）、阻塞行为、异常。5 个 QoS 常量值从 queue.c:212-216 抄录。Linux QoS best-effort 语义从源码注释/tests/qos 提取。

- [ ] **Step 2: 写 en/api/queue.md**

12 函数各一节（六段格式）+ QoS 常量表 + Queue 句柄类节。示例全部 argv 模式。

- [ ] **Step 3: 写 zh/api/queue.md 镜像**

同标题层级序列，中文行文。

- [ ] **Step 4: 写 guide/02-concepts 页对**

串行/并发队列、线程模型（每 worker 独立 PHP 运行时、一请求一线程）、任务规则（闭包 + 可序列化 argv、**禁止捕获**并解释原因）、句柄对象概念。示例复用 Step 2 已验证片段。

- [ ] **Step 5: 验证**

Run: 验证脚本跑 4 个新文件 → 退出 0；两个页对各跑 parity → 无输出；
`grep -rnE '\buse\s*\(' docs/en/api/queue.md docs/zh/api/queue.md docs/en/guide/02-concepts.md docs/zh/guide/02-concepts.md` → 无匹配。

- [ ] **Step 6: Commit**

```bash
git add docs/en/api/queue.md docs/zh/api/queue.md docs/en/guide/02-concepts.md docs/zh/guide/02-concepts.md
git commit -m "docs(queue): API reference + concepts guide (en+zh)"
```

---

### Task 3: 派发模块（api/dispatch + guide/03-dispatch）

**Files:**
- Create: `docs/{en,zh}/api/dispatch.md`, `docs/{en,zh}/guide/03-dispatch.md`
- Read: `gcd.c`（gcd_async/sync/barrier_async/barrier_sync/apply 函数体）, `src/queue/dispatch.c`, `src/queue/once.c`, `tests/queue/{serial_order,concurrent_width,loop_models}.phpt`, `tests/error/`

**Interfaces:**
- Consumes: Task 2 的 Queue 句柄说明与锚点。
- Produces: `api/dispatch.md` 锚点；**死锁语义权威描述**（sync/barrier 在何条件下抛 DeadlockException，从 tests/error 提取），guide/08 将引用。

- [ ] **Step 1: 提取源码事实**

6 函数（async/sync/barrier_async/barrier_sync/apply/once）签名读函数体；从 tests/error/ 与 dispatch.c 提取：sync 在串行队列自身上下文调用的行为、barrier 的排他语义、once 的恰好一次保证、apply 的迭代次数语义。

- [ ] **Step 2: 写 en/api/dispatch.md**

6 函数六段格式；Errors 段必须写明 DeadlockException 触发条件（Review Focus 3）。

- [ ] **Step 3: 写 zh 镜像**

- [ ] **Step 4: 写 guide/03-dispatch 页对**

async/sync 选择、barrier 读写锁模式、apply 并行迭代、once 初始化；等待模式（drain/group_wait 链接 Task 4/9 页）。含一个**验证过的死锁触发示例**（捕获 DeadlockException，预期输出断言）。

- [ ] **Step 5: 验证**

Run: 验证脚本跑 4 文件 → 退出 0（死锁示例须真正抛出并捕获，脚本判通过）；parity ×2；use( grep → 无匹配。

- [ ] **Step 6: Commit**

```bash
git add docs/en/api/dispatch.md docs/zh/api/dispatch.md docs/en/guide/03-dispatch.md docs/zh/guide/03-dispatch.md
git commit -m "docs(dispatch): API reference + dispatch guide, deadlock semantics (en+zh)"
```

---

### Task 4: 同步原语（api/group + api/semaphore + guide/04-sync）

**Files:**
- Create: `docs/{en,zh}/api/group.md`, `docs/{en,zh}/api/semaphore.md`, `docs/{en,zh}/guide/04-sync.md`
- Read: `src/queue/group.c`, `src/queue/semaphore.c`, `tests/sync/`, `examples/crawler.php`（group_wait 惯用法）

**Interfaces:**
- Consumes: Task 3 的 dispatch 锚点（group_async 与 async 的关系）。
- Produces: group/semaphore 锚点；guide/03 等待模式链接目标。

- [ ] **Step 1: 提取源码事实**

group 6 函数（create/async/enter/leave/wait/notify）+ semaphore 4 函数（create/open/wait/signal）签名读函数体；enter/leave 配对与 wait 超时语义、notify 回调线程上下文、semaphore 计数语义从 tests/sync/ 提取。

- [ ] **Step 2: 写 en/api/group.md 与 en/api/semaphore.md**

各函数六段格式 + Group/Semaphore 句柄类节。

- [ ] **Step 3: 写 zh 镜像（2 文件）**

- [ ] **Step 4: 写 guide/04-sync 页对**

group 扇出-汇合（复用 crawler.php 模式）、enter/leave 手动配对、notify、semaphore 限流；示例含并发上限断言（参照 tests/sync/semaphore.phpt）。

- [ ] **Step 5: 验证**

Run: 验证脚本跑 6 文件 → 退出 0；parity ×3；use( grep → 无匹配。

- [ ] **Step 6: Commit**

```bash
git add docs/en/api/group.md docs/zh/api/group.md docs/en/api/semaphore.md docs/zh/api/semaphore.md docs/en/guide/04-sync.md docs/zh/guide/04-sync.md
git commit -m "docs(sync): group + semaphore reference and guide (en+zh)"
```

---

### Task 5: 事件源（api/source + guide/05-sources）

**Files:**
- Create: `docs/{en,zh}/api/source.md`, `docs/{en,zh}/guide/05-sources.md`
- Read: `src/source/source.c`（7 函数、6 SOURCE_* 常量、句柄类 355）, `tests/source/`

**Interfaces:**
- Consumes: Task 2 Queue 锚点、Task 3 dispatch 锚点。
- Produces: source 锚点 + 6 个 SOURCE_* 常量名与值；guide/06 的 fd 读写节将链接 SOURCE_READ/SOURCE_WRITE。

- [ ] **Step 1: 提取源码事实**

source_create/set_event_handler/set_timer/resume/cancel/merge_data/get_data 签名读函数体；6 个 SOURCE_* 常量名与值从 source.c 抄录；各类型触发条件、timer 参数（间隔/leeway）、cancel 后行为从 tests/source/ 提取。

- [ ] **Step 2: 写 en/api/source.md**

7 函数六段 + 常量表 + Source 句柄节。

- [ ] **Step 3: 写 zh 镜像**

- [ ] **Step 4: 写 guide/05-sources 页对**

timer 周期任务、signal 处理、proc 子进程退出、fd 读写源概览（详例留 guide/06）；示例参照 tests/source/{timer,signal}.phpt 的断言模式。

- [ ] **Step 5: 验证**

Run: 验证脚本跑 4 文件 → 退出 0；parity ×2；use( grep → 无匹配。

- [ ] **Step 6: Commit**

```bash
git add docs/en/api/source.md docs/zh/api/source.md docs/en/guide/05-sources.md docs/zh/guide/05-sources.md
git commit -m "docs(source): event source reference and guide (en+zh)"
```

---

### Task 6: 数据（api/data + guide/06-data）

**Files:**
- Create: `docs/{en,zh}/api/data.md`, `docs/{en,zh}/guide/06-data.md`
- Read: `src/io/data.c`（4 函数 + Data::size + 类表 150）, `tests/io/`

**Interfaces:**
- Consumes: Task 5 的 SOURCE_READ/SOURCE_WRITE 锚点。
- Produces: data 锚点；Data 类说明（仅 size()，不可变缓冲）。

- [ ] **Step 1: 提取源码事实**

data_create/data_concat/data_create_map/data_subrange 签名读函数体；data 不可变性、map 回调签名、subrange 边界行为从 tests/io/ 提取。

- [ ] **Step 2: 写 en/api/data.md**

4 函数六段 + Data 类节（size()）。

- [ ] **Step 3: 写 zh 镜像**

- [ ] **Step 4: 写 guide/06-data 页对**

data 缓冲拼接/映射/切片；配合 READ/WRITE 源的 fd 读写模式（v1 无 dispatch_io，正文明确说明 fd I/O 走事件源）。

- [ ] **Step 5: 验证**

Run: 验证脚本跑 4 文件 → 退出 0；parity ×2；use( grep → 无匹配。

- [ ] **Step 6: Commit**

```bash
git add docs/en/api/data.md docs/zh/api/data.md docs/en/guide/06-data.md docs/zh/guide/06-data.md
git commit -m "docs(data): dispatch_data reference and fd I/O guide (en+zh)"
```

---

### Task 7: 共享容器（api/containers + guide/07-containers）

**Files:**
- Create: `docs/{en,zh}/api/containers.md`, `docs/{en,zh}/guide/07-containers.md`
- Read: `src/container/{atomic,map,set,cqueue}.c`, `tests/container/`, `examples/crawler.php`（open() 命名注册）

**Interfaces:**
- Consumes: Task 3 dispatch 锚点、Task 4 group 锚点。
- Produces: containers 锚点；**命名注册语义权威描述**（静态 open() 同名返回同一进程内实例、生命周期、与 __construct 的区别），guide/01 已暗示、此页定论。

- [ ] **Step 1: 提取源码事实**

4 类逐方法读函数体：Atomic（__construct/open/inc/dec/get/set）、Map（__construct/open/set/get/has/delete/count）、Set（__construct/open/add/contains/remove/count）、ConcurrentQueue（__construct/open/push/pop/tryPop/count）。open() 注册表实现（g_registry + 互斥锁）、pop 阻塞/tryPop 非阻塞、非法值 SerializationException 从 tests/container/ 提取（Review Focus 4）。

- [ ] **Step 2: 写 en/api/containers.md**

每类一节：创建（__construct vs open）→ 方法一览 → 逐方法六段。

- [ ] **Step 3: 写 zh 镜像**

- [ ] **Step 4: 写 guide/07-containers 页对**

跨队列共享（同名 open 在不同任务中拿到同一实例——示例须断言）、Atomic 计数无丢失更新（参照 tests/stress/atomic_stress.phpt 缩小规模）、Set 去重、ConcurrentQueue 生产者-消费者。

- [ ] **Step 5: 验证**

Run: 验证脚本跑 4 文件 → 退出 0；parity ×2；use( grep → 无匹配。

- [ ] **Step 6: Commit**

```bash
git add docs/en/api/containers.md docs/zh/api/containers.md docs/en/guide/07-containers.md docs/zh/guide/07-containers.md
git commit -m "docs(containers): shared container reference and guide (en+zh)"
```

---

### Task 8: 错误处理（api/errors + guide/08-errors）

**Files:**
- Create: `docs/{en,zh}/api/errors.md`, `docs/{en,zh}/guide/08-errors.md`
- Read: `gcd.c`（gcd_set_error_handler/gcd_queue_set_error_handler）, `src/queue/queue.c:203-211`（异常注册，父类 zend_ce_error）, `src/runtime/error.c`, `tests/error/`

**Interfaces:**
- Consumes: Task 3 的死锁语义描述。
- Produces: errors 锚点；三层错误上报模型权威描述（处理器回调 → 异常 → 默认行为，以源码为准）。

- [ ] **Step 1: 提取源码事实**

2 个处理器函数签名读函数体；3 异常（CaptureException/SerializationException/DeadlockException）继承 Error、各自触发场景从 error.c + tests/error/ 提取；处理器回调签名、返回值语义、队列级 vs 全局级覆盖关系从源码提取。

- [ ] **Step 2: 写 en/api/errors.md**

2 函数六段 + 3 异常节（继承关系、触发条件、消息格式）。

- [ ] **Step 3: 写 zh 镜像**

- [ ] **Step 4: 写 guide/08-errors 页对**

三层模型、自定义处理器、异常捕获模式；示例验证处理器确实被调用（输出断言）。

- [ ] **Step 5: 验证**

Run: 验证脚本跑 4 文件 → 退出 0；parity ×2；use( grep → 无匹配。

- [ ] **Step 6: Commit**

```bash
git add docs/en/api/errors.md docs/zh/api/errors.md docs/en/guide/08-errors.md docs/zh/guide/08-errors.md
git commit -m "docs(errors): error handling reference and guide (en+zh)"
```

---

### Task 9: 主循环与 QoS（api/loop + guide/09-qos-mainloop）

**Files:**
- Create: `docs/{en,zh}/api/loop.md`, `docs/{en,zh}/guide/09-qos-mainloop.md`
- Read: `src/loop.c`（main/drain）, `src/queue/queue.c`（QoS 函数）, `tests/qos/`, `tests/queue/loop_models.phpt`

**Interfaces:**
- Consumes: Task 2 QoS 常量表锚点、Task 5 source 锚点（main 循环驱动事件源）。
- Produces: loop 锚点；双模型选择指南（main 回调驱动 vs drain 手动）。

- [ ] **Step 1: 提取源码事实**

main/drain 签名读函数体；drain 的退出条件（空转即返 vs 阻塞）、main 的回调驱动与事件源集成、QoS 设置时机与 Linux best-effort 从 tests/qos/ + loop_models.phpt 提取。

- [ ] **Step 2: 写 en/api/loop.md**

main/drain 六段。

- [ ] **Step 3: 写 zh 镜像**

- [ ] **Step 4: 写 guide/09-qos-mainloop 页对**

双模型选择（脚本批处理用 drain、长驻服务用 main）、QoS 五档用法与平台差异；示例验证 drain 等待 pending 任务（参照 loop_models.phpt）。

- [ ] **Step 5: 验证**

Run: 验证脚本跑 4 文件 → 退出 0；parity ×2；use( grep → 无匹配。

- [ ] **Step 6: Commit**

```bash
git add docs/en/api/loop.md docs/zh/api/loop.md docs/en/guide/09-qos-mainloop.md docs/zh/guide/09-qos-mainloop.md
git commit -m "docs(loop): main loop + QoS reference and guide (en+zh)"
```

---

### Task 10: README 刷新 + 差异记录 + 全量验证

**Files:**
- Modify: `README.md`（Requirements 节版本描述 + 新增 Documentation 节）
- Create: `specs/002-user-docs/notes.md`

**Interfaces:**
- Consumes: 全部前序任务的页对与验证命令。
- Produces: 完整可发布的 docs/ 树。

- [ ] **Step 1: 刷新 README**

Requirements 节：ZTS PHP 8.4–8.6 + 验证状态标注（Global Constraints 原文）；新增 Documentation 节链接 docs/README.md；保留 Docker 构建路线。

- [ ] **Step 2: 写 notes.md**

汇总 Task 2–9 提取事实时发现的契约 vs 实现差异（契约缺席功能清单、容器语义差异等），每条注明来源文件。

- [ ] **Step 3: 全量示例验证**

Run: `/opt/homebrew/opt/php-zts/bin/php scripts/verify-doc-examples.php docs` → 退出 0。

- [ ] **Step 4: 全量 parity + 禁捕获 + 链接检查**

Run: 对 20 个页对逐一 parity diff → 全部无输出；
`grep -rnE '\buse\s*\(' docs/` → 无匹配；
链接检查：`find docs -name '*.md' | while read f; do grep -oE '\]\([^)#]+\.md' "$f" | sed 's/](//' | while read l; do [ -f "$(dirname "$f")/$l" ] || echo "BROKEN: $f -> $l"; done; done` → 无输出。

- [ ] **Step 5: Commit**

```bash
git add README.md specs/002-user-docs/notes.md
git commit -m "docs: refresh README version promise, divergence notes, full verification"
```