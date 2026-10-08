# MODLOG: BeamNG × Los Santos

Journal for the GTA V Legacy × BeamNG.drive mashup. Anything not written here is lost at the next context
compaction. Newest entries at the bottom of each section.

## The idea (agreed with the creator, 2026-10-08)

The real BeamNG car, with BeamNG's physics, tuning and soft-body damage, driving through Los Santos inside
GTA V story mode. GTA brings the city, traffic, pedestrians, police and wanted level. BeamNG brings the car
(model, physics, tuning, deformation) from the player's own BeamNG install.

First playable version must have all three:
1. Drive and crash: F6 in GTA opens BeamNG's vehicle selector; the chosen car appears in Los Santos and drives
   with BeamNG physics; crashes into traffic, poles and walls deform it for real.
2. Police: GTA's wanted level; cops chase and ram, and each ram deforms the BeamNG car.
3. Garage and tuning: F7 opens BeamNG's tuning (parts + sliders) any time; Los Santos Customs repairs the car.

Solo only (GTA story mode). Steam or Epic, GTA V **Legacy** only. The creator wants it to "just work" on
whatever version the store installs: today that is 1.0.3889.0 on both stores.

## Players need
- GTA V Legacy (Steam or Epic), story mode. Melty installs Ultimate ASI Loader (v9.7.4) itself.
- BeamNG.drive (Steam), current version (0.39.x / Melty reports 2026.9.16.0 on players' PCs).
- A PC strong enough to run both at once. Windows 10 2004+ (Windows Graphics Capture).

## Route and why

**Passthrough, picture + state** (route names from universal-modder `choosing-a-mashup-route.md`):
both real games run side by side and exchange state over UDP on 127.0.0.1; BeamNG's picture of the car is
composited into GTA's frame.

- **Who owns what.** BeamNG owns the player's car (pose, velocity, damage, tuning). GTA owns the world, the
  camera, traffic, peds, police, time of day. GTA's invisible *proxy car* follows BeamNG's car so traffic,
  peds and police react to it; GTA's world near the car is mirrored into BeamNG as invisible collision.
- **Coordinates.** GTA and BeamNG are both right-handed, Z up, metres. BeamNG world = GTA world (identity),
  optionally shifted by `origin` (sheet `settings`). Vehicle forward: GTA +Y, BeamNG −Y (rotate 180° about Z).
- **Picture.** BeamNG renders only the car (empty level, sky and ground hidden, key-colour background) from
  GTA's camera pose. GTA's ReShade add-on captures BeamNG's window with Windows Graphics Capture on GTA's own
  D3D11 device, keys out the background, and depth-tests the car against GTA's depth buffer using the car's
  oriented bounding box as its depth. Based on universal-modder's Minecraft × GTA V compositor (MIT).
- **Why not the alternatives.**
  - Matching GTA car with BeamNG damage only: the creator chose the real BeamNG car.
  - ReShade inside BeamNG for its depth: would add files to BeamNG's game folder and load in every BeamNG
    session; window capture + OBB depth needs nothing in BeamNG's folder.
  - Script Hook V: Melty cannot install it and it can't be shipped (proprietary). So **BeamLS.asi calls
    GTA's natives itself** (pattern-scanned native table + per-build hash table), like Melty's own
    Minecraft × GTA V (pinned to 3889).

## Hard constraints from Melty (game_info, 2026-10-08)
- GTA: build for story mode only, say so on the listing. Never add `-nobattleye` to commandline.txt.
- GTA: native hashes change with every game update; Melty's own reader was pinned to a build.
- GTA: key bindings are encrypted; read input with IS_CONTROL_PRESSED / GET_DISABLED_CONTROL_NORMAL.
- GTA camera: GET_FINAL_RENDERED_CAM_* (Z up, metres, rotation order 2, degrees).
- BeamNG: Melty installs no loader; only plain files. Mods folder: `%LOCALAPPDATA%\BeamNG\BeamNG.drive\current\mods`
  (BeamNG ≥ 0.37, docs.beamng.com/support/userfolder).
- Selling: GTA tips-only. Free mod.

## Environment
- Development so far: Linux cloud container (no Windows, no games). Installed here: `mingw-w64` (Ubuntu
  package, cross-compiles the ASI and add-on), `luajit` (runs the BeamNG Lua tests with stubs).
- In-game work (native table for 3889, patterns, BeamNG API names, capture, tuning, screenshot) needs the
  creator's Windows PC running Claude Code. Everything that needs it is an unverified cell in `sheets/`;
  `python3 tools/preflight.py` lists them.

## Facts learned (with sources)
- BeamMP (AGPL, read only for API facts, no code copied): GE Lua has `require('socket')` (LuaSocket) with
  `settimeout(0)`; `veh:setClusterPosRelRot(refNode, x,y,z, qx,qy,qz,qw)`, `veh:applyClusterVelocityScaleAdd(refNode, 1, vx,vy,vz)`,
  `veh:setPositionNoPhysicsReset(Point3F)`, `veh:setPositionRotation(...)` + `veh:resetBrokenFlexMesh()`,
  `quatFromDir(-veh:getDirectionVector(), veh:getDirectionVectorUp())` = vehicle rotation;
  vehicle Lua `input.event(name, value, FILTER_DIRECT, nil,nil,nil, source)` with names steering, throttle,
  brake, parkingbrake, clutch; `input.setAllowedInputSource(name, "local", false)`;
  `obj:requestReset(RESET_PHYSICS)`; `core_camera.setPosRot(0, x,y,z, qx,qy,qz,qw)`; `commands.setFreeCamera()`;
  `freeroam_freeroam.startFreeroam(levelPath)`; `scripts/<mod>/modScript.lua` runs on mod load with `load()` and
  `setExtensionUnloadMode()`.
- BeamNGpy (MIT): launch args `-userpath <dir>` (breaks on spaces), `-lua "<code>"`, `-console`, `-gfx dx11`.
- GTA native signatures and original hashes: alloc8or/gta5-nativedb-data `natives.json`.
- Native hashes ARE scrambled per build: CSY0N/GTA-V-Crossmaps (b3586) maps PLAYER_PED_ID 0xD80958FC74E988A6 →
  0x4A8C381C258A124D. No public crossmap for 3889 exists (searched 2026-10-08). → generate on the PC.

## Decisions
1. Internal id `beamls` (files `BeamLS.asi`, `BeamLS.addon64`, BeamNG mod folder `beamls`).
2. Bridge: JSON datagrams over UDP 127.0.0.1. BeamNG listens on port 47011; GTA sends `hello` from any port.
3. BeamNG part is an unpacked mod at `{localappdata}/BeamNG/BeamNG.drive/current/mods/unpacked/beamls`. Its
   modScript loads only a tiny listener; nothing else runs until BeamLS.asi says hello, so the player's
   normal BeamNG sessions are untouched.
4. The mod stays OFF on a GTA build it has no hash table for, and shuts down for good if GTA Online starts.
5. Design lives in `sheets/*.json`; code in `gta/src/gen` and `beamng/mod/lua/ge/extensions/beamls/gen` is
   generated by `tools/gen.py`. Change the sheet, then regenerate.

## Open risks (highest first)
1. 3889 hash table: must be derived on the PC (see `docs/NATIVE_TABLE.md`).
2. Script-thread context for natives without Script Hook V (patterns for 3889).
3. BeamNG UI state names for vehicle selector / tuning; focus switching between windows.
4. Ground mirroring smoothness (slabs) at speed; fallback "flat" mode.
5. Composite latency (BeamNG frame vs GTA camera).

## Log
- 2026-10-08: interview done, plan agreed. Melty connected (no existing mods). Research done.
