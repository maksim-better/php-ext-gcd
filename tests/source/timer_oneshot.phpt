--TEST--
GCD timer with omitted interval fires exactly once (D9)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create('w', 1);
$c = new GCD\Atomic(0, 't9');
$src = GCD\source_create(GCD\SOURCE_TIMER, 0, 0, $q);
GCD\source_set_event_handler($src, function () { GCD\Atomic::open('t9')->inc(); });
GCD\source_set_timer($src, 0.01);
GCD\source_resume($src);
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $c->get();
?>
--EXPECT--
1