#!/usr/bin/env bash
# 在 TSan PHP 镜像内构建扩展并跑并发测试，过滤涉及扩展代码(src/)的数据竞争
set -euo pipefail
cd "$(dirname "$0")/.."
docker run --rm --privileged --security-opt seccomp=unconfined -v "$PWD":/ext -w /ext ext-gcd-tsan:8.4 bash -c '
  phpize >/dev/null 2>&1
  ./configure --enable-gcd CFLAGS="-fsanitize=thread -fno-omit-frame-pointer -g -O1" LDFLAGS="-fsanitize=thread" >/dev/null 2>&1
  make clean >/dev/null 2>&1; make -j"$(nproc)" >/dev/null 2>&1
  export TSAN_OPTIONS="halt_on_error=0 report_thread_leaks=0 history_size=7"
  echo "=== load ==="; php -d extension=modules/gcd.so -r "echo \"tsan-load-ok\n\";" 2>/dev/null
  echo "=== run concurrency tests ==="
  TEST_PHP_ARGS="-d extension=modules/gcd.so" php run-tests.php -q tests/stress tests/queue tests/sync tests/container 2>&1 | grep -aE "Tests passed|Tests failed" | sed "s/\x1b\[[0-9;]*m//g"
  echo "=== ThreadSanitizer warnings total ==="
  grep -rl "ThreadSanitizer" tests/ 2>/dev/null | wc -l
  echo "=== warnings involving extension src/ ==="
  grep -rl "ThreadSanitizer" tests/ 2>/dev/null | while read f; do
    if grep -q "/ext/src/" "$f"; then echo "EXT-RACE: $f"; fi
  done
'