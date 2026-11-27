--TEST--
GCD error handler receives real message for Error-derived exceptions (D7)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$m = new GCD\Map('msg');
GCD\set_error_handler(function ($msg, $cls) { GCD\Map::open('msg')->set($cls, $msg); });
$q = GCD\queue_create('w', 1);
GCD\sync($q, function () { throw new Error('hello-error'); });
GCD\drain();
echo $m->get('Error');
?>
--EXPECT--
hello-error