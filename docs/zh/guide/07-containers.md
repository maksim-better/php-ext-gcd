# 共享容器

[English](../../en/guide/07-containers.md) | [中文](../../zh/guide/07-containers.md)

普通 PHP 变量是每运行时各自的：任务无法共享它们。四个容器是跨线程工具箱。

## 命名与共享

句柄不可序列化，任务无法经 `$argv` 接收容器。做法是在主线程给容器**命名**，任务内按名 **open**——每次 open 返回同一底层实例：

```php
$c = new GCD\Atomic(0, 'shared');
$q = GCD\queue_create('w', 4);
$g = GCD\group_create();
for ($i = 0; $i < 8; $i++) {
    GCD\group_async($g, $q, function () {
        GCD\Atomic::open('shared')->inc();
    });
}
GCD\group_wait($g);
echo $c->get();   // 8 —— 每个 worker 自增的是同一计数器
```

注册表进程级、持久。完整规则见[容器参考](../api/containers.md)。

## Atomic：无丢失更新的计数

`inc`/`dec` 是原子读-改-写，返回新值。重度竞争也不丢更新：

```php
$c = new GCD\Atomic(0, 'stress');
$q = GCD\queue_create('w', 8);
$g = GCD\group_create();
for ($i = 0; $i < 8; $i++) {
    GCD\group_async($g, $q, function () {
        $a = GCD\Atomic::open('stress');
        for ($j = 0; $j < 1000; $j++) { $a->inc(); }
    });
}
GCD\group_wait($g);
echo $c->get();   // 8000
```

## Map 与 Set

`Map` 以字符串键存可序列化值；`Set` 按序列化形式去重。均有互斥锁保护：

```php
$s = new GCD\Set('dedup');
$q = GCD\queue_create('w', 4);
$g = GCD\group_create();
for ($i = 0; $i < 8; $i++) {
    GCD\group_async($g, $q, function (int $n) {
        GCD\Set::open('dedup')->add($n % 4);
    }, [$i]);
}
GCD\group_wait($g);
echo $s->count();   // 4 —— 8 个并发 add，4 个不同值
```

不可序列化的值被拒绝：`Map::set` 与 `ConcurrentQueue::push` 抛 `GCD\SerializationException`。

## ConcurrentQueue：生产者–消费者

`push` 追加并唤醒阻塞的 `pop`；`tryPop` 从不阻塞。最小流水线：

```php
$q = new GCD\ConcurrentQueue('pipe');
$wq = GCD\queue_create('workers', 2);
$g = GCD\group_create();
GCD\group_async($g, $wq, function () {
    $pipe = GCD\ConcurrentQueue::open('pipe');
    for ($i = 0; $i < 5; $i++) { $pipe->push($i); }
});
GCD\group_wait($g);
$sum = 0;
while (($v = $q->tryPop()) !== null) { $sum += $v; }
echo $sum;   // 10
```

长驻消费者用 `pop()` 阻塞等待值（常规超时语义）；避免 push `null`——它与超时无法区分。