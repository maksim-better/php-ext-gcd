#!/usr/bin/env bash
# S2: 启动本地 PHP server，编译运行 curl 多线程 spike
set -euo pipefail
cd "$(dirname "$0")/.."
docker run --rm -v "$PWD":/ext -w /ext ext-gcd-zts-php:8.4 bash -c '
  mkdir -p /tmp/www && printf "<?php echo \"hello\";" > /tmp/www/index.php
  php -S 127.0.0.1:8099 -t /tmp/www >/tmp/server.log 2>&1 &
  SRV=$!
  sleep 1
  cc spikes/s2_curl_threads.c $(php-config --includes) -L/usr/local/lib -Wl,-rpath,/usr/local/lib -lphp $(php-config --libs) -o /tmp/s2 && /tmp/s2
  RC=$?
  kill $SRV 2>/dev/null
  exit $RC
'