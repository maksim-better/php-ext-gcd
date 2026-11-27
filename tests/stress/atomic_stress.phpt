--TEST--
GCD\Atomic contention stress: 64 tasks x 10000 inc, no lost updates (T037 / US2)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$a = new GCD\Atomic(0, "stress");
$q = GCD\queue_create("s", 64);
for ($i = 0; $i < 64; $i++) {
    GCD\async($q, function ($n) {
        $a = GCD\Atomic::open($n);
        for ($j = 0; $j < 10000; $j++) $a->inc();
    }, ["stress"]);
}
usleep(1500000);
echo $a->get();
?>
--EXPECT--
640000