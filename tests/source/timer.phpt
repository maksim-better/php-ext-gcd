--TEST--
GCD timer source fires periodically (T057 / US4)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create("w", 2);
$c = new GCD\Atomic(0, "tc");
$src = GCD\source_create(GCD\SOURCE_TIMER, 0, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open("tc")->inc();
});
GCD\source_set_timer($src, 0.02, 0.02, 0);
GCD\source_resume($src);
usleep(150000);
GCD\source_cancel($src);
GCD\drain(1.0);
$n = $c->get();
echo ($n >= 3 && $n <= 12) ? "ok" : "bad:$n";
?>
--EXPECT--
ok