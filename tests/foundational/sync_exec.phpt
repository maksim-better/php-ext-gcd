--TEST--
GCD sync dispatch runs a closure on a worker thread with argv (T019 smoke / US1)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$path = tempnam(sys_get_temp_dir(), "gcd");
$q = GCD\queue_create("test");
GCD\sync($q, function ($p) { file_put_contents($p, "ran"); }, [$path]);
echo file_get_contents($path);
unlink($path);
?>
--EXPECT--
ran