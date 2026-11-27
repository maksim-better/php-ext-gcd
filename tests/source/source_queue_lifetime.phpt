--TEST--
GCD source retains its queue: temporary queue handle survives firing (lifetime)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$src = GCD\source_create(GCD\SOURCE_TIMER, 0, 0, GCD\queue_create('q', 1));
GCD\source_set_event_handler($src, function () {});
GCD\source_set_timer($src, 0.01);
GCD\source_resume($src);
usleep(50000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo "ok";
?>
--EXPECT--
ok