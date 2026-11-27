#!/usr/bin/env bash
# 在 ASan PHP 镜像内构建扩展并跑测试（内存错误检测）
set -euo pipefail
cd "$(dirname "$0")/.."
docker run --rm -v "$PWD":/ext -w /ext ext-gcd-asan:8.4 bash -c '
  phpize >/dev/null 2>&1
  ./configure --enable-gcd CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" LDFLAGS="-fsanitize=address" >/dev/null 2>&1
  make -j"$(nproc)" >/dev/null 2>&1
  export ASAN_OPTIONS=detect_leaks=0:halt_on_error=0:abort_on_error=0
  echo "=== load check ==="
  php -d extension=modules/gcd.so -r "echo \"asan-load-ok\n\";" 2>&1 | head -20
  echo "=== suite ==="
  TEST_PHP_ARGS="-d extension=modules/gcd.so" php run-tests.php -q tests/ 2>&1 | grep -E "Number of tests|Tests passed|Tests failed|FAIL" | sed "s/\x1b\[[0-9;]*m//g"
'