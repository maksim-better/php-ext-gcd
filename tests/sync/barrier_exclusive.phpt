--TEST--
GCD two barriers on a concurrent queue are mutually exclusive (review #4)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create("c", 4);
$active = new GCD\Atomic(0, "ba");
$bad = new GCD\Atomic(0, "bb");
$barrier = function () {
    $cur = GCD\Atomic::open("ba")->inc();
    if ($cur > 1) GCD\Atomic::open("bb")->inc();
    usleep(5000);
    GCD\Atomic::open("ba")->dec();
};
GCD\barrier_async($q, $barrier, []);
GCD\barrier_async($q, $barrier, []);
GCD\drain(1.0);
echo $bad->get();
?>
--EXPECT--
0