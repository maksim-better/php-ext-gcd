#!/usr/bin/env bash
# 构建 ZTS PHP 8.4 基础镜像（T003）
set -euo pipefail
cd "$(dirname "$0")/.."
docker build -f docker/Dockerfile.zts -t ext-gcd-zts-php:8.4 docker/