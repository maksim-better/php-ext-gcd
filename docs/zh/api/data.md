# 数据

[English](../../en/api/data.md) | [中文](../../zh/api/data.md)

## Data 类

`GCD\Data` 是只有一个方法 `size(): int` 的不可变字节缓冲。用 `data_create`、`data_concat`、`data_subrange` 创建；用 `data_create_map` 读回字节。缓冲二进制安全。`GCD\Data` 对象无法跨运行时：经 `$argv` 传递抛 `GCD\SerializationException`，容器也拒绝它。跨线程移动字节请传字符串，在任务内调 `data_create`。

## data_create

**签名**

```text
GCD\data_create(string $bytes): GCD\Data
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$bytes` | `string` | 要拷贝的字节（二进制安全，允许 NUL） |

**返回值** —— 新建的不可变 `GCD\Data`。

**错误与异常** —— 参数类型错误抛标准 `TypeError`。

**备注** —— 字符串被拷贝；之后修改它不影响缓冲。

**示例**

```php
$d = GCD\data_create("a\0b");
echo $d->size();   // 3 —— 二进制安全
```

## data_concat

**签名**

```text
GCD\data_concat(GCD\Data ...$parts): GCD\Data
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `...$parts` | `GCD\Data` | 一个或多个缓冲，按序拼接 |

**返回值** —— 拼接结果的新缓冲。

**错误与异常** —— 不传参数抛标准 `TypeError`。

**备注** —— 非 `GCD\Data` 的参数被静默跳过。

**示例**

```php
$d = GCD\data_concat(GCD\data_create('hello'), GCD\data_create(' '), GCD\data_create('world'));
echo GCD\data_create_map($d);   // hello world
```

## data_subrange

**签名**

```text
GCD\data_subrange(GCD\Data $data, int $offset, int $length): GCD\Data
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$data` | `GCD\Data` | 源缓冲 |
| `$offset` | `int` | 起始偏移；钳位到 [0, size] |
| `$length` | `int` | 字节数；负值或越界钳位为剩余字节 |

**返回值** —— 子区间的新缓冲（副本）。

**错误与异常** —— 非 Data 抛 `TypeError`（"expected GCD\Data"）。

**备注** —— 钳位不抛异常：偏移超过末尾得到空缓冲。

**示例**

```php
$d = GCD\data_create('hello world');
echo GCD\data_create_map(GCD\data_subrange($d, 6, 5)), "\n";   // world
echo GCD\data_create_map(GCD\data_subrange($d, 6, 99));         // world —— 钳位
```

## data_create_map

**签名**

```text
GCD\data_create_map(GCD\Data $data): string
```

**参数**

| 名称 | 类型 | 说明 |
|---|---|---|
| `$data` | `GCD\Data` | 要映射的缓冲 |

**返回值** —— 缓冲字节的 PHP 字符串（二进制安全）。

**错误与异常** —— 非 Data 抛 `TypeError`（"expected GCD\Data"）。

**备注** —— v1 返回整个缓冲的连续副本；没有懒映射回调。

**示例**

```php
$d = GCD\data_create('hello');
echo GCD\data_create_map($d), "\n";
echo $d->size();   // 5
```