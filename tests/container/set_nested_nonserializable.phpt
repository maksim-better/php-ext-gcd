--TEST--
GCD Set rejects nested closures and resources with SerializationException (D2 nested)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$s = new GCD\Set('n');
try { $s->add([function(){}]); echo 'accepted'; } catch (GCD\SerializationException $e) { echo 'rejected'; }
echo '-';
$fp = fopen('php://memory', 'r');
try { $s->add(['x'=>$fp]); echo 'accepted'; } catch (GCD\SerializationException $e) { echo 'rejected'; }
?>
--EXPECT--
rejected-rejected