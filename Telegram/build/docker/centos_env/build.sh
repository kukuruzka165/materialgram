#!/bin/bash
set -e

cd Telegram
./configure.sh "$@"
# KEEP_GOING=1 lets ninja report every failing translation unit in one
# run instead of stopping at the first; the exit status still fails.
cmake --build ../out --config "${CONFIG:-Release}" ${KEEP_GOING:+-- -k 0}

([[ -d ../out/install ]] && rm -rf ../out/install; mkdir -p ../out/install) && DESTDIR=../out/install cmake --install ../out --config "${CONFIG:-Release}"
