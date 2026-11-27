--TEST--
GCD sync to current serial queue throws DeadlockException (T024 / US1)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create("s");
$dl = new GCD\Atomic(0, "dl");
GCD\set_error_handler(function ($msg, $cls) {
    if ($cls === "GCD\\DeadlockException") GCD\Atomic::open("dl")->inc();
});
GCD\sync($q, function () {
    GCD\sync(GCD\current_queue(), function () {});
});
usleep(50000);
echo $dl->get();
?>
--EXPECT--
1