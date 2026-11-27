# 共享容器

[English](../../en/api/containers.md) | [中文](../../zh/api/containers.md)

供任务间共享状态的线程安全容器。所有操作都可从任意线程调用：`Atomic` 用无锁原子操作；`Map`、`Set`、`ConcurrentQueue` 用内部互斥锁。

**命名与共享。** 每个类各有自己的进程级注册表，进程存续期间持久。`new ClassName(..., $name)`（或 `new GCD\Atomic($value, $name)`）把实例注册到 `$name`；重复注册同名会替换注册表条目，旧句柄仍指向原实例。`ClassName::open($name)` 返回指向**同一底层实例**的新句柄——句柄本身不可序列化，这是 worker 任务访问主线程所建状态的途径。名字未注册时 `open` 抛 `ValueError`。未命名实例只能在能直接传递句柄的地方使用。

## Atomic

共享整数计数器。

| 方法 | 签名 |
|---|---|
| `__construct` | `new GCD\Atomic(int $value = 0, string $name = null)` |
| `open`（静态） | `GCD\Atomic::open(string $name): GCD\Atomic` |
| `inc` | `$a->inc(int $n = 1): int` |
| `dec` | `$a->dec(int $n = 1): int` |
| `get` | `$a->get(): int` |
| `set` | `$a->set(int $value): void` |

### __construct

**签名**

```text
new GCD\Atomic(int $value = 0, string $name = null)
```

**参数** —— 初始值（默认 0）；可选注册名。

**返回值** —— 新建计数器。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。

**备注** —— 命名后注册供 `open` 使用。

**示例**

```php
$a = new GCD\Atomic(10, 'hits');
echo $a->inc(), "\n";                    // 11
echo $a->dec(2), "\n";                   // 9
echo GCD\Atomic::open('hits')->get();    // 9 —— 同一实例
```

### open

**签名**

```text
GCD\Atomic::open(string $name): GCD\Atomic
```

**参数** —— 注册名。

**返回值** —— 命名计数器的句柄。

**错误与异常** —— 从未注册时抛 `ValueError`（"unknown GCD\Atomic name"）。

**备注** —— 静态方法；与同名其他句柄共享同一底层计数器。

**示例** —— 见 [`__construct`](#__construct)。

### inc

**签名**

```text
$a->inc(int $n = 1): int
```

**参数** —— 增量（默认 1）。

**返回值** —— 自增**后**的值。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。

**备注** —— 原子读-改-写；竞争下无丢失更新。

**示例** —— 见 [`__construct`](#__construct)。

### dec

**签名**

```text
$a->dec(int $n = 1): int
```

**参数** —— 减量（默认 1）。

**返回值** —— 自减**后**的值。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。

**备注** —— 原子操作；可为负。

**示例** —— 见 [`__construct`](#__construct)。

### get

**签名**

```text
$a->get(): int
```

**参数** —— 无。

**返回值** —— 当前值。

**错误与异常** —— 无。

**备注** —— 原子加载。

**示例** —— 见 [`__construct`](#__construct)。

### set

**签名**

```text
$a->set(int $value): void
```

**参数** —— 新值。

**返回值** —— 无。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。

**备注** —— 原子存储；无论当前值直接覆盖。

**示例** —— 见 [`__construct`](#__construct)。

## Map

共享字符串键映射。值进出均深拷贝；必须可序列化。

| 方法 | 签名 |
|---|---|
| `__construct` | `new GCD\Map(string $name = null)` |
| `open`（静态） | `GCD\Map::open(string $name): GCD\Map` |
| `set` | `$m->set(string $key, mixed $value): void` |
| `get` | `$m->get(string $key): mixed` |
| `has` | `$m->has(string $key): bool` |
| `delete` | `$m->delete(string $key): bool` |
| `count` | `$m->count(): int` |

### __construct

**签名**

```text
new GCD\Map(string $name = null)
```

**参数** —— 可选注册名。

**返回值** —— 新建空映射。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。

**备注** —— 命名后注册供 `open` 使用。

**示例**

```php
$m = new GCD\Map('cfg');
$m->set('retries', 3);
var_export($m->get('retries')); echo "\n";      // 3
var_export($m->has('retries')); echo "\n";      // true
var_export($m->delete('retries')); echo "\n";   // true
var_export($m->get('retries'));                 // NULL
```

### open

**签名**

```text
GCD\Map::open(string $name): GCD\Map
```

**参数** —— 注册名。

**返回值** —— 命名映射的句柄。

**错误与异常** —— 从未注册时抛 `ValueError`（"unknown GCD\Map name"）。

**备注** —— 与同名其他句柄共享同一底层映射。

**示例** —— 见 [`__construct`](#__construct-1)。

### set

**签名**

```text
$m->set(string $key, mixed $value): void
```

**参数** —— 字符串键；值仅限 null/bool/int/float/string 及它们的数组（对象抛 SerializationException）。

**返回值** —— 无。

**错误与异常** —— 值不可序列化时抛 `GCD\SerializationException`（"value is not serializable"）。

**备注** —— 值被深拷贝；之后修改原值不影响存储副本。覆盖已有键。

**示例**

```php
$m = new GCD\Map('bad');
try {
    $m->set('fn', function () {});
} catch (GCD\SerializationException $e) {
    echo "rejected\n";
}
```

### get

**签名**

```text
$m->get(string $key): mixed
```

**参数** —— 字符串键。

**返回值** —— 存储值的副本；不存在时 `null`。

**错误与异常** —— 无。

**备注** —— 修改返回副本不影响映射。

**示例** —— 见 [`__construct`](#__construct-1)。

### has

**签名**

```text
$m->has(string $key): bool
```

**参数** —— 字符串键。

**返回值** —— 键是否存在。

**错误与异常** —— 无。

**备注** —— 存储的 `null` 也算存在。

**示例** —— 见 [`__construct`](#__construct-1)。

### delete

**签名**

```text
$m->delete(string $key): bool
```

**参数** —— 字符串键。

**返回值** —— 删除了键返回 `true`；本不存在返回 `false`。

**错误与异常** —— 无。

**备注** —— 互斥锁保护。

**示例** —— 见 [`__construct`](#__construct-1)。

### count

**签名**

```text
$m->count(): int
```

**参数** —— 无。

**返回值** —— 键数量。

**错误与异常** —— 无。

**备注** —— 锁内快照。

**示例** —— 见 [`__construct`](#__construct-1)。

## Set

共享集合。成员以**序列化形式**标识（PHP 序列化，可序列化对象可接受）。**闭包与资源不支持，抛 `GCD\SerializationException`**——`add`、`contains`、`remove` 都会序列化值。

| 方法 | 签名 |
|---|---|
| `__construct` | `new GCD\Set(string $name = null)` |
| `open`（静态） | `GCD\Set::open(string $name): GCD\Set` |
| `add` | `$s->add(mixed $value): bool` |
| `contains` | `$s->contains(mixed $value): bool` |
| `remove` | `$s->remove(mixed $value): bool` |
| `count` | `$s->count(): int` |

### __construct

**签名**

```text
new GCD\Set(string $name = null)
```

**参数** —— 可选注册名。

**返回值** —— 新建空集合。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。

**备注** —— 命名后注册供 `open` 使用。

**示例**

```php
$s = new GCD\Set('seen');
var_export($s->add('x')); echo "\n";       // true —— 新增
var_export($s->add('x')); echo "\n";       // false —— 重复
var_export($s->contains('x')); echo "\n";  // true
echo $s->count();                          // 1
```

### open

**签名**

```text
GCD\Set::open(string $name): GCD\Set
```

**参数** —— 注册名。

**返回值** —— 命名集合的句柄。

**错误与异常** —— 从未注册时抛 `ValueError`（"unknown GCD\Set name"）。

**备注** —— 与同名其他句柄共享同一底层集合。

**示例** —— 见 [`__construct`](#__construct-2)。

### add

**签名**

```text
$s->add(mixed $value): bool
```

**参数** —— 要插入的值（可序列化）。

**返回值** —— 新增返回 `true`；已存在返回 `false`。

**错误与异常** —— 不支持的值（闭包、资源）抛 `GCD\SerializationException`；值请用标量、数组与可序列化对象。

**备注** —— 去重键是序列化值；并发 add 安全。

**示例** —— 见 [`__construct`](#__construct-2)。

### contains

**签名**

```text
$s->contains(mixed $value): bool
```

**参数** —— 要查找的值。

**返回值** —— 是否存在相等成员。

**错误与异常** —— 不支持的值（闭包、资源）抛 `GCD\SerializationException`。

**备注** —— 比较序列化形式。

**示例** —— 见 [`__construct`](#__construct-2)。

### remove

**签名**

```text
$s->remove(mixed $value): bool
```

**参数** —— 要删除的值。

**返回值** —— 删除返回 `true`；不存在返回 `false`。

**错误与异常** —— 不支持的值（闭包、资源）抛 `GCD\SerializationException`。

**备注** —— 互斥锁保护。

**示例** —— 见 [`__construct`](#__construct-2)。

### count

**签名**

```text
$s->count(): int
```

**参数** —— 无。

**返回值** —— 成员数量。

**错误与异常** —— 无。

**备注** —— 锁内快照。

**示例** —— 见 [`__construct`](#__construct-2)。

## ConcurrentQueue

共享多生产者/多消费者 FIFO 队列。值深拷贝、必须可序列化。

| 方法 | 签名 |
|---|---|
| `__construct` | `new GCD\ConcurrentQueue(string $name = null)` |
| `open`（静态） | `GCD\ConcurrentQueue::open(string $name): GCD\ConcurrentQueue` |
| `push` | `$q->push(mixed $value): void` |
| `pop` | `$q->pop(float $timeout = -1): mixed` |
| `tryPop` | `$q->tryPop(): mixed` |
| `count` | `$q->count(): int` |

### __construct

**签名**

```text
new GCD\ConcurrentQueue(string $name = null)
```

**参数** —— 可选注册名。

**返回值** —— 新建空队列。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。

**备注** —— 命名后注册供 `open` 使用。

**示例**

```php
$q = new GCD\ConcurrentQueue('jobs');
$q->push('a');
$q->push('b');
echo $q->pop(), "\n";        // a —— FIFO
echo $q->tryPop(), "\n";     // b
var_export($q->tryPop());    // NULL —— 空
```

### open

**签名**

```text
GCD\ConcurrentQueue::open(string $name): GCD\ConcurrentQueue
```

**参数** —— 注册名。

**返回值** —— 命名队列的句柄。

**错误与异常** —— 从未注册时抛 `ValueError`（"unknown GCD\ConcurrentQueue name"）。

**备注** —— 与同名其他句柄共享同一底层队列。

**示例** —— 见 [`__construct`](#__construct-3)。

### push

**签名**

```text
$q->push(mixed $value): void
```

**参数** —— 要入队的值：null/bool/int/float/string 及它们的数组（对象抛 SerializationException）。

**返回值** —— 无。

**错误与异常** —— 值不可序列化时抛 `GCD\SerializationException`（"value is not serializable"）。

**备注** —— 追加到队尾并唤醒一个阻塞的 `pop`。

**示例** —— 见 [`__construct`](#__construct-3)。

### pop

**签名**

```text
$q->pop(float $timeout = -1): mixed
```

**参数** —— 秒数：< 0 永久等待（默认）；0 非阻塞探测；> 0 最多等待该秒数。

**返回值** —— 队头值（副本）；超时/空探测返回 `null`。

**错误与异常** —— 无。

**备注** —— 空时阻塞。存储的 `null` 与超时无法区分——避免 push `null`。

**示例** —— 见 [`__construct`](#__construct-3)。

### tryPop

**签名**

```text
$q->tryPop(): mixed
```

**参数** —— 无。

**返回值** —— 队头值；空则 `null`。从不阻塞。

**错误与异常** —— 无。

**备注** —— 等价于 `pop(0.0)`。

**示例** —— 见 [`__construct`](#__construct-3)。

### count

**签名**

```text
$q->count(): int
```

**参数** —— 无。

**返回值** —— 排队值数量。

**错误与异常** —— 无。

**备注** —— 锁内快照。

**示例** —— 见 [`__construct`](#__construct-3)。