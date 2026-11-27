--TEST--
GCD global set_error_handler rejects capturing closure with CaptureException (D8)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$x = 1;
try {
    GCD\set_error_handler(function () use (&$x) {});
    echo 'accepted';
} catch (GCD\CaptureException $e) {
    echo 'rejected';
}
?>
--EXPECT--
rejected