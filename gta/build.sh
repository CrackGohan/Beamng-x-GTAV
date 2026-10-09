#!/bin/bash
# Cross-compile BeamLS.asi (Windows x64) with mingw-w64. Run tools/preflight.py first; it must be clean.
#   gta/build.sh            -> build/gta/BeamLS.asi
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
OUT=${OUT:-$ROOT/build/gta}
OBJ=$OUT/obj
CXX=${CXX:-x86_64-w64-mingw32-g++}
CC=${CC:-x86_64-w64-mingw32-gcc}
mkdir -p "$OBJ"

python3 "$ROOT/tools/preflight.py" > "$OUT/preflight.txt" || { cat "$OUT/preflight.txt"; echo "preflight is not clean: fix the sheets first"; exit 1; }

DEFS="-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE"
INC="-I$HERE/shim -I$HERE/third_party/minhook/include -I$HERE/third_party/reshade/include"
CFLAGS="-O2 -g0 -fno-strict-aliasing $DEFS $INC"
CXXFLAGS="-std=c++17 -O2 -g0 -Wall -Wno-unknown-pragmas -Wno-missing-field-initializers $DEFS $INC"

objs=()
for c in "$HERE"/third_party/minhook/src/hook.c "$HERE"/third_party/minhook/src/buffer.c \
         "$HERE"/third_party/minhook/src/trampoline.c "$HERE"/third_party/minhook/src/hde/hde64.c; do
	o="$OBJ/minhook_$(basename "${c%.c}").o"
	$CC $CFLAGS -c "$c" -o "$o"
	objs+=("$o")
done
for cpp in "$HERE"/src/dllmain.cpp "$HERE"/src/core/*.cpp "$HERE"/src/net/*.cpp "$HERE"/src/game/*.cpp "$HERE"/src/addon/*.cpp; do
	o="$OBJ/$(basename "$(dirname "$cpp")")_$(basename "${cpp%.cpp}").o"
	$CXX $CXXFLAGS -c "$cpp" -o "$o"
	objs+=("$o")
done
$CXX -shared -static -static-libgcc -static-libstdc++ -o "$OUT/BeamLS.asi" "${objs[@]}" \
	-lws2_32 -lversion -ld3d11 -ldxgi -lole32 -luuid -Wl,--subsystem,windows -s
echo "built $OUT/BeamLS.asi ($(stat -c %s "$OUT/BeamLS.asi") bytes)"
