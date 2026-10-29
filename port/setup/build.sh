#!/bin/sh
# Builds the setup window (port/setup/setup.cpp, native.cpp).
#   port/setup/build.sh        -> ./BT4decomp-setup            (needs g++ and SDL3; Dear ImGui is in the repository)
#   port/setup/build.sh win    -> port/build/BT4decomp-setup.exe
#        on Linux: cross-compiled with mingw-w64 (x86_64-w64-mingw32-g++) and SDL3's mingw development package
#                  unpacked under port/build/win (or its x86_64-w64-mingw32 folder named by BT3_SDL3);
#        on Windows (MSYS2 shell): with that shell's g++ and SDL3.
set -eu
cd "$(dirname "$0")/../.." || exit 1
I=port/third_party/imgui
if [ "${1:-linux}" = win ]; then
    O=port/build/setup_win; OUT=port/build/BT4decomp-setup.exe; LIBS="-lSDL3 -static -static-libgcc -static-libstdc++ -mwindows"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) CXX=g++; CC=gcc; SDL="" ;;
        *) CXX=x86_64-w64-mingw32-g++
           CC=x86_64-w64-mingw32-gcc
           S=${BT3_SDL3:-$(ls -d port/build/win/SDL3-*/x86_64-w64-mingw32 2>/dev/null | tail -1)}
           [ -d "$S" ] || { echo "SDL3 for mingw not found: unpack SDL3-devel-*-mingw.tar.gz under port/build/win"; exit 1; }
           SDL="-I$S/include -L$S/lib" ;;
    esac
elif [ "${1:-linux}" = mac ]; then
    O=port/build/setup_mac; OUT=BT4decomp-setup; CXX=clang++; CC=clang
    S=${BT3_SDL3:-/opt/homebrew/opt/sdl3}
    SDL="-I$S/include -L$S/lib"
    LIBS="-lSDL3 -lz -lpthread -Wl,-rpath,@executable_path/lib -mmacosx-version-min=11.0"
else
    O=port/build/setup; OUT=BT4decomp-setup; CXX=g++; CC=gcc
    SDL="$(pkg-config --cflags --libs sdl3 zlib)"
    LIBS="-lSDL3 -lz -lpthread -ldl -static-libstdc++ -static-libgcc -Wl,-rpath,\$ORIGIN/lib"
fi
mkdir -p $O
for f in $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp $I/imgui_impl_sdl3.cpp $I/imgui_impl_sdlgpu3.cpp $I/imgui_impl_opengl3.cpp port/setup/native.cpp port/setup/setup.cpp; do
    o=$O/$(basename "$f" .cpp).o
    if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ port/setup/native.h -nt "$o" ]; then
        $CXX -std=c++17 -O2 -w -I$I $SDL -c "$f" -o "$o" || exit 1
    fi
done
# Language packages use the same portable SHA-256 implementation as updates.
$CC -O2 $SDL -c port/src/gs/update_sign.c -o "$O/update_sign.o"
# -Wl,-Bdynamic for SDL3 only: the rest (C++ runtime, threads) is linked in
if [ "${1:-linux}" = win ]; then
    $CXX -o $OUT $O/*.o $SDL -Wl,-Bdynamic -lSDL3 -Wl,-Bstatic -lstdc++ -lwinpthread -lz -static-libgcc -mwindows && echo "built $OUT"
else
    $CXX -o $OUT $O/*.o $SDL $LIBS && echo "built ./$OUT"
fi
