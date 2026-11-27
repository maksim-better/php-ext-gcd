--TEST--
GCD serial queue preserves FIFO order for async tasks (T021 / US1)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$path = tempnam(sys_get_temp_dir(), "gcd");
file_put_contents($path, "");
$q = GCD\queue_create("serial");
for ($i = 0; $i < 5; $i++) {
    GCD\async($q, function ($p, $n) {
        file_put_contents($p, (string) $n, FILE_APPEND | LOCK_EX);
        usleep(2000);
    }, [$path, $i]);
}
GCD\sync($q, function () { /* serial barrier: runs after all prior tasks */ });
echo file_get_contents($path);
unlink($path);
?>
--EXPECT--
01234