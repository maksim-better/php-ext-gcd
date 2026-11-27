--TEST--
GCD barrier_sync drains in-flight tasks then runs exclusively (T046 / US3)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create("c", 4);
$c = new GCD\Atomic(0, "bc");
for ($i = 0; $i < 8; $i++) {
    GCD\async($q, function ($n) {
        GCD\Atomic::open($n)->inc();
        usleep(2000);
    }, ["bc"]);
}
GCD\barrier_sync($q, function () {});
echo $c->get();
?>
--EXPECT--
8