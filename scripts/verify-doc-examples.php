<?php
/**
 * Verify that every ```php fenced block in documentation runs cleanly.
 *
 * Usage: php scripts/verify-doc-examples.php <file-or-dir>...
 * Exit:  0 = all examples passed, 1 = any failure, 2 = usage error.
 *
 * Conventions: runnable examples use ```php fences WITHOUT a leading
 * <?php tag (one is prepended); signatures and non-runnable fragments
 * use ```text fences and are ignored.
 */

if ($argc < 2) {
    fwrite(STDERR, "Usage: php verify-doc-examples.php <file-or-dir>...\n");
    exit(2);
}

$files = [];
foreach (array_slice($argv, 1) as $arg) {
    if (is_dir($arg)) {
        $it = new RecursiveIteratorIterator(
            new RecursiveDirectoryIterator($arg, FilesystemIterator::SKIP_DOTS)
        );
        foreach ($it as $f) {
            if ($f->isFile() && strtolower($f->getExtension()) === 'md') {
                $files[] = $f->getPathname();
            }
        }
    } elseif (is_file($arg)) {
        $files[] = $arg;
    } else {
        fwrite(STDERR, "Not a file or directory: $arg\n");
        exit(2);
    }
}
sort($files);

$failures = 0;
$total = 0;
foreach ($files as $file) {
    $lines = file($file, FILE_IGNORE_NEW_LINES);
    $in = false;
    $block = [];
    $n = 0;
    foreach ($lines as $line) {
        if (!$in && preg_match('/^```php\s*$/', $line)) {
            $in = true;
            $block = [];
            continue;
        }
        if ($in && preg_match('/^```\s*$/', $line)) {
            $in = false;
            $n++;
            $total++;
            $tmp = tempnam(sys_get_temp_dir(), 'gcd_doc_');
            file_put_contents($tmp, "<?php\n" . implode("\n", $block) . "\n");
            $out = [];
            $code = 0;
            exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($tmp) . ' 2>&1', $out, $code);
            unlink($tmp);
            $text = implode("\n", $out);
            if ($code !== 0 || strpos($text, 'Fatal error') !== false || strpos($text, 'Parse error') !== false) {
                $failures++;
                echo "FAIL $file:$n (exit $code)\n$text\n";
            }
            continue;
        }
        if ($in) {
            $block[] = $line;
        }
    }
}

echo $failures ? "$failures of $total example(s) failed\n" : "All $total example(s) passed\n";
exit($failures ? 1 : 0);