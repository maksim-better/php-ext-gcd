--TEST--
GCD signal source armed while a timer source thread runs does not kill the process (D3a)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); if (!function_exists('posix_kill') || !defined('SIGUSR1')) die("skip posix required"); ?>
--FILE--
<?php
$q = GCD\queue_create('w', 1);
$t = GCD\source_create(GCD\SOURCE_TIMER, 0, 0, $q);
GCD\source_set_event_handler($t, function () {});
GCD\source_set_timer($t, 2.0);
GCD\source_resume($t);
usleep(50000);
$c = new GCD\Atomic(0, 's3a');
$s = GCD\source_create(GCD\SOURCE_SIGNAL, SIGUSR1, 0, $q);
GCD\source_set_event_handler($s, function () { GCD\Atomic::open('s3a')->inc(); });
GCD\source_resume($s);
usleep(100000);
posix_kill(getmypid(), SIGUSR1);
usleep(100000);
GCD\source_cancel($s);
GCD\source_cancel($t);
GCD\drain(1.0);
echo $c->get();
?>
--EXPECT--
1