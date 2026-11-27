--TEST--
GCD containers free array values without leaking (review #1)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = new GCD\ConcurrentQueue("lk");
for ($i = 0; $i < 2000; $i++) { $q->push(["url$i", "p$i"]); $q->pop(0); }
gc_collect_cycles();
$a = memory_get_usage(true);
for ($i = 0; $i < 20000; $i++) { $q->push(["url$i", "p$i", "x$i"]); $q->pop(0); }
gc_collect_cycles();
$b = memory_get_usage(true);
echo (($b - $a) < 1000000 ? "leak-ok" : "LEAK");
?>
--EXPECT--
leak-ok