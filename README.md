# ext-gcd

A PHP extension providing **GCD-style (Grand Central Dispatch) concurrency** on
**ZTS PHP** using a real OS thread pool, plus thread-safe C-level shared containers.

> Status: early implementation (P1 foundation). See `specs/001-php-gcd-extension/`
> for the spec, plan, research and task breakdown.

## Requirements

- **ZTS PHP 8.4–8.6.** 8.6 verified locally (29/29 tests); 8.4 is the Docker
  build target; 8.5 expected compatible but unverified; 8.2/8.3 not promised.
  NTS / FPM / Windows are **not supported** — the extension rejects NTS builds
  at configure/compile time.
- Linux or macOS (poll-based event sources).
- C toolchain: clang/gcc, autoconf, make, phpize.

## Build (recommended: Docker ZTS image)

```bash
bash docker/build.sh                 # builds ext-gcd-zts-php:8.4 (ZTS PHP from source)
docker/dev.sh bash -c "phpize && ./configure --enable-gcd && make"
docker/dev.sh php -d extension=modules/gcd.so -m | grep gcd
docker/test.sh tests/                # run PHPT suite inside the ZTS image
```

## Build (native ZTS PHP)

If you already have a ZTS PHP 8.4–8.6 (`php -i | grep 'Thread Safety'` => enabled):

```bash
phpize && ./configure --enable-gcd && make
```

macOS CI builds ZTS PHP via `scripts/build-zts-macos.sh`.

## Design highlights

- Custom pthread thread pool; each worker boots an isolated PHP runtime
  (`ts_resource` + `TSRMLS_CACHE_UPDATE` + `php_request_startup`, one request per thread).
- GCD queue layer (serial/concurrent, drain lock, target queue), async/sync/barrier/
  apply, group/semaphore/once, poll-based event sources, data buffers,
  QoS (recorded only). `after` and dispatch_io channels are planned, not in v1.
- Tasks are closures + serializable argv (no captured variables in v1).
- Thread-safe shared containers: queue / set / atomic counter / map.
- Main loop: `GCD\drain()` waits for pending work; `GCD\main()` is equivalent in v1
  (a never-returning, source-driven loop is planned).

## Documentation

Bilingual user documentation (API reference + task-oriented guides) lives in
[`docs/`](docs/README.md): [English](docs/en/README.md) · [中文](docs/zh/README.md).

## License

PHP License 3.01 (see `LICENSE`). Portions inspired by swift-corelibs-libdispatch
(Apache-2.0); see `NOTICE`.