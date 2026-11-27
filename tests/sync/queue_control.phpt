--TEST--
GCD queue suspend/resume + queue-specific (T048 / US3)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create("s", 1);
$c = new GCD\Atomic(0, "sc");
GCD\queue_suspend($q);
GCD\async($q, function ($n) { GCD\Atomic::open($n)->inc(); }, ["sc"]);
usleep(50000);
echo $c->get() . "\n";
GCD\queue_resume($q);
usleep(50000);
echo $c->get() . "\n";
GCD\queue_set_specific($q, "key", "val");
echo GCD\queue_get_specific($q, "key") . "\n";
?>
--EXPECT--
0
1
val