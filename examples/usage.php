<?php
// examples/usage.php — 当前已实现的 GCD API 用法演示
$dir = sys_get_temp_dir() . '/gcd-demo-' . getmypid();
mkdir($dir);

// 1) 并发队列（宽度 3）：并行消费任务
$workers = GCD\queue_create("workers", 3);
for ($i = 0; $i < 6; $i++) {
    // 闭包不能 use 捕获外部变量；数据通过 argv 传入
    GCD\async($workers, function ($dir, $n) {
        usleep(10000);                       // 模拟 I/O
        file_put_contents("$dir/task-$n.txt", "result-$n");
    }, [$dir, $i]);
}

// 2) 串行队列（宽度 1）：严格 FIFO 保序
$serial = GCD\queue_create("serial", 1);
foreach (['a', 'b', 'c'] as $s) {
    GCD\async($serial, function ($dir, $s) {
        file_put_contents("$dir/order.txt", $s, FILE_APPEND);
    }, [$dir, $s]);
}
// 串行队列上的 sync 任务充当屏障：返回时它之前的串行任务必已完成
GCD\sync($serial, function () {});
echo "串行顺序: " . file_get_contents("$dir/order.txt") . "\n";

// 3) 同步投递：阻塞调用者直到任务完成
GCD\sync($workers, function ($dir) {
    file_put_contents("$dir/sync.txt", "sync-done");
}, [$dir]);
echo "同步任务: " . file_get_contents("$dir/sync.txt") . "\n";

// 等待并发任务完成（当前尚无 group/barrier，简单等待）
usleep(100000);
echo "并发完成: " . count(glob("$dir/task-*.txt")) . "/6\n";

array_map('unlink', glob("$dir/*"));
rmdir($dir);