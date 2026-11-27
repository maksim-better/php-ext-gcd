PHP_ARG_ENABLE([gcd],
  [whether to enable gcd support],
  [AS_HELP_STRING([--enable-gcd], [Enable gcd support])],
  [no])

if test "$PHP_GCD" != "no"; then
  dnl T002: ZTS 检测——拒绝 NTS 构建（PHP_THREAD_SAFETY 由 PHP 构建系统在扩展配置前置位）
  AC_MSG_CHECKING([whether PHP build is ZTS (thread-safe)])
  if test "$PHP_THREAD_SAFETY" != "yes"; then
    AC_MSG_ERROR([ext-gcd requires a ZTS PHP build (got NTS). Rebuild PHP with --enable-zts.])
  fi
  AC_MSG_RESULT([yes])

  AC_DEFINE(HAVE_GCD, 1, [Whether you have gcd support])
  PHP_NEW_EXTENSION(gcd, gcd.c \
    src/runtime/php_context.c \
    src/runtime/copy.c \
    src/runtime/task.c \
    src/runtime/thread_pool.c \
    src/queue/queue.c \
    src/queue/dispatch.c \
    src/queue/group.c \
    src/container/atomic.c \
    src/container/cqueue.c \
    src/container/set.c \
    src/container/map.c, $ext_shared)
fi