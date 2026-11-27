--TEST--
GCD once executes initializer exactly once across tasks (T047 / US3)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$count = new GCD\Atomic(0, "once");
$q = GCD\queue_create("w", 8);
for ($i = 0; $i < 8; $i++) {
    GCD\async($q, function () {
        GCD\once("init", function () {
            GCD\Atomic::open("once")->inc();
        });
    }, []);
}
usleep(200000);
echo $count->get();
?>
--EXPECT--
1