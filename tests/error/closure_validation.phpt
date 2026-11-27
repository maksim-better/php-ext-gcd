--TEST--
GCD rejects non-Closure and internal-function closures safely (review #11)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
try { GCD\set_error_handler(new stdClass()); echo "no-throw\n"; }
catch (TypeError $e) { echo "rejected\n"; }
try { GCD\async(GCD\queue_create("q"), Closure::fromCallable('strlen'), []); echo "no-throw\n"; }
catch (TypeError $e) { echo "rejected-internal\n"; }
?>
--EXPECT--
rejected
rejected-internal