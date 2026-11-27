--TEST--
GCD Set rejects closures and resources with SerializationException, no crash (D2)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$s = new GCD\Set('s');
try { $s->add(function () {}); echo 'accepted'; } catch (GCD\SerializationException $e) { echo 'rejected'; }
echo '-';
$fp = fopen('php://memory', 'r');
try { $s->add($fp); echo 'accepted'; } catch (GCD\SerializationException $e) { echo 'rejected'; }
?>
--EXPECT--
rejected-rejected