--TEST--
GCD\Set dedup under concurrency (T033 / US2)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$s = new GCD\Set("seen");
$added = new GCD\Atomic(0, "added");
$pool = GCD\queue_create("w", 4);
for ($i = 0; $i < 8; $i++) {
    GCD\async($pool, function ($sn, $an) {
        $s = GCD\Set::open($sn);
        $a = GCD\Atomic::open($an);
        for ($j = 0; $j < 100; $j++) if ($s->add($j)) $a->inc();
    }, ["seen", "added"]);
}
usleep(300000);
echo $added->get() . "\n";
echo $s->count() . "\n";
echo ($s->contains(50) ? "yes" : "no") . "\n";
?>
--EXPECT--
100
100
yes