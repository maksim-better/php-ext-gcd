--TEST--
GCD queue target is retained: QoS chain valid after unsetting target variable (D6)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$p = GCD\queue_create('p', 2);
GCD\queue_set_qos($p, GCD\QOS_UTILITY);
$c = GCD\queue_create('c', 1);
GCD\queue_set_target($c, $p);
unset($p);
echo GCD\queue_get_qos($c);
?>
--EXPECT--
1