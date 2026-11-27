--TEST--
GCD signal source armed after pool boot does not kill the process (D3)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); if (!function_exists('posix_kill') || !defined('SIGUSR1')) die("skip posix required"); ?>
--FILE--
<?php
$q = GCD\queue_create('boot', 2);
GCD\async($q, function () {});
GCD\drain();
usleep(50000);
$c = new GCD\Atomic(0, 's3');
$src = GCD\source_create(GCD\SOURCE_SIGNAL, SIGUSR1, 0, $q);
GCD\source_set_event_handler($src, function () { GCD\Atomic::open('s3')->inc(); });
GCD\source_resume($src);
usleep(50000);
posix_kill(getmypid(), SIGUSR1);
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $c->get();
?>
--EXPECT--
1