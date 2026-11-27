# QoS 与主循环

[English](../../en/guide/09-qos-mainloop.md) | [中文](../../zh/guide/09-qos-mainloop.md)

## 选择循环模型

- **脚本/批处理：** 派发工作后用 [`drain`](../api/loop.md#drain) 等待。多数示例采用此模型。
- **长驻服务：** 保持[源](05-sources.md) resume，用自己的循环包裹 `drain` 推进。v1 中 [`main`](../api/loop.md#main) 等价于无超时的 `drain()`——未完成工作结束即返回，而非永不返回。

## drain

`drain` 等待线程池的未完成任务计数：

```php
$q = GCD\queue_create('w', 1);
$c = new GCD\Atomic(0, 'd0');
GCD\async($q, function (string $n) { usleep(50000); GCD\Atomic::open($n)->inc(); }, ['d0']);
GCD\drain(0.0);
echo $c->get(), "\n";   // 0 —— 探测不等待
GCD\drain();
echo $c->get();         // 1
```

超时语义：负值无限等待，`0` 探测，正值限定等待。无返回值——检查自己的状态。另记：drain 只等已在途的工作，不等*未来*的源事件。

## v1 的 main

```php
$q = GCD\queue_create('w', 2);
$c = new GCD\Atomic(0, 'main-demo');
GCD\async($q, function (string $n) { GCD\Atomic::open($n)->inc(); }, ['main-demo']);
GCD\main();
echo $c->get();   // 1
```

永不返回的源驱动 `dispatch_main` 在 v1 未实现；服务请写成 `while (...) { GCD\drain($tick); }` 并保持源 resume。

## QoS 等级

五档，从高到低：`QOS_USER_INTERACTIVE`（4）、`QOS_USER_INITIATED`（3）、`QOS_DEFAULT`（2）、`QOS_UTILITY`（1）、`QOS_BACKGROUND`（0）。设置在队列上；未设自身 QoS 的队列沿目标链继承：

```php
$parent = GCD\queue_create('parent', 2);
GCD\queue_set_qos($parent, GCD\QOS_USER_INITIATED);
$child = GCD\queue_create('child', 1);
GCD\queue_set_target($child, $parent);
echo GCD\queue_get_qos($child) === GCD\QOS_USER_INITIATED ? 'inherit-ok' : 'bad';
```

v1 的 QoS **仅被记录**：值被存储并沿目标链解析，但调度并不使用它——无操作系统调度效果——且 `qos_class_self()` 恒返回 `QOS_DEFAULT`。见[队列参考](../api/queue.md#qos-常量)。