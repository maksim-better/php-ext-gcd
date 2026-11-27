--TEST--
GCD\ConcurrentQueue MPMC + blocking pop, exactly-once (T032 / US2)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = new GCD\ConcurrentQueue("fq");
$sum = new GCD\Atomic(0, "sum");
$pool = GCD\queue_create("w", 2);
GCD\async($pool, function ($qn) {
    $q = GCD\ConcurrentQueue::open($qn);
    for ($i = 1; $i <= 100; $i++) $q->push($i);
}, ["fq"]);
GCD\async($pool, function ($qn, $sn) {
    $q = GCD\ConcurrentQueue::open($qn);
    $s = GCD\Atomic::open($sn);
    for ($i = 0; $i < 100; $i++) {
        $v = $q->pop(5.0);
        $s->inc($v);
    }
}, ["fq", "sum"]);
usleep(300000);
echo $sum->get() . "\n";
echo ($q->tryPop() === null ? "empty-ok" : "empty-bad") . "\n";
?>
--EXPECT--
5050
empty-ok