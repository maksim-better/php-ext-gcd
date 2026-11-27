--TEST--
GCD\Atomic counter: named sharing + no lost updates under concurrency (T034 / US2)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$c = new GCD\Atomic(0, "cnt");
$q = GCD\queue_create("w", 4);
for ($i = 0; $i < 8; $i++) {
    GCD\async($q, function ($name) {
        $a = GCD\Atomic::open($name);
        for ($j = 0; $j < 1000; $j++) $a->inc();
    }, ["cnt"]);
}
usleep(200000);
echo $c->get();
?>
--EXPECT--
8000