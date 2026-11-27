--TEST--
GCD group_notify retains its queue: temporary queue handle survives firing (lifetime)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$g = GCD\group_create();
GCD\group_notify($g, GCD\queue_create('n', 1), function () {});
GCD\group_async($g, GCD\queue_create('w', 1), function () {});
GCD\group_wait($g);
GCD\drain(1.0);
echo "ok";
?>
--EXPECT--
ok