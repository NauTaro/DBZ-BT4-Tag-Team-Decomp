#!/bin/sh
# Native release builds use blank data tables. No ISO is needed or included.
set -eu
case "${1:-}" in
 linux) export BT3_CC=clang64 ;;
 mac) export BT3_CC=mac64 ;;
 *) echo "usage: inside_native.sh linux|mac" >&2; exit 2 ;;
esac
export BT3_SKELETON=1
PYTHON=${PYTHON:-python3}
mkdir -p port/build
"$PYTHON" port/tools/undefined.py > "port/build/compile_$BT3_CC.txt" 2>&1 || {
    tail -60 "port/build/compile_$BT3_CC.txt"; exit 1;
}
if grep -q '^FAILED' "port/build/compile_$BT3_CC.txt"; then
    grep -A12 '^FAILED' "port/build/compile_$BT3_CC.txt" | head -100; exit 1
fi
"$PYTHON" port/tools/link.py > "port/build/link_$BT3_CC.txt" 2>&1 || {
    cat "port/build/link_$BT3_CC.txt"; exit 1;
}
cat "port/build/link_$BT3_CC.txt"
"$PYTHON" port/tests/native_transport/run.py
"$PYTHON" port/tests/native_transport_queue/run.py
"$PYTHON" port/tests/native_rtc/run.py
sh port/setup/build.sh "$1"
"$PYTHON" port/tools/package.py
"$PYTHON" port/tools/verify_native_package.py "$1"
