# BeamNG × Los Santos

The real BeamNG.drive car (its physics, tuning and soft-body damage) driving through GTA V Legacy's Los
Santos in story mode. GTA brings the city, traffic, police and wanted level; BeamNG brings the car from
your own BeamNG install. Made for [Melty](https://melty.gg): one click installs and starts both games.

**Status: in development.** It builds and passes its tests without the games; it has not run in the real
games yet (see `docs/PC_PHASE.md`). Story mode only, single player, GTA V Legacy 1.0.3889.0 (Steam or Epic).

## How it plays
- **F6** in GTA: BeamNG's vehicle selector comes to the front; pick a car, press F6 in BeamNG to come back.
  The car appears next to you; get in with F.
- **F7**: BeamNG's tuning (parts and sliders) for your car, any time.
- **F8**: switch the mashup off and on.
- **E** at a Los Santos Customs door: repair.
- Crashes into traffic, walls and police rams deform the BeamNG car for real.

## How it works
Both games run side by side and talk over UDP on 127.0.0.1 (`docs/CONTRACT.md`).
- **GTA V** (`gta/`): `BeamLS.asi`, loaded by Ultimate ASI Loader, calls GTA's natives without Script Hook
  V. It keeps an invisible GTA car on the BeamNG car's pose so traffic and police react, sends GTA's
  camera, controls, ground, walls and traffic to BeamNG, and composites BeamNG's picture of the car into
  GTA's frame through ReShade (`gta/shaders/BeamLS.fx`).
- **BeamNG** (`beamng/mod`): an unpacked mod whose Lua extension drives the car from GTA's controls,
  mirrors GTA's world as invisible collision and renders the car from GTA's camera on a key-colour
  background.

## Working on it
The design lives in `sheets/*.json`; most code in `gta/src/gen` and `beamng/.../beamls/gen` is generated.
```
python3 tools/preflight.py          # every unfilled, unverified or mismatched cell
python3 tools/gen.py                # sheets -> code
gta/build.sh                        # BeamLS.asi (mingw-w64)
gta/tests/build_tests.sh            # GTA-side tests against a fake GTA
luajit beamng/tests/test_session.lua
tests/e2e.sh                        # real Lua mod + real GTA logic over UDP
python3 tools/fetch_deps.py && python3 tools/package.py 0.1.0
```
Journal: `MODLOG.md`. Credits: `CREDITS.md`. Built with AI assistance (Claude Code).
