# Quickstart

[English](../../en/guide/01-quickstart.md) | [中文](../../zh/guide/01-quickstart.md)

## Requirements

- **ZTS PHP 8.4–8.6.** 8.6 is verified locally (29/29 tests); 8.4 is the Docker build target; 8.5 is expected compatible but unverified; 8.2/8.3 are not promised.
- Linux or macOS (poll-based event sources). NTS, FPM and Windows are **not supported** — the extension rejects NTS builds at compile time.
- C toolchain: clang/gcc, autoconf, make, phpize.

Check your PHP: `php -i | grep 'Thread Safety'` must say `enabled`.

## Build and install

### Native ZTS PHP

With a ZTS PHP on your PATH (its `phpize` and `php-config` belong to the same installation):

```bash
phpize
./configure --enable-gcd
make
make install
```

### Docker

Build the ZTS PHP 8.4 image and compile inside it:

```bash
bash docker/build.sh
docker/dev.sh bash -c "phpize && ./configure --enable-gcd && make"
docker/test.sh tests/
```

On macOS, `scripts/build-zts-macos.sh` builds ZTS PHP from source (used by CI).

## Enable the extension

After `make install`, add `extension=gcd.so` to your `php.ini` or drop a `gcd.ini` into the scan directory shown by `php --ini`. To try without installing:

```bash
php -d extension=modules/gcd.so -m | grep gcd
```

## Your first concurrent task

Tasks are closures plus an argument array (scalars and arrays only — see [task rules](02-concepts.md#task-rules)). Worker output is unsynchronized, so pass results back through files or [shared containers](07-containers.md):

```php
$q = GCD\queue_create('demo', 2);          // concurrent queue, width 2
$out = tempnam(sys_get_temp_dir(), 'gcd_demo_');
GCD\async($q, function (string $file, string $who) {
    file_put_contents($file, "hello, {$who}\n");
}, [$out, 'world']);
GCD\drain();                               // wait for pending work
echo file_get_contents($out);
unlink($out);
```

Output:

```text
hello, world
```

## Next steps

- [Core concepts](02-concepts.md) — queues, the thread model, and the rules every task must follow
- [Dispatching work](03-dispatch.md) — async/sync, barriers, apply, once
- [API index](../api/README.md)