#!/usr/bin/env bash
# 在 ZTS 镜像内编译并运行一个 spike（链接 embed SAPI）
# 用法: spikes/run-spike.sh spikes/s1_zts_boot.c
set -euo pipefail
cd "$(dirname "$0")/.."
SPIKE="${1:-spikes/s1_zts_boot.c}"
docker run --rm -v "$PWD":/ext -w /ext ext-gcd-zts-php:8.4 \
  bash -c "cc $SPIKE \$(php-config --includes) -L/usr/local/lib -Wl,-rpath,/usr/local/lib -lphp \$(php-config --libs) -o /tmp/spike && /tmp/spike"