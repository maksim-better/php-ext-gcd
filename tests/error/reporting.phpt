--TEST--
GCD global error handler reports task exceptions (T016/T088 / US3)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$errors = new GCD\Atomic(0, "err");
GCD\set_error_handler(function ($msg, $cls) {
    GCD\Atomic::open("err")->inc();
});
$q = GCD\queue_create("w", 4);
for ($i = 0; $i < 5; $i++) {
    GCD\async($q, function () { throw new RuntimeException("boom"); }, []);
}
usleep(100000);
echo $errors->get();
?>
--EXPECT--
5