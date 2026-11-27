--TEST--
GCD group_async + group_wait blocks until batch completes (T044 / US3)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$g = GCD\group_create();
$q = GCD\queue_create("w", 4);
$sum = new GCD\Atomic(0, "gsum");
for ($i = 1; $i <= 10; $i++) {
    GCD\group_async($g, $q, function ($sn, $n) {
        $a = GCD\Atomic::open($sn);
        $a->inc($n);
    }, ["gsum", $i]);
}
GCD\group_wait($g);
echo $sum->get();
?>
--EXPECT--
55