# OdysseyVK — short guide (English)

The full manual, with the measurements behind every decision, is [GUIA.md](GUIA.md) (Spanish).
Install and overview: [README](../README.md).

## Commands

```sh
odysseyvk doctor                     # hardware, permissions and panel diagnosis
odysseyvk serve                      # the service: watches fullscreen windows (runs as a user unit)
odysseyvk-panel                      # the settings panel (also from the app menu; `odysseyvk ajustes` is the same, slower to start)
odysseyvk-panel --juegos             # the panel open on the Games tab
odysseyvk integraciones [ver|escribir]  # Steam launch options per game: show them / write the pending ones
odysseyvk play video_sbs.mkv         # play a stereo file with the built-in mpv viewer
odysseyvk test                       # fullscreen calibration pattern
odysseyvk capture3d --uuid <id>      # weave the window KWin captures (what the service launches)
odysseyvk ctl net [download [fp32]]  # the depth network: status / download it
odysseyvk ctl estado                 # what the viewer is doing right now
odysseyvk ctl windows                # list windows and the fullscreen candidate
odysseyvk ctl ajuste clave=valor     # send a live number or order to the open viewer
odysseyvk ctl barra guardar [N]      # keep N small frames to tune the player-bar detector
odysseyvk mapa frame.png --sueltas   # depth maps of still images, to inspect
odysseyvk actualizar                 # download, build, install the latest version, restart
odysseyvk desinstalar                # remove program, service and network (config stays)
```

Service: `systemctl --user start|stop|restart odysseyvk-serve`, log with
`journalctl --user -u odysseyvk-serve -f`.

## The card and the pill

A fullscreen window triggers the card (bottom right): *Normal video* converts a single image to 3D
with the depth network; *Split video* is side-by-side. The checkbox remembers the choice for that
app (`auto = app:conv|sbs` in `serve.conf`). Closing the card leaves the video in 2D.

The pill (top right) has the 3D switch, two sliders (3D depth / 3D pop-out), the two video-type
buttons and the gear. The mouse passes through the viewer outside the pill, so you can control the
player. `Tab` hands the screen back to the browser; clicking the taskbar returns it.

## Keys

| Keys | What |
|---|---|
| `Ctrl+Shift+1` | 3D on / off (global, like the Odyssey 3D Hub) |
| `Ctrl+Shift+3` / `4` | 3D depth − / + |
| `Ctrl+Shift+5` / `6` | 3D pop-out − / + |
| `Ctrl+Shift+7` / `8` | pill smaller / bigger |
| `Ctrl+Shift+F1` | hide / show the pill |
| `Ctrl+Shift+9` | depth debug view: depth in gray (white = near), saliency in orange, the screen plane in cyan (`ODYSSEYVK_VER_PROFUNDIDAD=1` starts with it, for `--dump`) |
| In the viewer: `Esc` leave · `3` 2D/3D · `F` swap eyes · `M` cycle format · `C` converter on/off · `P`/space pause · `1` `2` switch depth network · `[` `]` `;` `'` lens pitch fine-tune · `8` reset |

## 2D → 3D conversion

- Network: Depth Anything V2 Small (open, from Hugging Face `onnx-community/depth-anything-v2-small`) in
  `~/.local/share/odysseyvk/models/` (until 1.15 `modelos/`; 1.16 renames it on first start and updates the paths
  in `serve.conf`). The panel's 3D tab lists the networks with a checkbox each: the FP16 (`model_fp16.onnx`,
  50 MB, recommended), the FP32 (`model.onnx`, 99 MB) and any other `.onnx` placed in that folder. One not
  downloaded yet is grey with «Download · 50 MB» and clicking it downloads it and sets it; downloaded ones show a
  blue tag. The ticked one goes to `modelo =` in `serve.conf` and a running viewer switches to it at once. From the
  terminal: `odysseyvk ctl net download` (FP16) or `odysseyvk ctl net download fp32`. If the Odyssey 3D Hub's own
  `mde_large.onnx` is in that folder it is used when nothing is ticked (key `1`/`2` switches).
- Videos only (since 1.17, experimental): Video Depth Anything Small (`video_depth_anything_small_fp32.onnx`, a
  community ONNX export of the Apache-2.0 model; any `.onnx` with «video» in its name counts too) looks at a window of
  the last 32 frames at 504×280 and keeps the newest map; the window restarts at scene cuts, which for this network
  are found on the image before it runs (a 32×32 luminance thumbnail against the previous one; cut above 0.14). It goes to
  `modelo_video =` in `serve.conf`; the service passes it to the viewer only for non-game windows, and a running video
  session switches when it's ticked or unticked (a game ignores it). Needs ~2.3 GB on the GPU (the CUDA cap rises to
  3.5 GB for it) and refuses the CPU, falling back to the usual network. In the panel the two lists sit side by side
  (the usual on the left, videos only on the right), and a side with more than two networks becomes a drop-down.
- Debug view (since 1.17): the panel's *Debug view* checkbox sends `ver_profundidad = 1|0` to the viewer (the
  `Ctrl+Shift+9` view) and keeps only the two network lists, in a column on the left, without the veil and with the
  keyboard on demand, so networks can be switched while watching the depth. Beside the checkbox: `red_ms` (the
  network alone), `mapa_ms` (with preparing and cleaning the map), `mapas_s` and `fps` from the viewer's `estado`,
  refreshed every second. It mirrors the viewer: opening the panel while the depth view is on opens this layout.
- GPU engine (since 1.17): whatever the installed ONNX Runtime offers, in this order: CUDA (`onnxruntime-cuda`,
  NVIDIA), MIGraphX (`onnxruntime-rocm`, AMD), the older ROCm engine, else the CPU (where the FP32 is used if
  present). `ODYSSEYVK_PROVEEDOR=cuda|migraphx|rocm|cpu` forces one. With AMD, ONNX Runtime's HIP device numbers are
  matched to the amdgpu cards by PCI address; if the requested card has under a quarter of the memory of the
  biggest one (an integrated Radeon, typically), the biggest is used, and `HIP_VISIBLE_DEVICES` pins one. MIGraphX
  prepares the network for the card at the start of each session, which can take a while. The AMD path is not yet
  tested on AMD hardware.
- Which card runs the network (since 1.18, `red = auto` in `serve.conf`, the default): the heavy part, the network,
  goes to a secondary card, never the one that draws the 3D and the game unless there is no other. In order: a
  dedicated card other than the viewer's (with one, straight there, nothing measured); else the viewer's card and the
  integrated ones, timed against each other; else the CPU. When the network could go to more than one card, the viewer
  times each with the network that is running (A B A: the current one, the others, the current one again; 8 maps to
  warm up and 3 s each), keeps the one that costs the game least (new frames per second, then lost refreshes, then how
  busy the viewer's card stays, then the network's time) and saves it in `~/.local/state/odysseyvk/tarjeta_red`
  (delete a line to time it again). `red = 0` or `red = 1` pins a card by hand, as before.
  `ODYSSEYVK_TARJETA_MEDIR=todas` times every card with room, the viewer's included (for testing on a machine with a
  single secondary card).
- A test on every layer change (since 1.18): when what gives the depth changes (another network, the video network,
  DLSS depth coming or going, or another DLSS preset in the game) and holds for 1.5 s, the viewer measures what runs
  for 3 s and writes a `[prueba]` line to the log (and `prueba =` to its `estado`): the game's FPS, the layer's copies,
  maps per second and their time, lost refreshes and how busy each card is.
- A map is computed 6 times per second on the GPU (`mapas_por_segundo`); the panel's Performance
  tab shows the time per map and the frame rate (120 or 165 fps).
- Edges are refined per pixel against the image colour (v0.89), the subject is placed on the
  screen plane (`saliencia = si`, `pivote = 0,3`), near/far limits are Samsung's. Relief strength:
  `relieve` (0,5–2) or the panel slider.
- Split videos too deep for the lens: `dividido_suavizado = si | auto | no`.
- Subtitles and overlays are pinned to the screen plane so they don't float.

## Head tracking

The monitor's IR stereo camera (`04e8:20d7`) is used when `rastreo = si`; the viewer prints
`[rastreo]` lines with the eye position. Without it, the nominal factory eye position is used.
Any webcam can stand in (`camara = /dev/videoN`), measuring face size for distance.

## Integrations (Steam launch options, since 1.15)

The panel's **Integrations** tab builds each game's Steam launch options from pieces instead of typing them:
the layer variables (`ODYSSEYVK=1`, `MANGOHUD=1`, `ENABLE_VKDAEDALUS=1`, `NEURAL_FORGE_ENABLE=1`, `ENABLE_VKBASALT=1`,
shown without `=1` while off), other variables, `vkdaedalus-run` and a custom command before `%command%`, game options
after it, and gamescope (render and output resolution by abbreviation, `-S stretch|fit`, `-f`,
`--force-grab-cursor`, and upscaling with FSR or NIS, which turns gamescope on). Since 1.16 the Games tab's footer
has a box for `ODYSSEYVK=1` in all games (DLSS depth; its reminder shows on hover, next to how many games have it
their own way): it changes only the all-games value. A game with `ODYSSEYVK=1` of its own follows it; a game where it
was removed (`odysseyvk = 0` in its section) keeps it removed either way. The result follows this format:

```
gamescope -w 2048 -h 1536 -W 3840 -H 2160 -S stretch -f --force-grab-cursor -- env MANGOHUD=1 ... vkdaedalus-run %command% --launcher-skip
```

- The tab's settings apply to **every installed game**; the gear on a cover (Games tab, on hover) opens that game's
  own settings, marked with a dot, and "Reset" returns it to the all-games ones.
- With Neural Forge on, `NEURAL_FORGE_TARGET_EXE` is taken from `neural-forge-cli games` (or kept from the game's line);
  a game missing from that list says "Not in Neural Forge's list: add it in its Setup tab".
- Since 1.18, Neural Forge's offloading (`nf_offload` in `integraciones.conf`): with Neural Forge on, a switch writes
  `NEURAL_FORGE_OFFLOAD_GPU=vendor:device@address` of the second card (an NVIDIA that is not the 3D monitor's: the
  `pantalla` output in `serve.conf`, else the one whose EDID says G90XF, else the boot GPU), and shows its name and
  real PCIe link (the lower of the card and its slot: an RTX 5080 that can do 5.0 x16 in a 4.0 x4 slot shows
  `PCIe 4.0 x4`), warning at x4 or x8. Turning it on turns Neural Forge on; turning Neural Forge off turns it off. A
  value already in the game's line (`=1`, `same`) is kept. `odysseyvk integraciones linea TEXT` shows the card found.
- Since 1.18, `ENABLE_MAKO` (MAKO's frame generation layer) is one of the layer switches, and a right-click on the FPS
  box in the 3D opens its menu for the game underneath (see below).
- Since 1.18, *Game's card* (`juego_tarjeta`, with two or more dedicated cards): sends the game to one card with
  `DXVK_FILTER_DEVICE_NAME` and `VKD3D_FILTER_DEVICE_NAME` (DX9 to DX12, by the name the game sees) and
  `MESA_VK_DEVICE_SELECT` (native Vulkan, the card order; not `_FORCE_DEFAULT_DEVICE`, which would also hide the other
  card from Neural Forge, whose layer sits above Mesa's). Empty means where Proton puts it: the first dedicated card by
  PCI address, which is not always the 3D monitor's card. When the game's card is not the monitor's, a note says which
  link every frame crosses (the slower of the two), and Neural Forge's second card is worked out against the game's.
- The 3D view runs on the card that drives the 3D monitor (the `pantalla` output, by its PCI address); with the
  monitor on another card than the first, it used to draw on the first and every 3D frame crossed PCIe to the screen.
  And the depth network avoids the game's card too, read from the game's environment (or the default card).
- What isn't installed shows greyed out with a "Not installed" pill and can't be turned on: a layer counts as
  installed when some Vulkan implicit-layer manifest is enabled by its variable (`enable_environment`), and
  `vkdaedalus-run` and gamescope when their command is in the `PATH`. `ENABLE_VKDAEDALUS` and `vkdaedalus-run` are
  one or the other: while one is on, the other is greyed out.
- OdysseyVK writes the **whole line** into Steam's `userdata/<account>/config/localconfig.vdf`. Steam only reads
  it at startup and rewrites it on exit, so with Steam open the change is kept pending and the service writes it as
  soon as Steam closes. Every write first saves a copy in `~/.local/state/odysseyvk/steam/` (the very first one,
  `localconfig-antes-de-odysseyvk.vdf`, is never deleted).
- Existing lines are read first, and so are later changes made outside (by hand in Steam, or by Neural Forge):
  they become that game's own settings, so nothing is lost. A line that can't be split into pieces (`;`, pipes,
  redirections, `$(...)`) is left alone until you choose "Use OdysseyVK's".
- Stored in `~/.config/odysseyvk/integraciones.conf`: `[todos]` for every game, `[appid]` for each game's own.

## With MAKO, vkDaedalus and MangoHud (since 1.15)

- MAKO's frame generation works alongside the OdysseyVK layer when it is enabled with `ENABLE_MAKO=1` or through
  `vkdaedalus-run`. The OdysseyVK layer sits above MAKO, so it sees the game's real frames. **Don't use
  `mako-launch`**: it sets `VK_IMPLICIT_LAYER_PATH` to MAKO's own folder, which leaves out the OdysseyVK layer (and
  Neural Forge).
- Since 1.18, a right-click on the FPS box opens MAKO's frame generation for the game underneath: on/off, model (LSFG,
  Lossless Scaling's, or MAKO FG, MAKO's own), x2 to x4, flow and performance mode, written into the game's
  `[[profile]]` in MAKO's `conf.toml` (backed up, checked with `mako-cli validate`). Changes apply on the fly; a model
  switch applies when the game is reopened. MAKO generates below the OdysseyVK layer, in the game: the layer still
  sees only real frames. There is no card choice: MAKO doesn't generate on another card than the game's.
- MAKO FG needs `mako-fg-host`, which MAKO only looks for in `$MAKO_FG_HOST` or next to its own library
  (`/usr/lib/mako-fg/`); a MAKO built from source lacks it. *Install MAKO FG* (in that menu, and under `ENABLE_MAKO`
  in Integrations) or `odysseyvk mako instalar` downloads MAKO FG V1.24 from Hugging Face (19 MB, pinned revision,
  every file SHA-256 checked, nothing replaced until all of it checks out) into `~/.local/share/odysseyvk/mako-fg`
  without root, and Integrations adds `MAKO_FG_HOST=` to the Steam line of games with `ENABLE_MAKO=1` (written once
  Steam is closed; not added if MAKO has its own or the game sets one by hand). `odysseyvk mako estado` shows what is
  found; `odysseyvk mako quitar` removes it. MAKO FG is licensed CC BY-NC 4.0 (non-commercial).
- Real vs generated frames: `ODYSSEYVK_CONTAR_MAKO=1` (Integrations adds it with `ODYSSEYVK=1` and `ENABLE_MAKO=1`)
  enables the meta-layer `VK_LAYER_odysseyvk_mako_contado` = [OdysseyVK layer, MAKO, `VK_LAYER_odysseyvk_contador`].
  The OdysseyVK layer counts the game's presents above MAKO and the counter counts what MAKO presents below it, in
  `/dev/shm/odysseyvk-fg-<pid>`; the FPS box shows *Real* and *Generated*. Same idea as vkDaedalus's present counter.
  Integrations doesn't add it to games with Neural Forge: the meta-layer would pull MAKO above Neural Forge (implicit
  layers load in discovery order, closest to the game first), and Neural Forge would then run its model on generated frames too.
  `install.sh` puts the counter in `~/.local/share/vulkan/explicit_layer.d` and the meta-layer next to the OdysseyVK
  layer's manifest. `ODYSSEYVK_NO_CONTAR=1` turns it off.
- Since 1.18, Integrations puts `odysseyvk juego` before `%command%` in games with Neural Forge. It runs the game and
  waits; if the kernel log (`journalctl -k`) shows an `NVRM: Xid` for one of the game's processes (those with its
  `SteamAppId`) while it ran, it writes `~/.local/state/odysseyvk/nf-apagado-<appid>`, shows a notice and starts the
  game once more with `NEURAL_FORGE_DISABLE=1`. While that file exists the game starts without Neural Forge; turning
  Neural Forge back on in that game's Integrations deletes it. The game's exit code is passed through.
- Game keys (registered with KDE only while the viewer is over a game): `Insert` pill, `Delete` DLSS depth or the
  chosen model, `End` MAKO frame generation, `Page Up` / `Page Down` multiplier. `End` collides with vkDaedalus's menu
  key; rebind one of them if you use both.
- ReShade effects (since 1.18) run in the viewer, not in the game: `CadenaEfectos` (src/efectos_vk.cpp) blits the
  captured frame to its own RGBA8 image, runs each enabled `.fx` (compiled by `third_party/reshadefx`, the ReShade FX
  compiler with vkDaedalus's patches, on a worker thread) and the weave samples the result. ReShade's depth texture is
  the 3D depth map (R8G8, 255 = near) stretched into a 16-bit image, with `RESHADE_DEPTH_INPUT_IS_REVERSED=1` and
  `RESHADE_DEPTH_LINEARIZATION_FAR_PLANE=1.0`. Settings: `~/.config/odysseyvk/effects/all.conf` and `<appid>.conf`
  (vkBasalt syntax plus `puestos = si|no`); folder: `efectos_carpeta` in serve.conf or `~/.config/vkdaedalus/reshade`.
  Not supported: compute passes, 3D textures, stencil. Tests: `odysseyvk efectos compilar NAME`, `odysseyvk ctl
  efectos IN.png OUT.png [--prof DEPTH.png] [NAME...]`, and `ODYSSEYVK_EFECTOS_JUEGO=APPID|todos` to force a game's
  effects in a test viewer. If vkDaedalus also runs effects in the game, they apply twice.
- Public shader sources: `odysseyvk efectos fuentes [ID...]` asks GitHub for each repository's tree
  (`api.github.com/repos/OWNER/REPO/git/trees/HEAD?recursive=1`, one request each) and compares every file's git blob SHA-1
  with the files under the ReShade folder's `Shaders/` and `Textures/`; the result goes to `~/.local/state/odysseyvk/efectos-fuentes`.
  `odysseyvk efectos bajar ID` downloads only new or changed files from `raw.githubusercontent.com`, checks each SHA-1,
  backs up replaced files under `~/.local/state/odysseyvk/efectos-copia/` and skips files named like one you already have
  elsewhere. `ODYSSEYVK_EFECTOS_API` / `ODYSSEYVK_EFECTOS_RAW` point both at fixtures for testing.
- Only one overlay in front at a time:
  - while the vkDaedalus menu is open (End), the OdysseyVK pill steps aside and the 3D goes flat, so the menu
    reads right. Its state is read from the file vkDaedalus's input hook keeps in `$XDG_RUNTIME_DIR`, so it needs
    a Wine game started with `vkdaedalus-run`;
  - if MangoHud or the vkDaedalus HUD shows FPS, OdysseyVK's FPS box doesn't;
  - the corner where MangoHud (its `position`) or the vkDaedalus HUD (top left) draws goes to the screen plane, like
    the game's own HUD.

## Configuration keys (serve.conf)

| Key | Default | Meaning |
|---|---|---|
| `idioma` | `en` | interface language: `en` / `es` / `fr` / `de` / `zh` / `ja` / `ru` (Settings → App → Language; Other in 1.12, the Program tab up to 1.11) |
| `modo` | `ask` | `ask` / `conv` / `sbs` / `tb` / `flat` |
| `auto` | — | `app:modo`, one per line; the card's "remember" writes them |
| `ignore` | system apps | app ids never offered 3D (screen capture and recording apps, such as Spectacle, Flameshot or OBS, are always left out too, since 1.16) |
| `pantalla` | auto | output name of the Odyssey (`DP-4`) |
| `espera` | 40 | seconds the card stays |
| `rastreo` | `no` | eye tracking with the IR camera (Settings → Head tracking) |
| `suavizado` | 0,35 | tracker filter |
| `relieve` | 1,00 | converter relief strength |
| `dividido_suavizado` | `si` | smooth split videos: `si` / `auto` / `no` |
| `crosstalk_base` | 0,0129 | ghosting compensation (0–0,25) |
| `filtro` | 10 | weave filter slope between the views (Samsung's); the panel's "Smooth between the views" shows it as 1/slope (10–100 %) |
| `hz` | 120 | panel refresh in 3D |
| `vrr_en_3d` | `no` | `si` keeps variable refresh (VRR) during 3D; off by default since 1.11 |
| `profundidad_dlss` / `hud_plano` / `fps_juego` | `si` / `si` / `si` | games with DLSS and `ODYSSEYVK=1` in their launch options (since 1.14, Settings → Games with DLSS): use the depth the game gives DLSS instead of the network; keep the game's HUD flat on the screen (`no` also stops the layer from touching the HUD in the game); the small FPS box next to the pill |
| `capa_fps_tope` | 0 | the DLSS layer's FPS cap (Settings → Smoothness): at most this many depth and HUD copies per second inside the game; 0 = no cap. The viewer makes 6 maps per second by default |
| `profundidad_conv` / `salida_conv` | 75 / 50 | starting Depth (fixed screen plane; `auto` = saliency) and Pop-out for normal video; `_suave` 75/50, `_sbs` 50/0; written when the sliders move |
| `exp_red` / `exp_mapa` / `exp_tope_arrastre` / `exp_anticipacion` | — | network input proportion (`cuadrado`/`518`/`770`) and depth map size (`518`/`770`/`1022`), keys only since 1.12; lag cap (`si`, Settings → Smoothness); head anticipation in ms (Settings → Head tracking) |
| `exp_sin_red` / `exp_sin_red_mezcla` / `exp_sin_red_relieve` / `exp_sin_red_topes` / `exp_sin_red_paralaje` | `no` / `0,35` / `1,00` / `2,75` / `2,00` | performance mode (the Neural | Performance switch on the pill): depth from the image on the CPU instead of the network (`si`), its floor ↔ image mix, relief strength, relief reach (the per-eye caps multiplied) and head-parallax exaggeration; the lightning-bolt menu on the pill writes all four (a preset or the sliders); in this mode Depth, Pop-out and `relieve` don't apply. With it on, VRR stays as the desktop has it |
| `atajo_<action>` | factory | a shortcut, e.g. `atajo_3d = Ctrl+Alt+3`; set in the panel's main page (the OdysseyVK logo) → Keyboard shortcuts |
| `cantos` | finos | `viejo` = pre-0.89 edges, to compare |
| `barra_2d` | `si` | player bar rows: `si` / `entera` (raw) / `no` |
| `actualizaciones` | `si` | update card |
| `actualizar_auto` | `no` | install updates automatically |
| `actualizaciones_cada` | 6 | hours between update checks |
| `rampa_x`, `rampa_y`, `fase` | — | lens pitch calibration of your unit (see GUIA.md) |

## Troubleshooting

- **`doctor` doesn't see lens / scaler / camera** → replug the monitor's USB-B cable. Never
  `udevadm trigger` on it.
- **Vertical fine stripes all over** → the lens pitch of your unit: tune `rampa_x` / `rampa_y`
  with the one-eye pattern (`b` key in the viewer); the procedure is in GUIA.md ("Calibrar el paso").
- **Ghosting** → lower `crosstalk_base` and `relieve` in the panel; check the IR camera is tracking.
- **Browser video freezes under the viewer** → Firefox/Zen: `about:config` →
  `gfx.vsync.compositor.unobserve-count = 0`; Chrome: `--disable-gpu-vsync`.
- **Monitor stays rotated / scaled after a crash** → `odysseyvk ctl panel restore`.
- **No depth, video stays flat** → the network is missing: `odysseyvk ctl net download`.
- Logs: `journalctl --user -u odysseyvk-serve -f`; the update log is
  `~/.local/state/odysseyvk/actualizar.log`.
