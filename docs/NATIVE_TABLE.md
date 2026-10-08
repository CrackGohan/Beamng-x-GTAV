# Filling the native hash table for a GTA build

GTA V scrambles every native's hash in each game update. `sheets/natives.json` holds each native's
original hash (from the native DB) and, per build, the hash that build uses (`xmap`). BeamLS.asi stays off on
a build whose column has any empty cell. No public table for 1.0.3889.0 was found (searched 2026-10-08), so
it has to be made on the creator's PC from the running game. Status: **not done; the routes below are
candidates to try in order on the PC.**

## What is known
- Original hashes and signatures: alloc8or/gta5-nativedb-data (natives.json).
- A public table for build 3586 (CSY0N/GTA-V-Crossmaps; no license stated, so it is not shipped) shows the
  hashes change per build (PLAYER_PED_ID 0xD80958FC74E988A6 → 0x4A8C381C258A124D in 3586).
- Script Hook V 3889 contains a full table internally; scrDbg 4.1.1 also lists 3889 natives.

## Candidate routes (decide on the PC)
A. **Script Hook V on the developer's PC only (never shipped, never needed by players).** Install Script Hook V
   3889 into the dev install for one session. A dev-only ASI (`tools/xmap_probe`, to be written on the PC)
   hooks the game's native lookup function (found by BeamLS's pattern scanner), then for each original hash
   the mod needs calls Script Hook V's exported `nativeInit(hash)` / `nativeCall()` with the hook returning a
   do-nothing handler: the translated hash Script Hook V looks up is the cell value. Works if Script Hook V
   resolves handlers lazily (check first). Remove Script Hook V afterwards.
B. **Script alignment.** Dump each loaded game script's native table (translated hashes, in table order) and
   bytecode from memory; align the order of native calls in the bytecode with a public decompile of the same
   build's scripts that names the natives (if one exists for the build).
C. **Behaviour check (always, after A or B).** BeamLS's self-test exercises every filled row in the running
   game before the cell's verification is ticked: GET_GAME_TIMER increases, PLAYER_PED_ID is a ped,
   GET_ENTITY_COORDS of the player is within 20 m of the camera, CREATE_VEHICLE returns an entity that
   DOES_ENTITY_EXIST, and so on.

Write the results into `sheets/natives.json` (`xmap.3889`), run `python3 tools/gen.py`, rebuild.
When a new GTA build ships, add it to `games.supported_builds`, repeat, and widen `games.version_range`.
