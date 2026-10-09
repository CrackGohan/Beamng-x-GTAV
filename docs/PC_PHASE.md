# Next: the PC phase (needs the creator's Windows PC with both games)

Everything below needs the real games. Run `python3 tools/preflight.py --release`: every PC-ONLY and
UNVERIFIED line is one of these steps. A Claude Code session on the PC (Claude Desktop app, or
`claude remote-control` in this repo's folder) picks up here; read MODLOG.md first.

## 0. Set up (the agent does this itself)
- Find GTA V Legacy (Steam: `steamapps/common/Grand Theft Auto V`, app 271590; Epic: its library folder) and
  BeamNG.drive (Steam app 284160). Check GTA5.exe's file version is 1.0.3889.0.
- Tools: Git, Python 3, and a compiler: MSYS2 with mingw-w64 (same flags as `gta/build.sh`) or Visual Studio
  Build Tools. Ultimate ASI Loader (MIT, github.com/ThirteenAG/Ultimate-ASI-Loader) for dev runs outside
  Melty, as `dinput8.dll` in the GTA folder; Melty installs it for players.
- Back up `Documents/Rockstar Games/GTA V/settings.xml` and the BeamNG user folder's `settings/`.
- Never let GTA reach GTA Online with the mod loaded; ask the creator before automating clicks on GTA's
  landing page (universal-modder gotcha 20).

## 1. Native hash table for build 3889 (blocks everything on the GTA side)
`docs/NATIVE_TABLE.md`. Fill `xmap.3889` in `sheets/natives.json` for all 64 natives, `tools/gen.py`,
`gta/build.sh`. Confirm the `native_lookup` pattern (sheet patterns) in BeamLS.log.

## 2. First run of BeamLS.asi
- `BeamLS/BeamLS.log`: build check, pattern addresses, "natives ready", "ticking from the PLAYER_ID handler".
- Self-test in story mode: proxy creation, PLAYER_PED_ID, camera values. Tick the sheet rows it proves.

## 3. BeamNG side
- Copy `beamng/mod` to `%LOCALAPPDATA%/BeamNG/BeamNG.drive/current/mods/unpacked/beamls` (Melty's mapping).
- Start BeamNG by `Bin64/BeamNG.drive.x64.exe` (settings.melty_bng_launch): does it start without the
  launcher window? With Steam closed?
- After a session the BeamNG log has `bngapi ok:` / `bngapi failed:`: every failed row of sheet bng_api
  gets a corrected body (read BeamNG's own Lua under the game folder's `lua/` and `ui/`), especially the
  UI state names for the vehicle selector and tuning menu, `canvasClearColor`, slabs (TSStatic, collision
  reload), the water plane and the proxy jbeam with `.pc` variables.

## 4. Picture
- Gold-box test (sheet systems gta_camera_sync oracle): the BeamNG car must sit exactly on the proxy in the
  composite. Tune `bng_fov_is_vertical`, `bng_cam_forward_is_y`, `gta_cam_lag_frames`,
  `capture_lag_frames` and the key colour tolerance.
- BeamNG window: windowed or borderless, same aspect as GTA, behind GTA, not minimised; background
  rendering not throttled.

## 5. Gameplay checks (each system's oracle column)
Drive, crash into a wall, ram traffic, 3-star police rams, F6 car change, F7 tuning, LSC repair, night,
Del Perro pier, F8, pause menu, character switch, BeamNG closed mid-drive.

## 6. Media and release
- A real screenshot or clip of this build in GTA (the game window only).
- `python3 tools/package.py <version>`; Melty: create_mod (agreed title, tagline, description, credits,
  licence, remix), start_upload / finish_upload for both zips, submit_release with `dist/recipe.json`,
  add_screenshot, play once through Melty, publish when the creator says so. Save `dist/melty.json` as
  `melty.json` at the repo root once agreed.
