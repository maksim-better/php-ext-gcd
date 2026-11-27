# Semaphore

[English](../../en/api/semaphore.md) | [中文](../../zh/api/semaphore.md)

## Semaphore 句柄

`GCD\Semaphore` 是无方法的句柄对象，由 `semaphore_create` 创建。持有一个计数值：`semaphore_wait` 减一（为零时阻塞），`semaphore_signal` 加一。

## semaphore_create

**签名**

```text
GCD\semaphore_create(int $value, string $name = null): GCD\Semaphore
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$value` | `int` | 初始计数 |
| `$name` | `string\|null` | 可选的进程级注册名 |

**返回值** —— 新建的 semaphore 句柄。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。

**备注** —— 命名后任务可通过 `semaphore_open` 访问；未命名的 semaphore 只能在能传递句柄本身的地方使用。

**示例**

```php
$s = GCD\semaphore_create(1);
var_export(GCD\semaphore_wait($s, 0.0));   // true
echo "\n";
var_export(GCD\semaphore_wait($s, 0.0));   // false —— 已耗尽
echo "\n";
GCD\semaphore_signal($s);
var_export(GCD\semaphore_wait($s, 0.0));   // true
```

## semaphore_open

**签名**

```text
GCD\semaphore_open(string $name): GCD\Semaphore
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$name` | `string` | 创建时给的注册名 |

**返回值** —— 命名 semaphore 的句柄（同一底层计数）。

**错误与异常** —— 名字从未注册时抛 `ValueError`（"unknown GCD\Semaphore name"）。

**备注** —— 注册表进程级、持久；这是 worker 任务访问主线程所建 semaphore 的途径。

**示例**

```php
GCD\semaphore_create(2, 'limiter');
$s = GCD\semaphore_open('limiter');
var_export(GCD\semaphore_wait($s, 0.0));   // true
echo "\n";
try {
    GCD\semaphore_open('nope');
} catch (ValueError $e) {
    echo "unknown name rejected\n";
}
```

## semaphore_wait

**签名**

```text
GCD\semaphore_wait(GCD\Semaphore $semaphore, float $timeout = -1): bool
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$semaphore` | `GCD\Semaphore` | 要获取的 semaphore |
| `$timeout` | `float` | 秒数：< 0 永久等待（默认）；0 非阻塞探测；> 0 最多等待该秒数 |

**返回值** —— 获取成功（计数减一）返回 `true`；超时返回 `false`。

**错误与异常** —— 非 Semaphore 抛 `TypeError`（"expected GCD\Semaphore"）。

**备注** —— 计数为零时阻塞。受保护工作结束后务必配对 `semaphore_signal`。

**示例** —— 见 [`semaphore_create`](#semaphore_create)。

## semaphore_signal

**签名**

```text
GCD\semaphore_signal(GCD\Semaphore $semaphore): void
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$semaphore` | `GCD\Semaphore` | 要释放的 semaphore |

**返回值** —— 无。

**错误与异常** —— 非 Semaphore 抛 `TypeError`（"expected GCD\Semaphore"）。

**备注** —— 计数加一并唤醒一个等待者。

**示例** —— 见 [`semaphore_create`](#semaphore_create)。