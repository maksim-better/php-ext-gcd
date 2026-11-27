--TEST--
GCD containers reject non-serializable values (T036 / US2)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = new GCD\ConcurrentQueue("x");
$fp = fopen("php://memory", "r");
try { $q->push($fp); echo "no-throw\n"; }
catch (GCD\SerializationException $e) { echo "queue-rejected\n"; }
fclose($fp);
$m = new GCD\Map("y");
$fp = fopen("php://memory", "r");
try { $m->set("k", $fp); echo "no-throw\n"; }
catch (GCD\SerializationException $e) { echo "map-rejected\n"; }
fclose($fp);
?>
--EXPECT--
queue-rejected
map-rejected