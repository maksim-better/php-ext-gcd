--TEST--
GCD apply parallel iteration, caller waits (T070 / US5)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create("w", 4);
$sum = new GCD\Atomic(0, "ap");
GCD\apply(100, $q, function ($i) {
    GCD\Atomic::open("ap")->inc($i);
});
echo $sum->get();
?>
--EXPECT--
4950