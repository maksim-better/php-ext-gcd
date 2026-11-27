--TEST--
GCD task body can use try/catch (review #9)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create("w", 2);
$c = new GCD\Atomic(0, "tc");
GCD\async($q, function () {
    try {
        throw new Exception("x");
    } catch (Exception $e) {
        GCD\Atomic::open("tc")->inc();
    }
}, []);
GCD\drain(1.0);
echo $c->get();
?>
--EXPECT--
1