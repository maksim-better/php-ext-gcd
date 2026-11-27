#!/usr/bin/env bash
# 在 ZTS PHP 镜像内运行命令，仓库挂载到 /ext（用于构建/测试扩展）
# 用法: docker/dev.sh bash -c "phpize && ./configure --enable-gcd && make"
set -euo pipefail
cd "$(dirname "$0")/.."
docker run --rm -v "$PWD":/ext -w /ext ext-gcd-zts-php:8.4 "$@"