--TEST--
GCD notify fires after group unset post-wait, 50 rounds, no UAF (D5 stress)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$ok = 0;
for ($r = 0; $r < 50; $r++) {
    $g = GCD\group_create();
    $c = new GCD\Atomic(0, 'u5');
    GCD\group_notify($g, GCD\queue_create('n', 1), function () { GCD\Atomic::open('u5')->inc(); });
    GCD\group_async($g, GCD\queue_create('w', 1), function () { usleep(5000); });
    GCD\group_wait($g);
    unset($g);
    GCD\drain(2.0);
    if ($c->get() === 1) $ok++;
}
echo $ok;
?>
--EXPECT--
50