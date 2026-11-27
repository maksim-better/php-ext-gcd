# ext-gcd

A PHP extension providing **GCD-style (Grand Central Dispatch) concurrency** on
**ZTS PHP** using a real OS thread pool, plus thread-safe C-level shared containers.

> Status: early implementation (P1 foundation). See `specs/001-php-gcd-extension/`
> for the spec, plan, research and task breakdown.

## Requirements

- **ZTS PHP 8.4** (pinned target; 8.2/8.3 also intended). NTS / FPM / Windows are
  **not supported** — the extension rejects NTS builds at configure/compile time.
- Linux (epoll) or macOS (kqueue).
- C toolchain: clang/gcc, autoconf, make, phpize.

## Build (recommended: Docker ZTS image)

```bash
bash docker/build.sh                 # builds ext-gcd-zts-php:8.4 (ZTS PHP from source)
docker/dev.sh bash -c "phpize && ./configure --enable-gcd && make"
docker/dev.sh php -d extension=modules/gcd.so -m | grep gcd
docker/test.sh tests/                # run PHPT suite inside the ZTS image
```

## Build (native ZTS PHP)

If you already have a ZTS PHP 8.4 (`php -i | grep 'Thread Safety'` => enabled):

```bash
phpize && ./configure --enable-gcd && make
```

macOS CI builds ZTS PHP via `scripts/build-zts-macos.sh`.

## Design highlights

- Custom pthread thread pool; each worker boots an isolated PHP runtime
  (`ts_resource` + `TSRMLS_CACHE_UPDATE` + `php_request_startup`, one request per thread).
- GCD queue layer (serial/concurrent, drain lock, target queue), async/sync/barrier/
  after/apply, group/semaphore/once, event sources (epoll/kqueue), dispatch_data/io,
  QoS (Linux best-effort).
- Tasks are closures + serializable argv (no captured variables in v1).
- Thread-safe shared containers: queue / set / atomic counter / map.
- Dual main-loop model: `GCD\main()` (callback-driven) + `GCD\drain()` (manual loop).

## License

PHP License 3.01 (see `LICENSE`). Portions inspired by swift-corelibs-libdispatch
(Apache-2.0); see `NOTICE`.