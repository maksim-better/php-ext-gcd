<?php
// examples/crawler.php — 用当前 GCD API 模拟爬虫场景：
// frontier 队列 + 去重集合 + 原子计数 + 信号量限流 + group 批量等待
$frontier = new GCD\ConcurrentQueue("f");
$seen     = new GCD\Set("s");
$count    = new GCD\Atomic(0, "c");
$sem      = GCD\semaphore_create(3, "r");   // 最多 3 个并发"请求"
$workers  = GCD\queue_create("w", 4);

// 投递 20 个 URL + 2 个重复（应被去重）
for ($i = 0; $i < 20; $i++) $frontier->push("url$i");
$frontier->push("url0");
$frontier->push("url1");

$g = GCD\group_create();
for ($i = 0; $i < 4; $i++) {
    GCD\group_async($g, $workers, function () {
        $f = GCD\ConcurrentQueue::open("f");
        $s = GCD\Set::open("s");
        $c = GCD\Atomic::open("c");
        $sem = GCD\semaphore_open("r");
        while (($u = $f->tryPop()) !== null) {
            if (!$s->add($u)) continue;        // 去重
            GCD\semaphore_wait($sem);          // 限流
            usleep(500);                       // 模拟 HTTP 请求
            $c->inc();                         // 计数
            GCD\semaphore_signal($sem);
        }
    }, []);
}
GCD\group_wait($g, 10.0);                      // 等整批完成（无需 usleep 猜）
echo "处理: " . $count->get() . "\n";
echo "去重: " . $seen->count() . "\n";