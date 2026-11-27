# 快速开始

[English](../../en/guide/01-quickstart.md) | [中文](../../zh/guide/01-quickstart.md)

## 环境要求

- **ZTS PHP 8.4–8.6。** 8.6 本机实测（29/29 通过）；8.4 为 Docker 构建目标；8.5 理论兼容未验证；8.2/8.3 不承诺。
- Linux 或 macOS（基于 poll 的事件源）。NTS、FPM、Windows **不支持**——扩展在编译期拒绝 NTS 构建。
- C 工具链：clang/gcc、autoconf、make、phpize。

检查你的 PHP：`php -i | grep 'Thread Safety'` 必须为 `enabled`。

## 构建与安装

### 原生 ZTS PHP

PATH 上有一个 ZTS PHP（其 `phpize` 与 `php-config` 属于同一安装）：

```bash
phpize
./configure --enable-gcd
make
make install
```

### Docker

构建 ZTS PHP 8.4 镜像并在其中编译：

```bash
bash docker/build.sh
docker/dev.sh bash -c "phpize && ./configure --enable-gcd && make"
docker/test.sh tests/
```

macOS 上 `scripts/build-zts-macos.sh` 可从源码构建 ZTS PHP（CI 使用）。

## 启用扩展

`make install` 之后，把 `extension=gcd.so` 加进 `php.ini`，或在 `php --ini` 显示的扫描目录里放一个 `gcd.ini`。不安装直接试用：

```bash
php -d extension=modules/gcd.so -m | grep gcd
```

## 第一个并发任务

任务 = 闭包 + 参数数组（仅标量与数组——见[任务规则](02-concepts.md#任务规则)）。worker 输出无同步，结果通过文件或[共享容器](07-containers.md)传回：

```php
$q = GCD\queue_create('demo', 2);          // 并发队列，宽度 2
$out = tempnam(sys_get_temp_dir(), 'gcd_demo_');
GCD\async($q, function (string $file, string $who) {
    file_put_contents($file, "hello, {$who}\n");
}, [$out, 'world']);
GCD\drain();                               // 等待未完成的任务
echo file_get_contents($out);
unlink($out);
```

输出：

```text
hello, world
```

## 下一步

- [核心概念](02-concepts.md) —— 队列、线程模型、每个任务必须遵守的规则
- [派发任务](03-dispatch.md) —— async/sync、barrier、apply、once
- [API 索引](../api/README.md)