--TEST--
GCD current_queue returns the queue inside concurrent-queue tasks (S2)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$q = GCD\queue_create('c', 2);
$out = tempnam(sys_get_temp_dir(), 'cq');
GCD\async($q, function ($f) { file_put_contents($f, GCD\current_queue() === null ? 'null' : 'set'); }, [$out]);
GCD\drain();
echo file_get_contents($out);
unlink($out);
?>
--EXPECT--
set