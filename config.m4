PHP_ARG_ENABLE(sample_prof, whether to enable sampling profiler support,
[  --enable-sample-prof           Enable sampling profiler support])

if test "$PHP_SAMPLE_PROF" != "no"; then
  PHP_NEW_EXTENSION(sample_prof, sample_prof.c, $ext_shared)
  PHP_ADD_LIBRARY(pthread)
  PHP_LDFLAGS="$PHP_LDFLAGS -lpthread"
fi
