#!/bin/bash
# Build and run the GTA-side tests on Linux (g++), against the fake GTA (gta/tests/fake_gta.cpp).
#   gta/tests/build_tests.sh [--no-run]
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=$ROOT/build/tests
mkdir -p "$OUT"
CXX=${CXX:-g++}
FLAGS="-std=c++17 -O1 -g -Wall -Wno-unknown-pragmas -Wno-missing-field-initializers -fsanitize=address,undefined"
SRC=$ROOT/gta/src
COMMON=("$SRC/core/json.cpp" "$SRC/core/log.cpp" "$SRC/core/version.cpp" "$SRC/core/patterns.cpp" "$SRC/net/bridge.cpp"
	"$HERE/fake_gta.cpp" "$HERE/platform_fake.cpp")
GAME=("$SRC"/game/*.cpp)
$CXX $FLAGS -o "$OUT/test_core" "$HERE/test_core.cpp" "${COMMON[@]}" "${GAME[@]}"
$CXX $FLAGS -o "$OUT/test_game" "$HERE/test_game.cpp" "${COMMON[@]}" "${GAME[@]}"
$CXX $FLAGS -o "$OUT/gta_sim" "$HERE/gta_sim.cpp" "${COMMON[@]}" "${GAME[@]}"
[ "${1:-}" = "--no-run" ] && exit 0
"$OUT/test_core" "$OUT/lua_samples.txt"
"$OUT/test_game"
