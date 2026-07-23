#!/bin/bash
# Vendor libcurl's dev headers + a linkable .so, without root. This host has the libcurl4 RUNTIME
# package but not libcurl4-openssl-dev (no curl.h, no libcurl.so symlink for -lcurl) and no sudo.
# Same trick as build_dawn_native.sh's X11 note: apt-get download + dpkg-deb -x need no privileges;
# the extracted libcurl.so symlink points at the (already-installed) runtime .so.N.N.N, so no copy
# of the actual shared object is needed — only the dev symlink + headers were missing.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

OUT="$PWD/.compat-headers"
mkdir -p "$OUT/curl" "$OUT/lib"

tmp=$(mktemp -d)
( cd "$tmp" && apt-get download libcurl4-openssl-dev && dpkg-deb -x ./libcurl4-openssl-dev_*.deb x )
cp "$tmp"/x/usr/include/*/curl/*.h "$OUT/curl/"
ln -sf /lib/x86_64-linux-gnu/libcurl.so.4 "$OUT/lib/libcurl.so"
rm -rf "$tmp"

echo "libcurl compat -> $OUT (curl/*.h + lib/libcurl.so)"
