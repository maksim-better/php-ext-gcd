--TEST--
GCD\Map set/get/delete under concurrency (T035 / US2)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$m = new GCD\Map("results");
$pool = GCD\queue_create("w", 4);
for ($i = 0; $i < 8; $i++) {
    GCD\async($pool, function ($mn, $n) {
        $m = GCD\Map::open($mn);
        $m->set("k$n", $n * 10);
    }, ["results", $i]);
}
usleep(200000);
echo $m->count() . "\n";
echo $m->get("k3") . "\n";
echo ($m->has("k7") ? "yes" : "no") . "\n";
echo ($m->delete("k7") ? "del-ok" : "del-bad") . "\n";
echo ($m->has("k7") ? "still" : "gone") . "\n";
?>
--EXPECT--
8
30
yes
del-ok
gone