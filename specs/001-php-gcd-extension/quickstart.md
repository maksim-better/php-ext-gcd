# Quickstart Validation Guide: PHP GCD 并发扩展

**Purpose**: 端到端验证扩展可用的运行/测试指南。API 细节见 [contracts/php-api.md](./contracts/php-api.md)，实体见 [data-model.md](./data-model.md)。不含实现代码。

## 前置条件

- **ZTS PHP 8.2+ CLI**（8.2/8.3/8.4）。NTS/FPM 不可用。
- 构建工具：phpize、autoconf、C 编译器（gcc/clang）、make。
- 平台：Linux（epoll）或 macOS（kqueue）。
- 可选：TSan/Helgrind/ASan 用于并发安全验证；systemd/supervisord 用于致命错误重启验证。

## 安装与构建

```bash
# 1. 取得 ZTS PHP（推荐用 docker/ 下的 ZTS 构建镜像，确保 --enable-zts）
php -v && php -i | grep 'Thread Safety'   # 期望：Thread Safety => enabled

# 2. 构建扩展
phpize
./configure --enable-gcd
make
make install

# 3. 启用（php.ini 或 -d）
php -d extension=gcd.so -m | grep gcd     # 期望：列出 gcd
```

**期望**：`php -m` 含 `gcd`；`php -i | grep 'Thread Safety'` 为 enabled。

## 验证场景（对应 User Stories / Success Criteria）

### V1 — 并行消费与保序（US1, SC-001）

```bash
php -d extension=gcd.so examples/parallel_consume.php
make test TESTS=tests/queue
```

**期望**：宽度 4 的并发队列任一时刻在途任务 ≤4 且并行执行；串行队列执行顺序严格等于投递顺序；向当前串行队列 sync 抛 `DeadlockException`。

### V2 — 共享容器并发安全（US2, SC-002）

```bash
# 64 线程压力：计数器 64×1000 次 inc、集合去重、队列 MPMC
php -d extension=gcd.so tests/stress/container_stress.php
# 竞态检测（TSan 构建）
make test TESTS=tests/stress   # 期望：TSan/Helgrind 无报告
```

**期望**：计数器终值恰为 64000（零丢失更新）；集合元素唯一；队列每元素恰消费一次；空 pop 阻塞超时返回 null、tryPop 立即返回 null；存入资源抛 `SerializationException`。

### V3 — 任务协调（US3）

```bash
php -d extension=gcd.so examples/coordination.php
make test TESTS=tests/sync
```

**期望**：group 全 leave 后 wait 返回/notify 触发；初值 3 的信号量任一时刻 ≤3 任务在执行段；barrier 排空在途后独占；once 恰好执行一次。

### V4 — 定时与事件（US4）

```bash
php -d extension=gcd.so examples/events.php
make test TESTS=tests/source
```

**期望**：after 不早于延时执行；timer source 周期触发；cancel 后任务不执行且 test_cancel 为真；proc source 在目标进程退出时回调。

### V5 — 并行迭代与 IO（US5）

```bash
make test TESTS=tests/io
```

**期望**：apply 并行执行 N 次迭代且调用线程等待全部完成；dispatch_data 不可变且拼接/子范围正确；dispatch_io 读写 fd 数据正确。

### V6 — QoS 与层级（US6）

```bash
make test TESTS=tests/qos
```

**期望**：高 QoS 队列优先获得调度（Linux best-effort）；target queue 层级 QoS 正确传播。

### V7 — 双主循环模型（FR-031）

```bash
php -d extension=gcd.so examples/loop_callback.php   # dispatch_main 回调驱动
php -d extension=gcd.so examples/loop_manual.php     # while + drain/step
```

**期望**：两种模型均能常驻运行、持续消费外部队列并进程内生产任务。

### V8 — 常驻爬虫长跑（SC-004）

```bash
# 外部队列消费 + 内部链接生产 + 并发爬取 + 共享去重/frontier/计数
php -d extension=gcd.so examples/crawler/crawler.php
# 配合外部监督验证致命错误重启（FR-036）
supervisord -c examples/crawler/supervisord.conf
```

**期望**：连续运行 ≥24h 无内存泄漏（ASan/valgrind）、无崩溃；可捕获异常经三层上报不崩进程；致命错误进程终止后被监督拉起。

### V9 — 异常三层上报（FR-030, SC-003）

```bash
make test TESTS=tests/error
```

**期望**：未设局部 handler 时异常走全局兜底（不静默吞）；per-queue handler 覆盖生效；group 聚合成员异常。

## 跨平台与性能（SC-005, SC-006, SC-007）

```bash
# CI 矩阵：Linux(epoll) + macOS(kqueue)，ZTS
.github/workflows/ci.yml   # 期望双平台全绿
# 性能基准（对比 ext-parallel / swoole 协程）
php -d extension=gcd.so benchmarks/io_throughput.php
```

**期望**：双平台测试套件 100% 通过；全量跨平台 API 表面 100% 有测试覆盖；I/O 吞吐基准数据可复现并公布。

## 完成判定

全部 V1–V9 期望达成 + 跨平台 CI 绿 + 基准可复现 ⇒ 特性端到端验证通过。