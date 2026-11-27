# 主循环

[English](../../en/api/loop.md) | [中文](../../zh/api/loop.md)

## drain

**签名**

```text
GCD\drain(float $timeout = -1): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$timeout` | `float` | 秒数：< 0 等到没有未完成工作（默认）；0 立即返回；> 0 最多等待该秒数 |

**返回值** —— 无。没有成功标志——是否完成要检查自己的状态（容器、文件）。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。

**备注** —— 等待线程池的未完成任务计数。**不**等待未来事件：未完成数归零即返回，哪怕之后源还会触发。drain 前给源留出派发时间（见[源指南](../guide/05-sources.md)）。

**示例**

```php
$q = GCD\queue_create('w', 4);
$c = new GCD\Atomic(0, 'mc');
for ($i = 0; $i < 8; $i++) {
    GCD\async($q, function (string $n) {
        GCD\Atomic::open($n)->inc();
        usleep(1000);
    }, ['mc']);
}
GCD\drain(5.0);
echo $c->get();   // 8
```

## main

**签名**

```text
GCD\main(): void
```

**参数** —— 无。

**返回值** —— 无。

**错误与异常** —— 无。

**备注** —— v1 中 `main()` 等价于无超时的 `drain()`：等待未完成工作后返回。尚未实现永不返回的源驱动循环；周期工作请保持源 resume，在自己的循环里调 `drain()`。

**示例**

```php
$q = GCD\queue_create('w', 2);
$c = new GCD\Atomic(0, 'main-demo');
GCD\async($q, function (string $n) { GCD\Atomic::open($n)->inc(); }, ['main-demo']);
GCD\main();
echo $c->get();   // 1
```