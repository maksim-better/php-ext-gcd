--TEST--
GCD async rejects non-copyable argv object with SerializationException, no crash (D1)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create('w', 1);
try {
    GCD\async($q, function () {}, [new stdClass]);
    echo 'accepted';
} catch (GCD\SerializationException $e) {
    echo 'rejected';
}
?>
--EXPECT--
rejected