--TEST--
GCD QoS set/get + target-queue inheritance (T078 / US6)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create("q", 2);
GCD\queue_set_qos($q, GCD\QOS_USER_INITIATED);
$child = GCD\queue_create("c", 1);
GCD\queue_set_target($child, $q);
echo (GCD\queue_get_qos($child) === GCD\QOS_USER_INITIATED ? "inherit-ok" : "bad") . "\n";
echo (GCD\queue_get_qos($q) === GCD\QOS_USER_INITIATED ? "own-ok" : "bad") . "\n";
echo (GCD\qos_class_self() >= 0 ? "self-ok" : "bad");
?>
--EXPECT--
inherit-ok
own-ok
self-ok