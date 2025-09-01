#!/bin/bash
set -e

cd Telegram
./configure.sh "$@"
cmake --build ../out --config "${CONFIG:-Release}"

([[ -d ../out/install ]] && rm -rf ../out/install; mkdir -p ../out/install) && DESTDIR=../out/install cmake --install ../out --config "${CONFIG:-Release}"
