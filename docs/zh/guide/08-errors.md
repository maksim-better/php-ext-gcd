# 错误处理

[English](../../en/guide/08-errors.md) | [中文](../../zh/guide/08-errors.md)

## 两种失败

- **派发期异常** —— 句柄类型错误、`CaptureException`、`SerializationException`、`sync` 死锁保护。同步抛给调用方；在脚本里用普通 try/catch 捕获。
- **任务内异常** —— 任务运行时抛出的一切。永远不到达调用方；走下面的上报层。

## 上报层级

任务带着异常结束时，扩展依次：

1. 调用**队列级处理器**（`queue_set_error_handler`，若已设置），
2. 否则调用**全局处理器**（`set_error_handler`），
3. 否则**静默丢弃异常**。

处理器接收 `(string $message, string $className)`，在失败任务的 worker 中运行，且抛异常也无用——自身的异常同样被丢弃。结果通过共享容器记录：

```php
$errors = new GCD\Atomic(0, 'err');
GCD\set_error_handler(function (string $msg, string $cls) {
    GCD\Atomic::open('err')->inc();
});
$q = GCD\queue_create('w', 4);
$g = GCD\group_create();
for ($i = 0; $i < 5; $i++) {
    GCD\group_async($g, $q, function () { throw new RuntimeException('boom'); });
}
GCD\group_wait($g);
GCD\drain();
echo $errors->get();   // 5 —— 每次失败都上报
```

## 自定义处理器

队列级处理器仅对该队列覆盖全局处理器：

```php
$global = new GCD\Atomic(0, 'g-err');
$local = new GCD\Atomic(0, 'q-err');
GCD\set_error_handler(function (string $m, string $c) { GCD\Atomic::open('g-err')->inc(); });
$q = GCD\queue_create('w', 1);
GCD\queue_set_error_handler($q, function (string $m, string $c) { GCD\Atomic::open('q-err')->inc(); });
GCD\sync($q, function () { throw new RuntimeException('x'); });
GCD\drain();
echo $global->get(), "\n";   // 0
echo $local->get();          // 1
```

处理器闭包与任务遵守同样的 v1 规则：无捕获、静态变量、try/catch。

## 捕获派发期异常

```php
$q = GCD\queue_create('w', 1);
try {
    GCD\async($q, function () { static $x = 0; });
} catch (GCD\CaptureException $e) {
    echo "capture rejected\n";
}
```

死锁保护在任务内触发，需经处理器观察——[派发指南](03-dispatch.md#async-or-sync)给出完整模式。三个异常类均继承 `Error`；见[错误参考](../api/errors.md)。