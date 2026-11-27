# 数据缓冲

[English](../../en/guide/06-data.md) | [中文](../../zh/guide/06-data.md)

`GCD\Data` 是不可变、二进制安全的字节缓冲。Data 对象只存在于创建它的运行时，无法传给任务；跨线程移动字节请传字符串（经 `$argv`、Map 或 ConcurrentQueue），在任务内调 `data_create`。

## 创建与读取

[`data_create`](../api/data.md#data_create) 拷入字符串；[`data_create_map`](../api/data.md#data_create_map) 拷出字节；`size()` 报告长度：

```php
$d = GCD\data_create('hello');
echo GCD\data_create_map($d), "\n";
echo $d->size();   // 5
```

## 拼接

[`data_concat`](../api/data.md#data_concat) 接受变参，返回新缓冲——不修改任何输入：

```php
$d = GCD\data_concat(GCD\data_create('hello'), GCD\data_create(' '), GCD\data_create('world'));
echo GCD\data_create_map($d);   // hello world
```

## 子区间

[`data_subrange`](../api/data.md#data_subrange) 拷贝切片；偏移与长度钳位而非抛异常：

```php
$d = GCD\data_create('hello world');
echo GCD\data_create_map(GCD\data_subrange($d, 6, 5)), "\n";   // world
echo GCD\data_create_map(GCD\data_subrange($d, 6, 99));         // world —— 钳位
```

## 配合事件源的 fd 读写

v1 没有 `dispatch_io` 通道 API。文件描述符 I/O 由两块拼成：

1. [READ/WRITE 源](05-sources.md#文件描述符)：fd 就绪期间持续触发，
2. 数据缓冲：存放读写的字节。

```text
$src = GCD\source_create(GCD\SOURCE_READ, $fd, 0, $queue);
GCD\source_set_event_handler($src, function () {
    // read($fd) 得到字符串，再 GCD\data_create(...) 缓冲
});
GCD\source_resume($src);
```

处理器是普通任务，累积块通过[共享容器](07-containers.md)或文件。READ/WRITE 的 handle 是原生整型 fd（见[源参考](../api/source.md#源类型常量)）。