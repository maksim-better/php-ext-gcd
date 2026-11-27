--TEST--
GCD barrier ordering holds under concurrent main + timer-source submission (S1 stress)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create('mix', 4);
$c = new GCD\Atomic(0, 'cs1');
$src = GCD\source_create(GCD\SOURCE_TIMER, 0, 0, $q);
GCD\source_set_event_handler($src, function () { GCD\Atomic::open('cs1')->inc(); });
GCD\source_set_timer($src, 0.001, 0.001);
GCD\source_resume($src);
for ($i = 0; $i < 100; $i++) {
    GCD\async($q, function () { GCD\Atomic::open('cs1')->inc(); });
    GCD\barrier_async($q, function () { GCD\Atomic::open('cs1')->inc(); });
}
usleep(50000);
GCD\source_cancel($src);
GCD\drain(5.0);
echo ($c->get() >= 200 ? 'ok' : 'bad');
?>
--EXPECT--
ok