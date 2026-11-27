--TEST--
GCD drain waits for pending async tasks (T025 / US1)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create("w", 4);
$c = new GCD\Atomic(0, "mc");
for ($i = 0; $i < 8; $i++) {
    GCD\async($q, function ($n) {
        GCD\Atomic::open($n)->inc();
        usleep(1000);
    }, ["mc"]);
}
GCD\drain(5.0);
echo $c->get();
?>
--EXPECT--
8