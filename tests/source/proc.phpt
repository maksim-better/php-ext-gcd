--TEST--
GCD proc source fires on child exit (US4 tail)
--SKIPIF--
<?php if (!extension_loaded("gcd") || !function_exists("pcntl_fork")) die("skip"); ?>
--FILE--
<?php
$pid = pcntl_fork();
if ($pid === 0) { usleep(50000); exit(0); }
$q = GCD\queue_create("w", 2);
$c = new GCD\Atomic(0, "pc");
$src = GCD\source_create(GCD\SOURCE_PROC, $pid, 0, $q);
GCD\source_set_event_handler($src, function () {
    GCD\Atomic::open("pc")->inc();
});
GCD\source_resume($src);
usleep(200000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $c->get();
?>
--EXPECT--
1