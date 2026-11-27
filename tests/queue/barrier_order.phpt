--TEST--
GCD barrier orders later submissions: task submitted after barrier never runs first (S1)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$bad = 0;
for ($r = 0; $r < 20; $r++) {
    $q = GCD\queue_create('b', 4);
    $order = new GCD\ConcurrentQueue('ord');
    GCD\async($q, function () { usleep(20000); });
    GCD\barrier_async($q, function () { GCD\ConcurrentQueue::open('ord')->push('barrier'); });
    GCD\async($q, function () { GCD\ConcurrentQueue::open('ord')->push('later'); });
    GCD\drain();
    if ($order->tryPop() !== 'barrier') $bad++;
}
echo $bad;
?>
--EXPECT--
0