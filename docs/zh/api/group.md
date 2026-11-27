# Group

[English](../../en/api/group.md) | [中文](../../zh/api/group.md)

## Group 句柄

`GCD\Group` 是无方法的句柄对象，由 `group_create` 创建。它跟踪未完成工作的计数；`group_wait` 阻塞到计数归零。group 可跨批次复用。每个 `group_notify` 注册触发一次（计数首次归零时）；需要每批通知就每批注册新 notify。

## group_create

**签名**

```text
GCD\group_create(): GCD\Group
```

**参数** —— 无。

**返回值** —— 新建的空 group。

**错误与异常** —— 无。

**备注** —— 每次调用创建独立的 group。

**示例**

```php
$g = GCD\group_create();
var_export(GCD\group_wait($g, 0.0));   // true —— 空 group
```

## group_async

**签名**

```text
GCD\group_async(GCD\Group $group, GCD\Queue $queue, Closure $task, array $argv = []): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$group` | `GCD\Group` | 要加入的 group |
| `$queue` | `GCD\Queue` | 目标队列 |
| `$task` | `Closure` | 规则同 `async`（无捕获/静态；允许 try/catch） |
| `$argv` | `array` | 可序列化参数 |

**返回值** —— 无（`null`）。不等待。

**错误与异常** —— 句柄类型错误抛 `TypeError`（"expected GCD\Group and GCD\Queue"）；任务构建错误同 [`async`](dispatch.md#async)。

**备注** —— 等价于 `group_enter` + `async` + 任务结束（含失败）时自动 `group_leave`。推荐的扇出惯用法。

**示例**

```php
$g = GCD\group_create();
$q = GCD\queue_create('fan', 4);
$c = new GCD\Atomic(0, 'ga-demo');
for ($i = 0; $i < 6; $i++) {
    GCD\group_async($g, $q, function (string $n) {
        GCD\Atomic::open($n)->inc();
    }, ['ga-demo']);
}
var_export(GCD\group_wait($g));   // true
echo "\n";
echo $c->get();                   // 6
```

## group_enter

**签名**

```text
GCD\group_enter(GCD\Group $group): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$group` | `GCD\Group` | 要进入的 group |

**返回值** —— 无。

**错误与异常** —— 非 Group 抛 `TypeError`（"expected GCD\Group"）。

**备注** —— 未完成计数加一。必须与 `group_leave` 配对；派发任务优先用 `group_async`。

**示例**

```php
$g = GCD\group_create();
GCD\group_enter($g);
var_export(GCD\group_wait($g, 0.0));   // false —— 仍在组内
echo "\n";
GCD\group_leave($g);
var_export(GCD\group_wait($g, 0.0));   // true
```

## group_leave

**签名**

```text
GCD\group_leave(GCD\Group $group): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$group` | `GCD\Group` | 要离开的 group |

**返回值** —— 无。

**错误与异常** —— 非 Group 抛 `TypeError`（"expected GCD\Group"）。

**备注** —— 计数减一；归零时唤醒 `group_wait` 并触发待执行的通知。

**示例** —— 见 [`group_enter`](#group_enter)。

## group_wait

**签名**

```text
GCD\group_wait(GCD\Group $group, float $timeout = -1): bool
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$group` | `GCD\Group` | 要等待的 group |
| `$timeout` | `float` | 秒数：< 0 永久等待（默认）；0 非阻塞探测；> 0 最多等待该秒数（支持小数） |

**返回值** —— 计数归零返回 `true`；超时返回 `false`。

**错误与异常** —— 非 Group 抛 `TypeError`（"expected GCD\Group"）。

**备注** —— 等待空 group 立即返回 `true`。

**示例** —— 见 [`group_async`](#group_async)。

## group_notify

**签名**

```text
GCD\group_notify(GCD\Group $group, GCD\Queue $queue, Closure $task): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$group` | `GCD\Group` | 要监视的 group |
| `$queue` | `GCD\Queue` | 通知运行其上的队列 |
| `$task` | `Closure` | 零参回调；同样的 v1 闭包规则 |

**返回值** —— 无。

**错误与异常** —— `TypeError`（"expected GCD\Group and GCD\Queue"）；`TypeError`（"notify must be a user-function Closure"）；`GCD\CaptureException`（"notify closure unsupported in v1"）。

**备注** —— 计数归零时回调被派发到 `$queue`——在 worker 上运行，而非调用线程。可注册多个通知；结果通过容器共享。每个注册触发一次；group 本身仍可复用（见[句柄说明](#group-句柄)）。

**示例**

```php
$g = GCD\group_create();
$q = GCD\queue_create('fan', 2);
$nq = GCD\queue_create('notify', 1);
$c = new GCD\Atomic(0, 'gn-demo');
GCD\group_notify($g, $nq, function () {
    GCD\Atomic::open('gn-demo')->inc();
});
GCD\group_async($g, $q, function () {});
GCD\group_wait($g);
GCD\drain();
echo $c->get();   // 1
```