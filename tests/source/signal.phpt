--TEST--
GCD signal source fires on signal delivery (T058 / US4)
--SKIPIF--
<?php if (!extension_loaded("gcd") || !function_exists("posix_kill")) die("skip"); ?>
--FILE--
<?php
$q = GCD\queue_create("w", 2);
$c = new GCD\Atomic(0, "sigc");
$src = GCD\source_create(GCD\SOURCE_SIGNAL, SIGUSR1, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open("sigc")->inc();
});
GCD\source_resume($src);
usleep(50000);
posix_kill(getmypid(), SIGUSR1);
usleep(50000);
posix_kill(getmypid(), SIGUSR1);
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $c->get();
?>
--EXPECT--
2