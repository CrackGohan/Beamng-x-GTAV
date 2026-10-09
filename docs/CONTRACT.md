# Bridge contract: GTA V ↔ BeamNG

The message list itself is `sheets/messages.json` (generated into `gta/src/gen/protocol.hpp` and
`beamls/gen/protocol.lua`). This file fixes everything around it.

## Ownership

| Thing | Owner | The other side |
|---|---|---|
| The player's car: pose, velocity, damage, parts, tuning | BeamNG | GTA's invisible proxy car copies pose + velocity every frame |
| Player input while driving | GTA reads it (keyboard or pad) | BeamNG applies it; BeamNG's own input for those controls is disabled |
| World: ground, walls, props | GTA | BeamNG mirrors them near the car as invisible static collision |
| Traffic and police | GTA (AI, wanted level) | BeamNG mirrors them as invisible solid proxies that follow GTA |
| Camera | GTA | BeamNG's free camera copies it every frame |
| Time of day | GTA | BeamNG copies the clock |
| Pause | GTA (pause menu, loading, cutscene, switch) | BeamNG pauses physics on `mode` paused/menu/off |
| Menus (vehicle selector, tuning) | BeamNG | GTA hands over focus and waits for `menu_closed` |

Control returns to GTA when BeamNG sends `menu_closed`, when the player clicks the GTA window, or when BeamNG
goes silent for `peer_timeout_ms` (then GTA parks the proxy and shows "BeamNG disconnected").

## Units and axes

- Metres, seconds, radians for angular velocity, degrees only for FOV and heading.
- Both worlds: right-handed, Z up. GTA X east, Y north. BeamNG world = GTA world − `settings.origin`.
- Rotations on the wire are quaternions (x, y, z, w) in **GTA's convention: a car's forward is +Y, up +Z**.
  BeamNG vehicles face −Y, so BeamNG converts with a 180° turn about Z (`q_gta = q_bng * Rz(180°)`).
- Camera: GTA's final rendered camera, rotation order 2 (Z then X then Y: yaw, pitch, roll), converted to a
  quaternion with forward +Y. `cam.fov` is GTA's vertical FOV.
- GTA heading (spawn_at): degrees counter-clockwise from north.

## Channels

- One UDP socket each side, 127.0.0.1 only. BeamNG binds `settings.bridge_port` (47011); GTA binds an
  ephemeral port and BeamNG replies to the sender of the last `hello`.
- One JSON object per datagram, field `t` = message id. Unknown `t`: ignore. Missing field: keep default.
- **Latest wins.** Each side keeps only the newest message of each type per frame (cam, car, input, ground,
  walls, traffic). Event messages (hello, mode, spawn_at, repair, menu_closed, vehicle_info, bye) are
  processed in order, all of them.
- Full queue / dropped datagram: harmless; every state message is resent at its rate. Events that must not be
  lost (spawn_at, repair, mode) are repeated until the other side's state reflects them (`mode` 1 Hz,
  `car` shows the new pose, `car.damage` drops).
- Version: `hello.proto` must equal `messages.proto`; BeamNG answers a mismatching hello with
  `status{state="error"}` and ignores the session.

## Lifecycle

1. Melty starts BeamNG (recipe `together`, start first), then GTA.
2. BeamNG's modScript loads `beamls_listener`: binds the port, idle. No level is touched.
3. BeamLS.asi starts with GTA, checks the build (`gta_version`), resolves natives, waits for story mode.
4. GTA sends `hello` every second. The listener loads `beamls_main`, which loads the empty level and
   prepares the scene, then answers `hello_ack{ready=true}`.
5. Player presses F6 → `mode{menu_vehicle}` → BeamNG opens the selector in front → player picks →
   `menu_closed` → GTA sends `spawn_at` next to the player → BeamNG places the car → `car` flows →
   GTA creates the proxy, the player gets in, `mode{drive}`.
6. Shutdown: GTA Online detected → `bye{online}`, proxy deleted, BeamLS off until restart. GTA quits →
   `bye{quit}`. BeamNG unloads `beamls_main` on `bye` or after `peer_timeout_ms` of silence and returns to
   its main menu state (the listener stays).

## Timing

- GTA sends `cam` from its script tick, which runs before the frame is rendered; the composite in
  ReShade's present uses the camera of the frame being shown. `car.cam_seq` tells the compositor which camera
  BeamNG used for the picture it is capturing, so it can re-project from that camera to the current one.
- Both sides stamp nothing with wall clocks; sequence numbers only.
