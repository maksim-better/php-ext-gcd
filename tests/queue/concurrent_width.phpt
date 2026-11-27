--TEST--
GCD concurrent queue enforces width limit (T022 / US1)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$path = tempnam(sys_get_temp_dir(), "gcd");
file_put_contents($path, "0 0");
$q = GCD\queue_create("c", 2);
$worker = function ($p) {
    $fp = fopen($p, 'r+'); flock($fp, LOCK_EX);
    $d = explode(' ', stream_get_contents($fp));
    $cur = (int) $d[0] + 1; $max = max((int) $d[1], $cur);
    ftruncate($fp, 0); rewind($fp); fwrite($fp, "$cur $max"); fflush($fp);
    flock($fp, LOCK_UN); fclose($fp);
    usleep(8000);
    $fp = fopen($p, 'r+'); flock($fp, LOCK_EX);
    $d = explode(' ', stream_get_contents($fp));
    $cur = (int) $d[0] - 1;
    ftruncate($fp, 0); rewind($fp); fwrite($fp, "$cur $d[1]"); fflush($fp);
    flock($fp, LOCK_UN); fclose($fp);
};
for ($i = 0; $i < 8; $i++) GCD\async($q, $worker, [$path]);
usleep(300000);
$d = explode(' ', file_get_contents($path));
echo ((int) $d[1] <= 2 && (int) $d[0] == 0) ? "ok" : ("bad max=" . $d[1] . " cur=" . $d[0]);
unlink($path);
?>
--EXPECT--
ok