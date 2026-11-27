--TEST--
GCD dispatch_data create/concat/subrange/map (T071 / US5)
--SKIPIF--
<?php if (!extension_loaded("gcd")) die("skip gcd not loaded"); ?>
--FILE--
<?php
$d = GCD\data_create("hello");
$d2 = GCD\data_concat($d, GCD\data_create(" world"));
echo GCD\data_create_map($d2) . "\n";
$sub = GCD\data_subrange($d2, 6, 5);
echo GCD\data_create_map($sub) . "\n";
echo $d2->size();
?>
--EXPECT--
hello world
world
11