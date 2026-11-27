# ext-gcd 正确性与语义修复实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除 4 个崩溃/致命缺陷、修复 6 个正确性缺陷、补齐 barrier 顺序与 current_queue 语义，随后同步双语文档。

**Architecture:** 逐项 TDD：先写失败 phpt（崩溃项确认崩溃/错误输出），再改 C，重建跑绿，最后全量回归。结构性改动（S1 序号准入）先于复用它的 D5/D3。

**Tech Stack:** C（Zend/Pthreads）、PHP 8.6 ZTS（/opt/homebrew/opt/php-zts/bin/php）、phpt（run-tests.php）、make。

**Spec:** `specs/003-correctness-semantics/design.md`

## Global Constraints

- ZTS PHP 8.4–8.6；本机 macOS 8.6 实测。
- 现有 29 个 phpt 必须保持绿；TSan 套件 0 警告（docker 路线，最终任务跑）。
- 异常沿用 Capture/Serialization/Deadlock（继承 Error）+ 标准 TypeError/ValueError/Error。
- try/catch 维持「接受」，只修过时消息（D10）。
- 验证命令：`make test TESTS=tests/<path>.phpt`（自动重建、剥离已安装 ini、加载 modules/ 新构建）；全量 `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test`。
- 提交不加 Co-Authored-By；摘要英文 `fix(scope): ...`。
- 规格/计划放 specs/003-correctness-semantics/。

## Review Focus

1. **S1 并发提交死锁**：notify/source 线程并发提交时 seq 序与池序 invert → 固定 worker 池硬死锁。Task 8 加并发提交压力测试。
2. **D5 group UAF**：group_wait 返回后立即 unset group，notify 任务借用 handler → _use-after-free_。Task 9 加 unset 生命周期测试。
3. **D3 空闲 worker 被信号杀**：parked 在 cond_wait 的 worker 到不了任务间掩码点。Task 10 测试先 boot+idle 再发信号。
4. **D6 target release 级联/double-free**：替换 target、queue 销毁时的引用计数。Task 5 加替换/销毁测试。
5. **D4 嵌套闭包缓存**：真正 SIGBUS 来自嵌套 dynamic_func_defs 的 NULL run_time_cache。Task 7 测试任务内嵌套函数调用。

## File Structure

新增测试（tests/）：
```
runtime/current_queue_concurrent.phpt   runtime/capture_message.phpt
runtime/argv_object.phpt                runtime/script_symbol.phpt
error/handler_message.phpt              error/global_handler_capture.phpt
container/set_nonserializable.phpt      queue/target_lifetime.phpt
queue/barrier_order.phpt                source/timer_oneshot.phpt
source/signal_after_pool.phpt           sync/group_notify_reuse.phpt
```
修改 C：`src/runtime/{task.c,copy.c,error.c,thread_pool.c}`、`src/container/set.c`、`src/queue/{queue.c,group.c,dispatch.c}`、`src/source/source.c` + 对应头文件（task.h/queue.h/group.h/thread_pool.h）。
文档同步（Task 11）：docs/{en,zh} 相关页 + notes.md。

---

### Task 1: task.c 基础（D10 消息 + S2 current_queue）

**Files:**
- Modify: `src/runtime/task.c`（:128 current_queue 条件；:169 消息）
- Test: `tests/runtime/current_queue_concurrent.phpt`, `tests/runtime/capture_message.phpt`

**Interfaces:**
- Produces: current_queue 在并发队列任务内非 null（后续文档/测试依赖）。

- [ ] **Step 1: 写两个失败 phpt**

`current_queue_concurrent.phpt`（--TEST--/SKIPIF extension_loaded gcd/--FILE--/--EXPECT-- set）：
```php
$q = GCD\queue_create('c', 2);
$out = tempnam(sys_get_temp_dir(), 'cq');
GCD\async($q, function ($f) { file_put_contents($f, GCD\current_queue() === null ? 'null' : 'set'); }, [$out]);
GCD\drain();
echo file_get_contents($out); unlink($out);
```
EXPECT `set`。

`capture_message.phpt`（EXPECT `no-trycatch`）：
```php
$q = GCD\queue_create('w', 1);
try { GCD\async($q, function () { static $x = 0; }); }
catch (GCD\CaptureException $e) { echo strpos($e->getMessage(), 'try-catch') === false ? 'no-trycatch' : 'stale'; }
```

- [ ] **Step 2: 跑红**

Run: `make test TESTS="tests/runtime/current_queue_concurrent.phpt tests/runtime/capture_message.phpt"`
Expected: FAIL（current_queue 输出 null；消息含 try-catch 输出 stale）。

- [ ] **Step 3: 改 C**

task.c:128 `if (q && q->width == 1) gcd_current_queue = q;` → `if (q) gcd_current_queue = q;`（sync 死锁保护仍仅 width==1，不动）。
task.c:169 消息 `"closure captures variables or uses statics/try-catch (unsupported in v1)"` → `"closure captures variables or uses statics (unsupported in v1)"`。

- [ ] **Step 4: 跑绿**

Run: 同 Step 2 → PASS。

- [ ] **Step 5: 全量回归**

Run: `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test` → 29+2 全绿（注意 sync_deadlock 等仍绿）。

- [ ] **Step 6: Commit**

```bash
git add src/runtime/task.c tests/runtime/current_queue_concurrent.phpt tests/runtime/capture_message.phpt
git commit -m "fix(runtime): current_queue on concurrent queues (S2) + stale capture message (D10)"
```

---

### Task 2: 错误上报（D7 消息读取 + D8 全局处理器检查）

**Files:**
- Modify: `src/runtime/error.c`（:21 读 message；gcd_error_set_global + PHP_FUNCTION(set_error_handler)）
- Test: `tests/error/handler_message.phpt`, `tests/error/global_handler_capture.phpt`

**Interfaces:**
- Produces: 处理器对 Error 系收到真实 message；全局处理器拒绝捕获闭包。

- [ ] **Step 1: 写两个失败 phpt**

`handler_message.phpt`（EXPECT `hello-error`）：
```php
$m = new GCD\Map('msg');
GCD\set_error_handler(function ($msg, $cls) { GCD\Map::open('msg')->set($cls, $msg); });
$q = GCD\queue_create('w', 1);
GCD\sync($q, function () { throw new Error('hello-error'); });
GCD\drain();
echo $m->get('Error');
```

`global_handler_capture.phpt`（EXPECT `rejected`）：
```php
$x = 1;
try { GCD\set_error_handler(function () use (&$x) {}); echo 'accepted'; }
catch (GCD\CaptureException $e) { echo 'rejected'; }
```

- [ ] **Step 2: 跑红**

Run: `make test TESTS="tests/error/handler_message.phpt tests/error/global_handler_capture.phpt"`
Expected: FAIL（message 空；输出 accepted）。

- [ ] **Step 3: 改 C**

error.c:21 读 message 用异常自身 ce：`zend_read_property(ex->ce, ex, "message", ...)`（替换固定 zend_ce_exception）。
`gcd_error_set_global`：检查 `gcd_op_array_copy_persistent` 返回 NULL 时抛 `gcd_capture_ce` 并返回失败；PHP_FUNCTION(set_error_handler) 传播（RETURN_THROWS）。参照 queue_set_error_handler 的检查模式。

- [ ] **Step 4: 跑绿** → PASS。

- [ ] **Step 5: 全量回归** → 全绿（reporting.phpt 仍绿）。

- [ ] **Step 6: Commit**

```bash
git add src/runtime/error.c tests/error/handler_message.phpt tests/error/global_handler_capture.phpt
git commit -m "fix(error): read message via exception ce (D7) + reject capturing global handler (D8)"
```

---

### Task 3: argv 序列化检查（D1）

**Files:**
- Modify: `src/runtime/task.c` `gcd_task_build`（:172-177）
- Test: `tests/runtime/argv_object.phpt`

- [ ] **Step 1: 写失败 phpt**（EXPECT `rejected`）

```php
$q = GCD\queue_create('w', 1);
try { GCD\async($q, function () {}, [new stdClass]); echo 'accepted'; }
catch (GCD\SerializationException $e) { echo 'rejected'; }
```

- [ ] **Step 2: 跑红**

Run: `make test TESTS=tests/runtime/argv_object.phpt`
Expected: FAIL（SIGSEGV，进程崩溃）。

- [ ] **Step 3: 改 C**

gcd_task_build：`if (gcd_zval_copy_persistent(&pargv, argv) == FAILURE) { zend_throw_exception(gcd_serialization_ce, "value is not serializable", 0); return NULL; }`（调用方已处理 NULL → RETURN_THROWS）。

- [ ] **Step 4: 跑绿** → PASS（rejected，进程存活）。

- [ ] **Step 5: 全量回归** → 全绿。

- [ ] **Step 6: Commit**

```bash
git add src/runtime/task.c tests/runtime/argv_object.phpt
git commit -m "fix(runtime): throw SerializationException on non-copyable argv (D1)"
```

---

### Task 4: Set 非序列化值（D2）

**Files:**
- Modify: `src/container/set.c` `make_key` + add/contains/remove
- Test: `tests/container/set_nonserializable.phpt`

- [ ] **Step 1: 写失败 phpt**（EXPECT `rejected-rejected`）

```php
$s = new GCD\Set('s');
try { $s->add(function () {}); echo 'accepted'; } catch (GCD\SerializationException $e) { echo 'rejected'; }
echo '-';
$fp = fopen('php://memory', 'r');
try { $s->add($fp); echo 'accepted'; } catch (GCD\SerializationException $e) { echo 'rejected'; }
```

- [ ] **Step 2: 跑红** → FAIL（SIGSEGV）。

- [ ] **Step 3: 改 C**

make_key：入口类型预检 `Z_TYPE_P(v) == IS_RESOURCE` → 抛 gcd_serialization_ce；php_var_serialize 后检查 `buf.s == NULL` → 抛异常。add/contains/remove 在 make_key 后检查 EG(exception)，有则释放已分配资源并 RETURN_THROWS。

- [ ] **Step 4: 跑绿** → PASS。

- [ ] **Step 5: 全量回归** → 全绿（set.phpt 仍绿）。

- [ ] **Step 6: Commit**

```bash
git add src/container/set.c tests/container/set_nonserializable.phpt
git commit -m "fix(container): reject closures/resources in Set with SerializationException (D2)"
```

---

### Task 5: queue target 生命周期（D6）

**Files:**
- Modify: `src/queue/queue.c` `queue_set_target` + queue free/release
- Test: `tests/queue/target_lifetime.phpt`

- [ ] **Step 1: 写失败 phpt**（EXPECT `1`）

```php
$p = GCD\queue_create('p', 2);
GCD\queue_set_qos($p, GCD\QOS_UTILITY);
$c = GCD\queue_create('c', 1);
GCD\queue_set_target($c, $p);
unset($p);
echo GCD\queue_get_qos($c);
```

- [ ] **Step 2: 跑红** → FAIL（输出 0/垃圾）。

- [ ] **Step 3: 改 C**

queue_set_target：设新 target 前 `gcd_queue_retain(t)`；若 q->target 旧值非空先 release；赋值。queue free/release 路径：release q->target。环检查已保证无环，级联终止。

- [ ] **Step 4: 跑绿** → PASS（1）。

- [ ] **Step 5: 全量回归** → 全绿（hierarchy.phpt 仍绿）。

- [ ] **Step 6: Commit**

```bash
git add src/queue/queue.c tests/queue/target_lifetime.phpt
git commit -m "fix(queue): retain target queue to keep QoS chain valid (D6)"
```

---

### Task 6: timer 一次性默认（D9）

**Files:**
- Modify: `src/source/source.c` `source_set_timer` + source_thread TIMER 分支
- Test: `tests/source/timer_oneshot.phpt`

- [ ] **Step 1: 写失败 phpt**（EXPECT `1`）

```php
$q = GCD\queue_create('w', 1);
$c = new GCD\Atomic(0, 't9');
$src = GCD\source_create(GCD\SOURCE_TIMER, 0, 0, $q);
GCD\source_set_event_handler($src, function () { GCD\Atomic::open('t9')->inc(); });
GCD\source_set_timer($src, 0.01);
GCD\source_resume($src);
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $c->get();
```

- [ ] **Step 2: 跑红** → FAIL（未初始化 interval，计数 != 1）。

- [ ] **Step 3: 改 C**

source_set_timer：`double start, interval = 0, leeway = 0;`（interval 默认 0）。source_thread TIMER：`sleep_sec(start); dispatch_handler; if (interval <= 0) break; while(!cancelled){ sleep_sec(interval); dispatch_handler; }`（interval<=0 一次性）。

- [ ] **Step 4: 跑绿** → PASS（1）。

- [ ] **Step 5: 全量回归** → 全绿（timer.phpt 仍绿，它显式传 interval）。

- [ ] **Step 6: Commit**

```bash
git add src/source/source.c tests/source/timer_oneshot.phpt
git commit -m "fix(source): default timer interval to one-shot (D9)"
```

---

### Task 7: 脚本符号崩溃（D4，先调试定位）

**Files:**
- Modify: `src/runtime/copy.c` 和/或 `src/runtime/task.c gcd_invoke`（按 lldb 结论）
- Test: `tests/runtime/script_symbol.phpt`

**Interfaces:**
- Consumes: vendored PHP 源码 docker/php-src（Zend VM 路径）。

- [ ] **Step 1: 写失败 phpt**（EXPECT `1`）

```php
function script_helper() { return 1; }
$c = new GCD\Atomic(0, 'd4');
GCD\set_error_handler(function ($msg, $cls) { GCD\Atomic::open('d4')->inc(); });
$q = GCD\queue_create('w', 1);
GCD\async($q, function () { script_helper(); });
GCD\drain();
echo $c->get();
```

- [ ] **Step 2: 跑红 + 定位**

Run: `make test TESTS=tests/runtime/script_symbol.phpt` → FAIL（SIGBUS）。
用 lldb 跑最小复现脚本拿 backtrace，对照 docker/php-src 确认嫌疑（嵌套 dynamic_func_defs 的 run_time_cache__ptr 被 copy.c:131 置空但 gcd_invoke 只初始化顶层缓存；arg_info 浅拷贝；IS_CONSTANT_AST 字面量）。把根因记入 specs/003-correctness-semantics/notes.md。

- [ ] **Step 3: 改 C（根因驱动）**

按定位修：为嵌套 op_array 正确初始化 run_time_cache（或在 gcd_invoke 递归初始化），/深拷贝 arg_info，/处理 IS_CONSTANT_AST。目标：未定义符号走正常 zend_throw_error 路径抛可捕获 Error，经上报层计数。

- [ ] **Step 4: 跑绿** → PASS（1，进程存活）。

- [ ] **Step 5: 全量回归** → 全绿。

- [ ] **Step 6: Commit**

```bash
git add src/runtime/copy.c src/runtime/task.c tests/runtime/script_symbol.phpt specs/003-correctness-semantics/notes.md
git commit -m "fix(runtime): make unavailable script symbols throw catchable Error, not SIGBUS (D4)"
```

---

### Task 8: barrier 真顺序（S1，结构性）

**Files:**
- Modify: `src/runtime/task.c`（gate）、`src/runtime/task.h`（seq 字段）、`src/queue/queue.h`（next_seq/next_admit）、`src/queue/dispatch.c`、`src/queue/group.c`、`src/source/source.c`（提交点）
- Test: `tests/queue/barrier_order.phpt`

**Interfaces:**
- Produces: 带序号提交 helper（如 `gcd_queue_submit_sequenced(gcd_queue*, gcd_task*)`），D5/D3 复用；gate 按序准入。

- [ ] **Step 1: 写失败 phpt**（EXPECT `0`）

20 轮，每轮 barrier 后提交的任务不得早于 barrier：
```php
$bad = 0;
for ($r = 0; $r < 20; $r++) {
    $q = GCD\queue_create('b', 4);
    $order = new GCD\ConcurrentQueue('ord');
    GCD\async($q, function () { usleep(20000); });
    GCD\barrier_async($q, function () { GCD\ConcurrentQueue::open('ord')->push('barrier'); });
    GCD\async($q, function () { GCD\ConcurrentQueue::open('ord')->push('later'); });
    GCD\drain();
    if ($order->tryPop() !== 'barrier') $bad++;
}
echo $bad;
```

- [ ] **Step 2: 跑红** → FAIL（bad > 0）。

- [ ] **Step 3: 改 C**

gcd_task 加 `uint64_t seq`；gcd_queue 加 `uint64_t next_seq, next_admit`（width_mutex 保护）。
新增 `gcd_queue_submit_sequenced(q, t)`：持 width_mutex 取 `t->seq = q->next_seq++`，**保持持锁**调 gcd_pool_submit（锁序 width_mutex→g_mutex 安全），释放。
dispatch.c（async/sync/barrier_*/apply 的 width>1 分支）、group.c（group_async + notify）、source.c（dispatch_handler + DATA_ADD）的并发提交统一改走 helper。width-1 走原 enqueue（已 FIFO）。
gate（task.c）：非 barrier 等 `t->seq == q->next_admit && inflight < width && !barrier_active && !suspended`；barrier 等 `t->seq == q->next_admit && inflight == 0 && !barrier_active && !suspended`；准入时 `q->next_admit++` 并 **broadcast width_cond**。

- [ ] **Step 4: 跑绿** → PASS（0）。

- [ ] **Step 5: 并发压力 + 全量回归**

加并发提交压力脚本（多主线程序列 + notify/source 混合）跑多次确认无死锁；`make test` 全绿（barrier.phpt/barrier_exclusive.phpt 仍绿）。

- [ ] **Step 6: Commit**

```bash
git add src/runtime/task.c src/runtime/task.h src/queue/queue.h src/queue/dispatch.c src/queue/group.c src/source/source.c tests/queue/barrier_order.phpt
git commit -m "fix(queue): sequenced admission for true barrier ordering (S1)"
```

---

### Task 9: notify 生命周期 + 一次性（D5）

**Files:**
- Modify: `src/queue/group.c`（notify 创建/leave/free）、`src/runtime/task.c`（destroy no-leave）、`src/runtime/task.h`
- Test: `tests/sync/group_notify_reuse.phpt`

**Interfaces:**
- Consumes: Task 8 的带序号提交 helper。

- [ ] **Step 1: 写失败 phpt**（EXPECT `1\n1`）

```php
$g = GCD\group_create();
$nq = GCD\queue_create('n', 1);
$wq = GCD\queue_create('w', 2);
$notify = new GCD\Atomic(0, 'n5');
GCD\group_notify($g, $nq, function () { GCD\Atomic::open('n5')->inc(); });
GCD\group_async($g, $wq, function () { usleep(20000); });
GCD\group_wait($g);
GCD\drain();
$done = new GCD\Atomic(0, 'd5');
GCD\group_async($g, $wq, function () { usleep(50000); GCD\Atomic::open('d5')->inc(); });
GCD\group_wait($g);
echo $done->get(), "\n";
GCD\drain();
echo $notify->get();
```
第一行 1（第二批真正等待），第二行 1（notify 一次性）。

- [ ] **Step 2: 跑红** → FAIL（done=0 提前返回；notify=2 重 fired）。

- [ ] **Step 3: 改 C**

notify 任务加 no-leave 标志（task.h 字段）：完成时不调 gcd_group_leave，但仍 release group 生命周期引用（防 UAF）。gcd_group_leave 触发 notify 时**消费列表**（取出并释放节点，handler 拷贝归 notify 任务所有或深拷贝），后续 1→0 不再重 fired。group 本身计数正常，可复用。

- [ ] **Step 4: 跑绿** → PASS（1\n1）。

- [ ] **Step 5: UAF 测试 + 全量回归**

加 unset-group-after-wait 脚本（group_wait 后立即 unset($g)，notify 仍安全完成）跑多次；`make test` 全绿。

- [ ] **Step 6: Commit**

```bash
git add src/queue/group.c src/runtime/task.c src/runtime/task.h tests/sync/group_notify_reuse.phpt
git commit -m "fix(group): notify lifetime ref + one-shot consumption, reusable group (D5)"
```

---

### Task 10: 信号掩码全 worker（D3）

**Files:**
- Modify: `src/source/source.c`、`src/runtime/thread_pool.c`、`src/runtime/thread_pool.h`
- Test: `tests/source/signal_after_pool.phpt`

**Interfaces:**
- Consumes: Task 8 的 source 提交 helper。

- [ ] **Step 1: 写失败 phpt**（EXPECT `1`）

```php
$q = GCD\queue_create('boot', 2);
GCD\async($q, function () {});
GCD\drain();
usleep(50000);
$c = new GCD\Atomic(0, 's3');
$src = GCD\source_create(GCD\SOURCE_SIGNAL, SIGUSR1, 0, $q);
GCD\source_set_event_handler($src, function () { GCD\Atomic::open('s3')->inc(); });
GCD\source_resume($src);
usleep(50000);
posix_kill(getmypid(), SIGUSR1);
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $c->get();
```

- [ ] **Step 2: 跑红** → FAIL（进程被 SIGUSR1 终止，exit 158）。

- [ ] **Step 3: 改 C**

全局 `sigset_t g_source_sigset` + generation（thread_pool）。SIGNAL resume：加入 signo、阻塞调用线程、bump generation、**唤醒空闲 worker** 使其重新掩码。worker boot 与任务循环间（含被唤醒后）`pthread_sigmask(SIG_BLOCK, g_source_sigset)`。非信号源线程（timer/poll）创建时也应用。cancel 后保持阻塞（最小范围，文档保留警示）。

- [ ] **Step 4: 跑绿** → PASS（1，进程存活）。

- [ ] **Step 5: 全量回归** → 全绿（signal.phpt 仍绿）。

- [ ] **Step 6: Commit**

```bash
git add src/source/source.c src/runtime/thread_pool.c src/runtime/thread_pool.h tests/source/signal_after_pool.phpt
git commit -m "fix(source): block source signals across all workers incl. idle (D3)"
```

---

### Task 11: 全量验证 + TSan + 文档同步

**Files:**
- Modify: docs/{en,zh} 相关页、specs/002-user-docs/notes.md、specs/003-correctness-semantics/notes.md
- Test: 全套 phpt + 文档门禁

- [ ] **Step 1: 全量 phpt + TSan**

Run: `NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test` → 全绿（29 原有 + 12 新增）。
TSan：按 docker/ 既有路线跑 TSan 套件 → 0 警告（若本机无 TSan 环境，记录并说明）。

- [ ] **Step 2: 同步文档**

移除已修复限制的警示：argv 对象/Set 闭包改为「抛 SerializationException」（非崩溃）；信号源去掉「先于首次派发」强制要求（保留「源信号进程级阻塞」）；current_queue 改全队列；barrier 恢复顺序保证；group 可复用 + notify 一次性；处理器 message 正常；全局处理器抛 CaptureException；timer 省略 interval = 一次性。en/zh 同步。

- [ ] **Step 3: 文档门禁**

Run: `/opt/homebrew/opt/php-zts/bin/php scripts/verify-doc-examples.php docs` → 全过；20 对 parity；禁捕获 grep；锚点审计；链接检查。

- [ ] **Step 4: 更新 notes**

specs/002-user-docs/notes.md 差异表标注「已修复」；specs/003 notes 记 D4 根因与各项结论。

- [ ] **Step 5: Commit**

```bash
git add docs specs/002-user-docs/notes.md specs/003-correctness-semantics/notes.md
git commit -m "docs: sync user docs with corrected v1 behavior; mark divergences fixed"
```