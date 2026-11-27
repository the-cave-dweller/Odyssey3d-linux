# OdysseyVK — glasses-free 3D on the Samsung Odyssey 3D (G90XF) for Linux

**English** · [Español](README.es.md)

OdysseyVK drives the Samsung Odyssey 3D G90XF (lenticular lens + eye tracking) on **Linux with
Wayland** (KDE Plasma). It turns the lens and the monitor's 3D mode on, weaves the two views for
your eyes, follows your head with the monitor's own IR camera, and turns everything off again when
you leave. Samsung only ships software for Windows; this is a from-scratch C++/Vulkan port.

What you get:

- **Side-by-side and top-bottom videos** in 3D, from any player or browser in fullscreen.
- **Normal 2D video → 3D** with a depth network that runs on your GPU (open model, downloaded once).
- **Games** from your Steam library, each with its own 2D/3D switch.
- A floating **settings panel** over the desktop or the video, in **7 languages** (English,
  Español, Français, Deutsch, 中文, 日本語, Русский), updates from this repository and a one-click
  uninstall.

![The pill and its relief menu](docs/img/pill.png)

## New in 1.18

- **Monitor more than FPS.** The arrow on the *FPS box* tile opens a list, MangoHud-style: each GPU on its own row
  (usage, plus temperature, memory and power if ticked), CPU usage and temperature, system memory and the game's
  frame time. The box shows in the top-right corner once the pill hides, over games only. It says «DLSS» only while the depth really comes from DLSS.
- **The 3D no longer flickers when you open or close the panel**, and the browser no longer pauses the video: the
  viewer used to minimize itself whenever it lost focus.
- **The «game closed» card** only shows if the game closes within 20 seconds of entering 3D (a crash), not when you
  quit it.
- **A quiet install**: `install.sh` shows a few lines; the details go to `build/compilacion.log`.
- **The DLSS depth switch works straight away.** Turning off *Use DLSS depth* goes back to the depth network on the
  next map (it used to wait for the next video) and the game stops copying its depth; turning it on brings the game's
  depth back just as fast. *Performance* mode never uses it: no network and no DLSS.
- **The heavy part goes to the second card.** With two cards, the depth network now runs on the one that is not
  drawing the 3D and the game, on its own (`red = auto`, the default; a number still pins one). If it could go to
  more than one, the viewer times each with the network that is running and keeps the one that costs the game least,
  until you switch networks.
- **Frame generation from the FPS box.** A right-click on the FPS box opens MAKO's frame generation for the game
  underneath, like vkDaedalus's menu: on or off, the model (LSFG or MAKO FG), x2 to x4, flow and performance mode. It
  writes the game's profile in MAKO's `conf.toml` (backed up first, checked with `mako-cli validate`), which MAKO
  reads on the fly; with DLSS depth the box adds what reaches the screen. MAKO's layer goes in Integrations
  (`ENABLE_MAKO`).
- **An FG tab.** A new top tile, FG, shows the running game's MAKO multiplier (X2, X3, X4); inside are MAKO FG's
  install button and the game's MAKO settings.
- **Frame generation for videos.** In the FG tab, *Turn on for videos* with x2, x3 or x4: the 3D view itself asks MAKO FG for
  in-between frames of a video (not games), about 2 ms a frame on an RTX 4090; the video runs ~21 ms behind at x2.
- **MAKO FG with one button.** When MAKO's own frame generator (`mako-fg-host`) is missing, the menu and the FG tab
  offer *Install MAKO FG · 19 MB*: MAKO FG V1.24 from Hugging Face, pinned and SHA-256 checked, into
  `~/.local/share/odysseyvk/mako-fg` without root; games with MAKO get it through `MAKO_FG_HOST` in their Steam line.
  Also `odysseyvk mako estado|instalar|quitar`. MAKO FG is non-commercial (CC BY-NC 4.0).
- **Real and generated frames in the FPS box.** Like vkDaedalus, a second tiny layer counts the frames below MAKO
  while the OdysseyVK layer counts them above it, so the box shows *Real* and *Generated* fps
  (`ODYSSEYVK_CONTAR_MAKO=1`, added by Integrations next to `ODYSSEYVK=1` and `ENABLE_MAKO=1`).
- **Game keys.** Over a game only: `Insert` hides or shows the pill, `Delete` switches the depth between DLSS and the
  chosen model, `End` turns MAKO's frame generation on or off, `Page Up` / `Page Down` give x2 → x3 → x4 and back
  (all changeable in Keyboard shortcuts). `End` is also vkDaedalus's menu key: KDE takes it while the viewer is over a game.
- **Each game's frame generation in its Integrations** (its MAKO profile, by its executable), and with the frame time
  ticked the FPS box shows *Real*, *Generated* and *On screen* without DLSS too. The box now shows only when the pill is
  hidden.
- **Neural Forge off after a crash.** With Neural Forge, Integrations runs the game through `odysseyvk juego`: if the GPU
  reports an Xid in the game's processes, Neural Forge is turned off for that game and the game reopens without it; it
  shows off in that game's Integrations until turned back on.
- **ReShade effects, in the 3D viewer.** An **Effects** tab next to Games and an effects icon on each game's cover:
  ReShade `.fx` effects from `~/.config/vkdaedalus/reshade` run on every game frame before the 3D, with the 3D's depth
  as ReShade's depth buffer (so depth effects work in any game). Each effect has its toggle pill and its sliders; all
  games share one set and any game can have its own (`~/.config/odysseyvk/effects/`). *Preview*, next to the tab's
  name, moves the list to a column on the left and clears the rest, to tune them with the game in view. *Add an
  effect* opens a full-width picker split into effects that use depth and those that don't, with hover-to-scroll
  buttons. *Public sources* (ReShade, SweetFX, qUINT, prod80, AstrayFX...) download only the files you don't have,
  compared by content against GitHub: files you already have by name are kept and never replaced, and downloaded files that
  end up duplicated are deleted; a source with nothing new is greyed out. The compiler is ReShade FX
  (crosire, BSD-3) as patched by vkDaedalus.
- **A test on every layer change**: another network, DLSS depth coming or going, or another DLSS preset in the game
  gets a 3-second measurement of what runs (game FPS, maps per second, lost refreshes, how busy each card is) in the
  log.

| | Before | 1.18 |
|---|---|---|
| Opening or closing the panel over a video | 3D view rebuilt, video paused | nothing happens to the video |
| Clicking the video | same | the 3D is back on top in ~20 ms |
| Reading 3 GPUs, CPU and RAM for the box | — | 0.9 ms, once a second |
| Compiler warnings while installing | 561 | 0 |

![The FPS box tile menu and the box with what it monitors](docs/img/monitor.png)

### New in 1.17

- **Watch the depth while you choose the network.** The *Debug view* checkbox in the 3D tab shows the depth live and
  shrinks the panel to a column on the left. Switch networks and see the difference straight away; next to the
  checkbox, what the network costs, updated every second.
- **Use any depth network.** The 3D tab has two lists side by side: the usual networks, and networks for videos only.
  Drop any depth `.onnx` into `~/.local/share/odysseyvk/models` and it shows up; a list with more than two becomes a
  drop-down.
- **A network that looks at time (experimental, videos only):** Video Depth Anything Small looks at the last 32
  frames, so the depth jumps less from one frame to the next. It spots scene cuts in the picture, so a new shot
  starts clean.
- **AMD cards** can run the 2D→3D network through ROCm (not tested on AMD hardware yet).
- **The first time an app goes fullscreen,** a small popup asks 3D or 2D and remembers it.
- **Lighter on NVIDIA:** 30 MB less GPU memory per network, and with two GPUs the other card no longer loses 386 MB.

Measured on an RTX 5080:

| | Download | Time per depth map | GPU memory | For |
|---|---|---|---|---|
| Depth Anything V2 Small FP16 (recommended) | 50 MB | 4.3 ms | 640 MB | videos and games |
| Depth Anything V2 Small FP32 | 99 MB | 7.3 ms | 1 GB | the CPU, where it's 5× faster than FP16 |
| Video Depth Anything Small | 117 MB | 122 ms | 2.8 GB | videos only |

| Video network at a scene cut | Before | Now |
|---|---|---|
| Depth of the new shot | up to 20 % off, for 2 to 7+ seconds | right from the first map |
| False cuts in a 7-second talking-head clip | 1 or 2 | none |

![The two network lists in the 3D tab](docs/img/networks.png)

![Debug view: the depth live, with the lists on the left](docs/img/debug-view.png)

## Install

Arch / CachyOS, everything in one go (an NVIDIA GPU with CUDA, or an AMD one with ROCm, for the 2D→3D converter;
the rest works on any Vulkan 1.3 GPU):

```bash
sudo pacman -S --needed cmake gcc pkgconf git glfw openssl libpng freetype2 vulkan-icd-loader vulkan-headers spirv-headers glslang wayland plasma-wayland-protocols libpipewire systemd-libs mpv onnxruntime-cuda cuda cudnn
```

With an **AMD** card, swap `onnxruntime-cuda cuda cudnn` for `onnxruntime-rocm` (not yet tested on AMD hardware): the converter then runs on the GPU through ROCm, with ONNX Runtime's MIGraphX engine. The first time in
each session the network is prepared for the card, which can take a while. If the processor has integrated Radeon
graphics, the network goes to the card with more memory (`HIP_VISIBLE_DEVICES` pins one). Without a GPU engine the
network runs on the CPU. The DLSS depth of games is NVIDIA-only.

```bash
git clone https://github.com/the-cave-dweller/Odyssey3d-linux.git && cd Odyssey3d-linux && ./install.sh
```

`install.sh` compiles (quietly: the details go to `build/compilacion.log`, shown if something fails), asks for your
password once (binary, udev rule and calibration data), installs the menu entry and starts the user service. Then check that the monitor is seen:

```bash
odysseyvk doctor
```

`doctor` must list the lens (`/dev/ttyACM0`), the scaler (`/dev/sgN`, vendor `GCREADER`), the
camera (`04e8:20d7`) and the panel in 4K. If it doesn't see them, **unplug and plug the monitor's
USB-B cable** once (the udev rule applies on reconnect). Never run `udevadm trigger` on the
monitor: it resets its internal hub and the lens, scaler and camera disappear until you replug.

The first time the service starts without a depth network, the settings panel opens by itself on its 3D tab, which
lists the networks with a checkbox each: Depth Anything V2 Small from
[huggingface.co/onnx-community/depth-anything-v2-small](https://huggingface.co/onnx-community/depth-anything-v2-small)
in **FP16** (recommended: the same map as the FP32, faster and lighter) or **FP32**. One that isn't downloaded yet is
grey: clicking it downloads it and sets it; downloaded ones carry a blue tag, and the ticked one is the one in use.
Any other `.onnx` depth network you put in `~/.local/share/odysseyvk/models` shows up in the same list. When the
first download finishes the panel closes and the network loads. From a terminal it is:

```bash
odysseyvk ctl net download        # FP16
odysseyvk ctl net download fp32   # FP32
```

When the network has to run on the processor (no GPU engine, or no room on any GPU), OdysseyVK uses the FP32 instead
of the FP16 if it is downloaded: on a CPU it is about 5 times faster (298 vs 1,447 ms per map, measured by
[Noa3](https://github.com/Noa3) on a Ryzen 9 7950X3D). With two GPUs, `red = N` in `serve.conf` picks CUDA device N.

**Videos only (experimental):** to the right of the list, *Videos only* offers
[Video Depth Anything](https://github.com/DepthAnything/Video-Depth-Anything) Small (a community ONNX export of the
Apache-2.0 model: there is no official one). It looks at the last 32 frames instead of one, so the depth jumps less
from frame to frame (about half the near/far reordering on a landscape, somewhat less on a talking head). It costs
far more GPU work (see the table above), still within the viewer's 6 maps per second, and doesn't run on the CPU.
Ticked, it's used for videos only; games keep the regular network. Scene cuts are found in the picture before the
network runs, so a new shot starts clean. Any `.onnx` with «video» in its name goes to this list. From a terminal:
`odysseyvk ctl net download video`. With more than two networks on either side, that side turns into a drop-down.

**Try networks live.** *Debug view*, the checkbox above the two lists, turns on the viewer's depth debug view (the
same as `Ctrl+Shift+9`, below) and clears the panel down to a column on the left: the usual networks in the top half,
the video ones in the bottom half, and the video visible beside it. Next to the checkbox the panel shows what the
running network costs, refreshed every second: milliseconds per map (the network alone and in total), maps and frames
per second. Pick another network and watch the depth change; untick the box to get the whole panel back. It needs a
video or game open in 3D.

![First run: the panel asks for the depth network](docs/img/first-run.png)

## Use

Put a video in fullscreen. The first time with each app (a browser, a player), a popup in the top-right
corner, in the pill's style and its mode colour, asks **3D** or **2D**, with **Remember for this app** ticked;
nothing changes on the monitor until you choose. 3D starts the viewer; 2D leaves the video as it is, and remembered,
the app is left alone from then on (both can be undone in 3D → *Apps that don't ask*). With 3D, in its first
seconds the viewer tells a
**normal video** (one image, converted to 3D) from a **split** one (side by side or top and bottom),
switches by itself and stops looking. It compares the two halves of the frame — the picture and its
fine detail — and was measured on three side-by-side 3D videos and three normal ones (18 of 18
three-second windows right on each). Apps marked as 2D and games keep their own way.

The pill in the top-right corner, as tall as the panel's title bar, stays while the 3D is on: a round power button
(3D on/off), the **Neural | Performance** switch (depth with the network or from the image on the CPU, switched on
the spot), the relief button (**Auto** or **Manual**, see below; in Manual its menu also has Depth and Pop-out), the
two video-type buttons (normal and split, to switch by hand) and the gear that opens the settings panel. Resting the
mouse on it shows the shortcuts. It hides by itself after 5 seconds without the mouse near it; moving the mouse to
the top edge of the screen, where it was, brings it back. Without the converter (a split video shown as it is, or no
depth network) the relief button is Manual only.

Keys (they also work while the video or game has the keyboard):

| Keys | What |
|---|---|
| `Ctrl+Shift+1` | 3D on / off |
| `Ctrl+Shift+3` / `4` | 3D depth down / up |
| `Ctrl+Shift+5` / `6` | 3D pop-out down / up |
| `Ctrl+Shift+F1` | hide / show the pill |
| `Insert` (over a game) | hide / show the pill |
| `Delete` (over a game) | depth: DLSS or the chosen model |
| `End` (over a game) | MAKO frame generation on / off |
| `Page Up` / `Page Down` (over a game) | more / fewer generated frames (x2 to x4) |
| `Ctrl+Shift+9` | depth debug view (flat, no 3D): the depth in gray (white = near), where saliency weighs in orange (it picks the screen plane in Auto) and what sits on the screen plane in cyan; again to go back |
| `Esc` (in the viewer) | leave 3D |

Every shortcut can be changed in the panel's main page → **Keyboard shortcuts** (the section folds with the arrow
on its header, like *Apps that don't ask*; both start folded): click it and press the new
combination or a single key (Esc cancels, the circular arrow puts the factory one back). A single
key, or one with only Shift, works too: while the 3D is on, that key no longer reaches the video or the game, and the
panel says so when you set it. Any letter, digit or sign, F1–F24, the arrows and navigation keys, Space, Pause/Break,
Print Screen, Scroll Lock and Menu. While the settings panel is open it keeps the whole keyboard, so the game underneath gets
no keys; Esc closes it.

![Keyboard shortcuts in the settings panel](docs/img/shortcuts.png)

The settings panel opens from the gear on the pill, the OdysseyVK entry in the app menu, its
taskbar icon or the system-tray icon (click: settings, middle click: games). It fills the screen in the
style of vkDaedalus's menu, at the same size on the desktop and over a game: a title bar with the close cross; in the middle a large tile per tab — **3D** (video,
screen, the depth network, keyboard shortcuts, apps that don't ask), **Games**, **Integrations** (each game's Steam
launch options; see below) and **Settings** (the performance figures — 3D viewer, GPUs — and the rest:
smoothness, head-tracking anticipation, updates, taskbar and tray, and **Uninstall** folded under its arrow) —; and
on each side three small tiles that work at a click: DLSS depth, the game's HUD, head tracking, the FPS box, the
version (check for an update or install it) and the language. Each option takes one row: its explanation shows when
the mouse rests on it. Inside a game Integrations is grey and in Games only that game is in colour; over a video
Games is grey too. The colours follow the mode: blue with the network, amber in Performance. Everything is also in
`~/.config/odysseyvk/serve.conf`; the service picks up changes without restarting.

### Language

The **Language** tile switches the language at once: the panel, the pill, the cards and
the tray tooltip. English is the default. Chinese and Japanese use the Noto Sans CJK font
(`noto-fonts-cjk` on Arch).

## What makes the 3D good

**Depth from a single image.** For normal videos a depth network looks at each frame and tells
how far every pixel is; the viewer then shifts the image for each eye accordingly. The near and
far limits are the ones Samsung's own software uses, so nothing goes beyond what the lens can
show cleanly.

![A frame and its depth map](docs/img/depth-map.png)

**Edges in the right place.** The network's map is coarse (518 points across a 4K frame) and its
edges land a few millimetres away from the real outline. OdysseyVK refines every edge against the
colour of the image, per pixel, so each object's border is where you see it in both eyes. Measured
on real frames: the edge is within 2 pixels of the picture for 96 % of edges (it was 26 pixels
off before), with no ghost of the background beside it.

![Edges before and after the refinement](docs/img/depth-edges.png)

**Depth and pop-out are remembered.** Normal video and smoothed split video start at Depth 75
(the screen plane fixed at 0.75) and Pop-out 50; plain split video starts neutral (50 and 0). Move
either slider and that value is remembered for that kind of video, each one on its own.

**The subject can stay on the screen.** With `profundidad_conv = auto` the plane of the screen is
placed on what you are looking at, as Samsung's Hub does, so faces and subjects stay sharp and the
background opens behind. In
green, what sits on the screen plane (measured with 1.07 on real frames): without saliency the
plane fell on the space behind the ship (0.36); now it is the ship (0.95). On the globe it moves
from the back of the frame to the globe's rim (0.27 → 0.39).

![Subject plane before and after](docs/img/subject-plane.png)

**See what the 3D is built from.** `Ctrl+Shift+9` swaps the 3D for a flat view of the depth the
viewer is using, refined edges and anchored subtitles included: gray is the depth (white = near),
orange is where saliency weighs (in Auto it picks the screen plane; the brightest orange is what
weighed most in that frame) and cyan is what sits on the screen plane, the part that stays still and
sharp. A line under the pill explains the colours; press it again to go back to 3D. It needs the
converter (a normal video or a smoothed split one) and works in Neural and Performance alike. Below,
the same ship frame: saliency covers the ship and the screen plane runs along its hull.

![The depth debug view of a frame](docs/img/depth-debug.png)

**Split videos that are too deep get tamed.** Some side-by-side videos separate near objects by
several centimetres, more than the lens can show. "Smoothed split video" uses the left image and
the converter's relief instead, automatically when needed.

**The player's controls stay whole.** In a split video the player paints its bar across both
halves. OdysseyVK finds those rows and shows the controls once, over the video, which goes on
soft and dimmed behind them; when the bar hides, the cut goes away with it.

![The player bar over a split video](docs/img/player-bar.png)

**Games.** The Games tab lists your Steam library with covers; each game has a 2D | 3D switch.
Launch from the tab and the game enters 3D on its own when it goes fullscreen. With `modo = flat` in
`~/.config/odysseyvk/serve.conf`, fullscreen windows and games that aren't switched on stay in 2D (the
switches show it). Games must run in
**borderless window** mode (also under gamescope): in exclusive fullscreen the desktop puts the
game above the 3D. If a game in 3D covers the viewer or closes within 20 seconds of entering 3D,
OdysseyVK says so with a card (the one about a game covering the 3D closes itself after a
4-second countdown on its button). Hovering a cover shows a gear in its top-right corner: it
opens that game's [integrations](#integrations-steam-launch-options).

![Games tab](docs/img/games.png)

![The card a game gets when it closes in 3D](docs/img/card.png)

## Keeping it up to date

The service checks this repository every six hours. With a new version, a card appears in the
corner with **Update**, **Later** and a small "Don't notify about updates"; in the panel, the **Version** tile
turns its arrow amber and says which version is out.
Clicking the tile installs it, or checks again when there is nothing new. Update downloads, builds, installs
(a window asks for your password), restarts the service and reopens the video or game that was
in 3D. The **Settings** tab also has "Update automatically" (off by default) and **Uninstall**.

![Settings tab and the update card](docs/img/settings-program.png)

## Configuration

`~/.config/odysseyvk/serve.conf`, one `key = value` per line. The ones you may want:

| Key | Meaning |
|---|---|
| `idioma = en \| es \| fr \| de \| zh \| ja \| ru` | interface language (English by default) |
| `modo = ask \| conv \| sbs` | ask with the card / always normal video / always split video |
| `auto = app:conv` | an app that goes straight to 3D (the card's "Don't ask again" writes these) |
| `pantalla = DP-4` | which output is the Odyssey |
| `rastreo = si` | follow your eyes with the monitor's IR camera (Settings → Head tracking) |
| `relieve = 1,00` | strength of the converter's relief (0,5 … 2) |
| `dividido_suavizado = si \| auto \| no` | smooth split videos: always / when needed / never |
| `crosstalk_base = 0,013` | how much of the other eye's image to subtract (ghosting) |
| `filtro = 10` | slope of the weave filter between the two views: the change from one view to the other takes 1/slope of the cycle (Samsung's 10 is almost a step; the panel's "Smooth between the views" writes it) |
| `profundidad_conv = 75` / `salida_conv = 50` | starting Depth and Pop-out for normal video (`_suave` smoothed split, `_sbs` split); written when you move the sliders; `profundidad_conv = auto` = automatic screen plane |
| `hz = 165` | panel refresh rate in 3D (default 120; the G90XF also runs at 165) |
| `vrr_en_3d = si` | keep variable refresh (VRR) during 3D; by default it is switched off while the 3D runs (with it on, games below the refresh rate stuttered) |
| `atajo_3d = Ctrl+Shift+1` | a shortcut (the panel writes these): `atajo_profundidad_menos`, `atajo_profundidad_mas`, `atajo_salida_menos`, `atajo_salida_mas`, `atajo_pastilla_menos`, `atajo_pastilla_mas`, `atajo_pastilla` |
| `barra_tareas = no` / `bandeja = no` | hide OdysseyVK from the taskbar / the system tray |
| `actualizaciones = no` / `actualizar_auto = si` | no update card / install updates automatically |
| `actualizaciones_cada = 6` | hours between checks of this repository |

Numbers use a decimal comma in this file. Everything else (lens pitch calibration, head tracking,
workshop variables) is in the guides.

## Relief: Auto and Manual

In normal video with the network the pill turns light blue: next to the switch, a button with a chip
says **Auto** or **Manual** and opens its menu.

- **Auto**: in each scene a saliency map finds what draws the eye (a face, an object) and puts it on
  the screen plane. The depth reach follows the distance of your eyes (0.3° of parallax
  between the eyes, never below the factory limits: those up to 75 cm, 1.33 times them at 1 m), and the pop-out is worked out for the most
  relief that fits without flattening the image against those limits (measured: 0.9 % of the frame on
  the limits instead of 61 % with a fixed pop-out).
- **Manual**: Depth and Pop-out by hand, with the same automatic reach.

Subtitles and captions stay on the screen plane, each line in one piece. Only text lines count:
the top and bottom bands where subtitles and tickers live, and in the middle of the frame only a
real line of text, so a face, a T-shirt print or a city full of signs keeps its depth. In games
(Steam or gamescope windows) the bands only take long subtitle lines, so a third-person character
at the bottom of the screen isn't flattened. On thin edges the depth
step follows the colour of the picture, and what an object uncovers when it shifts is filled with the
background next to it.

## Performance mode

Depth without the network (`exp_sin_red = si`, the **Neural | Performance** switch on the pill,
switched on the spot), worked out from the image itself on the CPU (~9 ms per map, no CUDA, and without the network's
~1.1 GB on the GPU). It is rougher than the network but steadier. The pill turns amber and carries a
lightning-bolt button: its drop-down menu has three measured presets (**Gentle · Medium · Intense**)
and four fine sliders (mix, strength, reach, head parallax), all applied at once; hover over each to
see what it changes. In this mode variable refresh is left as the desktop has it.

## Exact depth in DLSS games

In games that use DLSS, OdysseyVK can use the game's own depth instead of guessing it with the
depth network: the depth buffer the game hands to DLSS every frame, exact and steady, at the game's
render resolution (in Cyberpunk 2077, 1920×1080). It needs an NVIDIA card, DLSS on in the game (any
mode) and `ODYSSEYVK=1` in the game's Steam launch options. Switch it on for every game with the box at the bottom of the
panel's Games tab (hover it for a reminder), or for one game with the gear on its cover (Integrations); games
where you removed it one by one stay that way and OdysseyVK writes it into Steam (with Steam closed; with it open, as
soon as it closes); or add it by hand:

```
ODYSSEYVK=1 %command%
```

That loads OdysseyVK's small Vulkan layer into that game only (`install.sh` puts it in place). The
layer finds the depth DLSS receives and, while the 3D is on, the game's GPU writes it straight into
shared memory for the viewer; with the 3D off it copies nothing. When the viewer is over that game
it uses it instead of the network (the viewer's log says `[red] la profundidad del juego`); if the
layer isn't there or the depth stops arriving, it goes back to the network by itself. Measured in
Cyberpunk: the layer's depth is bit-for-bit the one DLSS gets (checked against Neural Forge's dump of
the same frame). Ctrl+Shift+9 shows it.

![The depth the network guesses and the game's own](docs/img/depth-dlss.png)

The layer also finds the image the game draws its HUD into (in Cyberpunk, a 4K layer cleared to
transparent every frame) and hands its alpha to the viewer: the minimap, ammo, mission text and menus
stay flat on the screen instead of taking the depth of whatever is behind them. If a game draws its HUD
some other way nothing changes. When the pill hides, a small box in the top-right corner shows the game's FPS
(frames it presents) and the layer's (depths it sends; with DLSS frame generation the game's are double). The arrow on the *FPS box* tile picks more to show in it, MangoHud-style: each
GPU on its own row (usage, plus its temperature, memory and power if ticked), CPU usage and temperature, system
memory and the game's frame time (with it ticked and no DLSS, *Real*, *Generated* and *On screen*). Settings → Games
with DLSS turns each of the three things off: the DLSS depth, the flat HUD and the FPS box.
Settings → Smoothness → *Layer FPS cap* limits how many depths and HUD masks per second the layer copies
inside the game (the viewer makes 6 depth maps a second by default), so the game spends less on copies
nobody uses; it applies at once, as an exact average.

## Integrations: Steam launch options

The panel's **Integrations** tab puts together each game's Steam launch options from pieces
instead of typing them: the layers (`ODYSSEYVK`, `MANGOHUD`, `ENABLE_VKDAEDALUS`, `NEURAL_FORGE_ENABLE`,
`ENABLE_VKBASALT`), other variables, `vkdaedalus-run` or any command in front of the game, options after
it, and gamescope (render and output resolution by abbreviation, stretch or fit, fullscreen, cursor grab,
and FSR or NIS upscaling, which turns gamescope on). What you set on the tab applies to every installed
game; the gear on a cover (Games tab) opens that game's own settings, marked with a dot. With Neural Forge
on, the game's `NEURAL_FORGE_TARGET_EXE` is taken from Neural Forge itself (a game missing from Neural Forge's list
says so: add it in its Setup tab), and a switch below it puts Neural Forge's model on the second card
(`NEURAL_FORGE_OFFLOAD_GPU`, written with that card's PCI IDs and address), with the card's PCIe link: at x4 or
x8 every frame crossing to it and back can make the game slower than keeping the model on the game's card. With
two dedicated cards, *Game's card* sends the game to one of them (`DXVK_FILTER_DEVICE_NAME`,
`VKD3D_FILTER_DEVICE_NAME`, `MESA_VK_DEVICE_SELECT`); when that is not the card the 3D monitor is plugged into, it
says which link every frame crosses. The 3D view always runs on the 3D monitor's card.

![A game's integrations](docs/img/integrations.png)

OdysseyVK writes the whole line into Steam's configuration. Steam only reads it at startup and rewrites it
when it quits, so with Steam open the change waits and the service writes it as soon as Steam closes, after
saving a copy of Steam's file in `~/.local/state/odysseyvk/steam/`. The lines your games already have are
read first, and so are later changes made in Steam or by Neural Forge: they become that game's own
settings, so nothing is lost. A line that can't be split into pieces (with `;`, pipes or redirections) is
left alone. Details in the [guide](docs/GUIDE.md#integrations-steam-launch-options-since-115).

## Games and the cursor

When a game enters 3D the pill shows for a few seconds, so the shortcuts
can be read, and then hides by itself. While you bring it back with `Ctrl+Shift+F1` it takes the
cursor from the game (the game stops being the active window, so it lets go of a locked cursor);
hiding it again, or clicking anywhere on the game, gives the cursor back. The settings panel does
the same while it is open over a game: a click outside it closes it and returns control, unless you
pin it with the pin left of its name: pinned, it stays on top and a click on the game just gives the
game the mouse and keyboard back.

## Documentation

- [docs/GUIDE.md](docs/GUIDE.md) — the short guide in English: every command, every key, troubleshooting.
- [docs/GUIA.md](docs/GUIA.md) — the full manual in Spanish, with the measurements behind each decision.
- [CAMBIOS.md](CAMBIOS.md) — what changed in each version (Spanish).

## Hardware this was built on

Samsung Odyssey 3D G90XF, KDE Plasma 6 on Wayland, NVIDIA RTX 4090 and RTX 5080 (CachyOS). The lens model
constants come from a G90XF; the fine pitch of your unit is tuned with `rampa_x` / `rampa_y`
(see the guide). The monitor's USB-B cable must be connected: lens, scaler and camera go through it.

## Credits

Built on the work of [macdissey-3d](https://github.com/Fulviuus/macdissey-3d) (macOS) and
[Linux_3D_SBS_Player](https://github.com/m-j-tilley/Linux_3D_SBS_Player) (lens model), with
[Depth Anything V2](https://github.com/DepthAnything/Depth-Anything-V2) for depth and
[YuNet](https://github.com/opencv/opencv_zoo) for face detection. MIT license.

The panel is styled after [vkDaedalus](https://github.com/Daaboulex/vkDaedalus), so both feel the same in a game,
and uses its typeface, [Barlow](https://github.com/jpt/barlow) (SIL Open Font License 1.1, see
`assets/fuentes/OFL-Barlow.txt`), built into the program.

The screenshots are made with `python3 tools/capturas-readme.py`.
