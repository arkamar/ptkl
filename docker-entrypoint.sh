#! /bin/sh

exec env LD_PRELOAD=/lib/libptkl.so ${@} 100>&2
