--TEST--
GCD group_notify fires when group completes (US3 tail)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$g = GCD\group_create();
$q = GCD\queue_create("w", 2);
$done = new GCD\Atomic(0, "gn");
GCD\group_notify($g, $q, function () {
    GCD\Atomic::open("gn")->inc();
});
GCD\group_async($g, $q, function () {}, []);
GCD\group_wait($g);
GCD\drain(1.0);
echo $done->get();
?>
--EXPECT--
1