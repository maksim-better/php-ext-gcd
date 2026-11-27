--TEST--
GCD data source coalesces merged data (US4 tail)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create("w", 2);
$sum = new GCD\Atomic(0, "ds");
$src = GCD\source_create(GCD\SOURCE_DATA_ADD, 0, 0, $q);
GCD\source_set_event_handler($src, function ($d) {
    GCD\Atomic::open("ds")->inc($d);
});
GCD\source_resume($src);
GCD\source_merge_data($src, 5);
GCD\source_merge_data($src, 7);
usleep(100000);
GCD\source_cancel($src);
GCD\drain(1.0);
echo $sum->get();
?>
--EXPECT--
12