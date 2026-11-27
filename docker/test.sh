#!/usr/bin/env bash
# 在 ZTS 镜像内运行 PHPT 测试（T007）
# 用法: docker/test.sh [tests/子目录]
set -euo pipefail
cd "$(dirname "$0")/.."
TESTS="${1:-tests/}"
docker run --rm -v "$PWD":/ext -w /ext ext-gcd-zts-php:8.4 \
  bash -c "phpize >/dev/null 2>&1; ./configure --enable-gcd >/dev/null 2>&1; make -j\$(nproc) >/dev/null 2>&1; php run-tests.php -q $TESTS"