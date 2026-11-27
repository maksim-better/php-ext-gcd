#!/usr/bin/env bash
# 在 macOS 上从源码构建 ZTS PHP 8.4（CI 用；本地推荐 docker）
set -euo pipefail
PREFIX="$HOME/php-zts"
brew install autoconf bison re2c pkg-config openssl@3 curl sqlite zlib || true
export PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig:$(brew --prefix curl)/lib/pkgconfig:$(brew --prefix zlib)/lib/pkgconfig"
if [ ! -d /tmp/php-src ]; then
  git clone --depth 1 --branch PHP-8.4 https://github.com/php/php-src.git /tmp/php-src
fi
cd /tmp/php-src
./buildconf --force
./configure --prefix="$PREFIX" \
  --enable-zts --disable-all --enable-cli --enable-embed \
  --enable-curl --with-curl --with-openssl \
  --enable-pcntl --enable-posix --enable-sockets \
  --enable-filter --enable-tokenizer --enable-ctype --without-pear
make -j"$(sysctl -n hw.ncpu)"
make install
"$PREFIX/bin/php" -i | grep 'Thread Safety'