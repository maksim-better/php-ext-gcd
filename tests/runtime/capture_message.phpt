--TEST--
GCD CaptureException message no longer mentions try-catch (D10)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create('w', 1);
try {
    GCD\async($q, function () { static $x = 0; });
} catch (GCD\CaptureException $e) {
    echo strpos($e->getMessage(), 'try-catch') === false ? 'no-trycatch' : 'stale';
}
?>
--EXPECT--
no-trycatch