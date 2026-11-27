--TEST--
GCD group reusable after notify; notify fires once; second batch truly waits (D5)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$g = GCD\group_create();
$nq = GCD\queue_create('n', 1);
$wq = GCD\queue_create('w', 2);
$notify = new GCD\Atomic(0, 'n5');
GCD\group_notify($g, $nq, function () { GCD\Atomic::open('n5')->inc(); });
GCD\group_async($g, $wq, function () { usleep(20000); });
GCD\group_wait($g);
GCD\drain();
$done = new GCD\Atomic(0, 'd5');
GCD\group_async($g, $wq, function () { usleep(50000); GCD\Atomic::open('d5')->inc(); });
GCD\group_wait($g);
echo $done->get(), "\n";
GCD\drain();
echo $notify->get();
?>
--EXPECT--
1
1