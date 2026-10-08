#!/bin/bash
# End to end without the games: the real BeamNG mod (LuaJIT + LuaSocket + fake engine) and BeamLS's real GTA
# logic (fake GTA) talk over real UDP on 127.0.0.1:47011 in real time.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
[ -x build/tests/gta_sim ] || gta/tests/build_tests.sh --no-run
luajit beamng/tests/e2e_bng.lua > build/tests/e2e_bng.out 2>&1 &
BNG=$!
sleep 0.5
build/tests/gta_sim > build/tests/e2e_gta.out 2>&1
GTA=$?
wait $BNG
BNGRC=$?
cat build/tests/e2e_gta.out build/tests/e2e_bng.out
[ $GTA -eq 0 ] && [ $BNGRC -eq 0 ] && echo "E2E OK" || { echo "E2E FAILED"; exit 1; }
