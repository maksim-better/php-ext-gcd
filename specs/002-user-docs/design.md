# ext-gcd 用户文档设计（中英双语 API 参考 + 使用指南）

- 日期：2026-11-27
- 状态：待批准
- 选定方案：A —— 按模块分页的完整双语镜像

## 意图与背景（为什么做）

`docs/` 目录已存在但为空。目前面向使用者的材料只有一份 48 行、英文、偏构建说明的
README；`specs/001-php-gcd-extension/` 是开发视角的规格文档（FR 编号、任务拆解、
实现日志），不适合扩展使用者查阅。

扩展使用者（ZTS PHP 上的 PHP 开发者）需要能够：

1. 安装并启用扩展；
2. 理解线程模型与任务规则，安全地写出并发代码；
3. 查到任意函数/类的准确签名、参数、返回值、异常；
4. 按任务找到可运行的示例（派发与等待、同步原语、事件源、共享容器、错误处理）。

项目所有者中文工作，扩展生态以英文为主，两种语言的读者都是一等受众。

## 目标

- **完整 API 参考**：覆盖全部 43 个 `GCD\` 函数、5 个 QoS 常量、9 个类
  （4 个纯句柄类 + 5 个带方法类）、3 个异常类。签名以 C 源码 arginfo 为准逐个核对，
  含参数表、返回值、抛出的异常、线程安全/阻塞行为说明、短示例。
- **任务导向使用指南**：从快速开始到线程模型，再到按任务组织的章节。
- **中英完全对等**：`docs/en/` 与 `docs/zh/` 结构相同、页面相同、锚点相同，
  页面顶部互链对方语言版本。
- **示例全部可运行**：在本机已安装的 PHP 8.6 ZTS（homebrew php-zts，扩展已启用）
  上逐段验证。
- **README 刷新**：支持版本更新为 8.4–8.6 并标注验证状态（8.6 本机实测 29/29；
  8.4 为 Docker 构建目标；8.5 理论兼容未验证），增加 docs 入口链接。

## 非目标

- 不做静态站点 / 发布流水线（纯 Markdown，GitHub 直接渲染）。
- 不做 PHP stub 自动生成。
- 不写内部架构/贡献者文档（specs/ 已覆盖开发视角；指南中的「工作原理」仅到
  使用者需要理解的程度）。
- 不翻译 specs/。
- 不列「v1 未实现」功能清单（只文档化已实现 API；路线图在 specs/）。
- 不为 NTS / FPM / Windows 写支持文档（仅明确「不支持」）。

## 反目标（做出来就算失败的情况）

- 参考与实现漂移——arginfo 是唯一签名来源（arginfo 无类型信息，类型/默认值/返回类型
  须读 C 函数体）；specs 契约是设计期提案，含约 15 个 v1 未实现的函数/类
  （after/block_*/io_* 等），**不可作为语义权威**；行为语义以 C 源码 + tests/ +
  examples/crawler.php 为准，差异记录下来。
- 中英混排单文件（读者被迫滚过另一种语言）。
- 示例展示 v1 不支持的用法——**任务闭包不能捕获变量**（no `use (...)`），
  参数必须可序列化；指南必须把 argv 模式作为默认写法教。
- 把 FR 编号规格风格带进用户文档。

## 约束

- 扩展仅支持 ZTS PHP 8.4–8.6（用户拍板）：8.6 本机实测通过（29/29）；
  8.4 为 Docker 构建目标；8.5 理论兼容未验证；8.2/8.3 不再承诺。
  Linux（epoll）/ macOS（kqueue）；QoS 在 Linux 为 best-effort。
- v1 任务 = 闭包 + 可序列化 argv，无捕获变量。
- 纯 Markdown、相对链接、GitHub 可渲染；不引入构建步骤。
- 双语对等：同构、同锚点。

## 方案对比

| 方案 | 内容 | 取舍 | 结论 |
|---|---|---|---|
| **A 按模块分页完整镜像** | en/zh 各约 20 个文件：api/ 每模块一页 + guide/ 9 章 | 查阅体验接近 PHP 官方手册，每函数有示例空间，更新按模块定位；文件多、需同步两套 | **选定**（用户拍板） |
| B 单页紧凑镜像 | 每语言一份表格化 api-reference.md + 一份 guide.md | 建得快、维护省；单页过长、放不下逐函数示例 | 放弃 |
| 中英混排单文件 | 一页内含两种语言 | 文件少；阅读体验差 | 放弃 |
| stub 自动生成 | 写 PHP stub 生成参考再翻译 | 签名精确；12 个类不值得搭机械，指南仍需手写 | 放弃 |

## 目录结构

```
docs/
  README.md                  # 总入口：选语言 + 文档地图
  en/
    README.md                # 英文索引 + 目录
    guide/
      01-quickstart.md       # 安装/启用（phpize 路线 + homebrew ZTS 路线）、第一个异步任务
      02-concepts.md         # 队列（串行/并发）、线程模型、任务规则（无捕获、可序列化 argv）、句柄对象
      03-dispatch.md         # async/sync/barrier/apply/once 与等待模式
      04-sync.md             # group（enter/leave/wait/notify）、semaphore
      05-sources.md          # 事件源：timer/signal/proc 等（类型清单以源码核对）
      06-data.md             # data_* 与 Data 类；配合 SOURCE_READ/WRITE 的 fd 读写（v1 无 dispatch_io API）
      07-containers.md       # Atomic/Map/Set/ConcurrentQueue 共享容器
      08-errors.md           # 三层错误上报、错误处理器、三个异常类
      09-qos-mainloop.md     # QoS 等级、main()/drain() 双主循环模型
    api/
      README.md              # 总索引：函数按组 + 字母序、类、常量、异常
      queue.md               # queue_create/global_queue/current_queue/main_queue/
                             # suspend/resume/set_specific/get_specific/set_target +
                             # QoS 函数与 5 个常量 + Queue 句柄类
      dispatch.md            # async/sync/barrier_async/barrier_sync/apply/once
      group.md               # group_* 6 个函数 + Group 句柄类
      semaphore.md           # semaphore_* 4 个函数 + Semaphore 句柄类
      source.md              # source_* 7 个函数 + Source 句柄类
      data.md                # data_* 4 个函数 + Data 类（size()）
      loop.md                # main/drain
      errors.md              # set_error_handler/queue_set_error_handler + 3 个异常类
      containers.md          # Atomic/Map/Set/ConcurrentQueue 类（逐方法）
  zh/                        # 与 en/ 完全同构，内容为中文
```

每语言：1 索引 + 9 指南 + 10 参考 = 20 个文件，加 docs/README.md 共 41 个文件。

## API 参考页格式

每个函数一节，固定结构：

1. **签名**：从 arginfo 提取，含类型与默认值（如 `GCD\async(GCD\Queue $queue, Closure $task, array $argv = []): void`——实际签名以核对结果为准）
2. **参数表**：名称 / 类型 / 说明
3. **返回值**
4. **错误与异常**：抛什么、何时抛
5. **备注**：阻塞行为、线程安全、平台差异（如 QoS Linux best-effort）
6. **示例**：可运行短片段

类页：创建方式 → 方法一览 → 逐方法同上结构。句柄类（Queue/Group/Semaphore/Source，
源码中 `INIT_CLASS_ENTRY(..., NULL)` 无方法）说明其由哪个函数创建、生命周期、
不可直接实例化。异常页说明继承关系（实现时从源码核对父类）与触发场景。

## 准确性与验证流程

1. 逐函数从 C 源码提取 arginfo（`ZEND_BEGIN_ARG_INFO` / `ZEND_ARG_*`），作为签名唯一来源。
2. specs 契约仅作历史设计参考（提案，与实现有漂移）；行为语义从 C 源码 + tests/ +
   crawler.php 提取；差异记入实现备注文件
   `specs/002-user-docs/notes.md`（实现期创建）。
3. 每段示例在本机 PHP 8.6 ZTS 上运行验证（扩展已经 conf.d 启用）。
4. 双语对等检查：页面清单、标题层级、锚点一致。
5. README 更新：版本描述 8.4–8.6、docs 链接；保留 Docker 构建路线。

## 留给实现者的决定

- 具体行文、示例选材：优先 `examples/crawler.php`（当前，展示 `open()` 命名注册 +
  `group_wait` 惯用法）与 tests/；`usage.php` 可运行但注释过时，复用其模式须核对。
- 锚点命名与交叉链接细节。
- 已核实的实现事实（直接采用）：3 个异常类均继承 `Error`；6 个 `SOURCE_*` 常量；
  4 个容器类均有静态 `open()`（进程内命名注册）；`Data` 仅 `size()`。
  其余精确签名以 C 函数体核对为准。
- 指南章节内的示例顺序与篇幅分配。

## 后续扩展（本次不做）

- GitHub Pages 发布、更多语言、按版本分档文档。