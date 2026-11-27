# OdysseyVK — short guide (English)

The full manual, with the measurements behind every decision, is [GUIA.md](GUIA.md) (Spanish).
Install and overview: [README](../README.md).

## Commands

```sh
odysseyvk doctor                     # hardware, permissions and panel diagnosis
odysseyvk serve                      # the service: watches fullscreen windows (runs as a user unit)
odysseyvk ajustes                    # the floating settings panel (also from the app menu)
odysseyvk ajustes --juegos           # the panel open on the Games tab
odysseyvk integraciones [ver|escribir]  # Steam launch options per game: show them / write the pending ones
odysseyvk play video_sbs.mkv         # play a stereo file with the built-in mpv viewer
odysseyvk test                       # fullscreen calibration pattern
odysseyvk capture3d --uuid <id>      # weave the window KWin captures (what the service launches)
odysseyvk ctl red [descargar]        # the depth network: status / download it
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

- Network: Depth Anything V2 Small (open; since 1.12 the 50 MB fp16 file
  `depth_anything_v2_small_fp16.onnx`, before it the 95 MB one) in `~/.local/share/odysseyvk/modelos/`. If the
  Odyssey 3D Hub's own `mde_large.onnx` is in that folder it is used automatically (key `1`/`2`
  switches). `modelo = /path.onnx` in `serve.conf` forces another file.
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
`--force-grab-cursor`, and upscaling with FSR or NIS, which turns gamescope on). The result follows this format:

```
gamescope -w 2048 -h 1536 -W 3840 -H 2160 -S stretch -f --force-grab-cursor -- env MANGOHUD=1 ... vkdaedalus-run %command% --launcher-skip
```

- The tab's settings apply to **every installed game**; the gear on a cover (Games tab, on hover) opens that game's
  own settings, marked with a dot, and "Reset" returns it to the all-games ones.
- With Neural Forge on, `NEURAL_FORGE_TARGET_EXE` is taken from `neural-forge-cli games` (or kept from the game's line).
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
| `ignore` | system apps | app ids never offered 3D |
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
- **No depth, video stays flat** → the network is missing: `odysseyvk ctl red descargar`.
- Logs: `journalctl --user -u odysseyvk-serve -f`; the update log is
  `~/.local/state/odysseyvk/actualizar.log`.
