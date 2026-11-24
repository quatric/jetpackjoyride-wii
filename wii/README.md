# Wii port (work in progress)

Not a wrapper: the Switch port loads Android ARM64 code, which cannot run on the
Wii's PowerPC. This directory holds a from-scratch reimplementation effort that
reuses the original game's assets (v1.104.2, `assets/assets.zip` inside
`split_InstallPack.apk`).

## Asset formats found so far

| ext | status |
|-----|--------|
| `.tex` | Header `TEX\1`; no mips. Packed 4444 / 5551 / 888 / 8888 pixels, and flag `6` = ETC1 colour plane + 8-bit alpha plane (`tools/tex2png.py`). All 5323 decode. |
| `.atlas` | Plain binary, magic `0xfe000003` (parsed by `tools/atlas.py`, sliced by `tools/atlas_extract.py`). Sprites are keyed by `FileStringHash` (`tools/hashname.py`: Jenkins lookup2, init `0x805`, path lowercased, `\\`→`/`) of the original sprite `.tex` path. Those source `.tex` files are not shipped, so names are recovered from path strings in the assets and `libmortargame.so`: 1405 unique sprites, 1191 named (names recovered from path strings and by trying every known folder prefix on every name), 214 saved as `unknown/<hash>` (the earlier '2600' counted one sprite once per atlas). `SpineAnimation/*.atlas` are libgdx text atlases (skipped). |
| `.uiscreen` | Chunked binary (`FSIU`/`BSIU`/`TSIU`), screen components. |
| `bxml` (the `.xml` files, 1248 of them) | Binary XML, fully decoded by `tools/bxml.py` (string table + element/text nodes). Holds levels, laser patterns, sprite animation grids (`sprite_file`), configs. |
| `.json`, `.lua`, `.xml`, `.ogg`, `.ttf`, `.vs/.fs` | plain; shaders are GLES and must be replaced by GX TEV setups. |

## Usage

    python3 -I wii/tools/tex2png.py <extracted assets dir> <out dir>

## Findings from `libmortargame.so` (arm64, 1.104.2)

- The only AES use is save data and remote config (`Mortar::AESEncryption`, AES-128-CBC,
  key `10D8AB13CC516B87`, IV `AA778145A3498513`, ASCII strings at .rodata 0x8f3bf9 / 0x88980e).
  It does not touch `.atlas` files.
- Ghidra image base is 0x100000: Ghidra address minus 0x100000 = ELF file offset/vaddr.

## Wii libraries to use

- Input: [quatric/libwiiinput](https://github.com/quatric/libwiiinput) (`INPUT_Init`, `INPUT_Update`, `INPUT_ButtonsDown`, ...).
- Updates: [quatric/libwiiupdate](https://github.com/quatric/libwiiupdate) (`WIIUPDATE_Start`/`WIIUPDATE_Popup` for GUI apps).

## Wii app (`wii/app`) - first playable slice

Builds with devkitPPC/libogc plus sibling checkouts of libwiiinput and libwiiupdate (`make` in each, then `make` here).
Title screen (press A/B/1/2 to start), game-over screen with distance and best; hold A/B/1/2 to fly; coins come from the game's 27 standard coin patterns, chained at random heights; lasers spawn from the game's own `LazerPatterns.xml` (distance-weighted, warning then lethal, touching one resets the run); rotating/pulsing zappers (patterns and distance weights from `ObstaclePatterns.xml`, placeholder art); missiles (alert that tracks Barry, then a fast shot; constants from `MissileProbability.xml`); HOME quits. Checks GitHub for updates via libwiiupdate (no popup UI yet).

SD layout (`sd:/apps/jetpackjoyride/`): `boot.dol`, `barry.spr`, `coin.spr`, `beam.spr`, `emit.spr`, `missile.spr`, `alert.spr`, `missile.txt`, `bolt.spr`, `zappers.txt`, `levels/count.txt` + `levels/<n>/level.txt` + `lv<set>_<i>.spr` (one folder per level, the next level loads on each restart; background sets read from a real level file, e.g. `xml/LevelFiles/warehouse.xml`, with the game's parallax speed and y offset; CMPR tiles), `flame.spr` (effects/jetpack/standardflame, 4x2 grid of 16x32), `body.spr` + `head.spr` (Barry's default costume, `textures/Characters/Barry/defaultBody|defaultHead`: 4x8 grids of 32x32 frames; rows 0/1/2 used for run/thrust/fall, head offset is a guess), `font.spr` + `font.fnt` (the game's `vehicle_font`, BMFont text format; HUD shows distance and coins), `music.ogg` (any `Music_*.ogg` from the assets), and sound effects copied from `sound/` as `coin.ogg` (coin_pickup_1), `hurt.ogg` (Player_hurt_1), `laserw.ogg` (laser_warning), `missw.ogg` (missile_warning), `missl.ogg` (missile_launch), `jet.ogg` (jetpack_blast_lp, a stand-in for the default jetpack loop), `pattern.txt`, `lasers.txt`.

    python3 -I wii/tools/mkspr.py jj_sprites/Barry.png barry.spr
    python3 -I wii/tools/mkspr.py jj_sprites/CoinAlt.png coin.spr
    python3 -I wii/tools/mkspr.py jj_sprites/laserWarning.png beam.spr
    python3 -I wii/tools/mkspr.py jj_sprites/laser.png emit.spr 0 0 64 85
    python3 -I wii/tools/mkmissile.py <assets>/xml/MissileProbability.xml missile.txt
    python3 -I wii/tools/mkspr.py jj_sprites/missile.png missile.spr
    python3 -I wii/tools/mkspr.py jj_sprites/missileAlert.png alert.spr 0 0 256 64
    python3 -I wii/tools/mkzappers.py <assets>/xml/ObstaclePatterns.xml zappers.txt
    python3 -I wii/tools/mkspr.py jj_sprites/electricbolt.png bolt.spr
    python3 -I wii/tools/mklevels.py <assets>/xml/LevelFiles jj_sprites levels   # converts every fully resolvable level (34), writes count.txt
    python3 -I wii/tools/mkspr.py jj_sprites/textures/effects/jetpack/standardflame.png flame.spr
    python3 -I wii/tools/mkspr.py jj_png/textures/Characters/Barry/defaultBody.png body.spr
    python3 -I wii/tools/mkspr.py jj_png/textures/Characters/Barry/defaultHead.png head.spr
    python3 -I wii/tools/mkspr.py jj_png/font/vehicle_font.png font.spr && cp <assets>/font/vehicle_font.fnt font.fnt
    python3 -I wii/tools/mklasers.py <assets>/xml/LazerPatterns.xml lasers.txt
    python3 -I wii/tools/mkpatterns.py <assets>/patterns pattern.txt   # chains the 27 standard coin patterns

Tested in Dolphin (needs an MBR-partitioned FAT32 `sd.raw` and `[General] WiiSDCardPath` set).

### Still to do for the full game
Barry's animation (Spine/`.atlas` text + xml components), UI screens (`.uiscreen`), levels (`xml/Levels`, `LevelPacing.xml`),
obstacles/vehicles/bosses (100 Lua scripts in `scripts/`: needs an embedded Lua), more sound effects (music streams via Tremor + ASND on a thread; six effects are wired in `source/sfx.c`; `sound/*.ogg` has 1944),
save data, flag-6 `.tex` format, the unnamed atlas sprites, memory budget (88 MB RAM, ~800 MB of assets).

### Lua scripts are skins, not the game

`scripts/*.lua` (100 files, e.g. `Chicken.lua`) only drive sounds, animations, particles and HUD for a vehicle and call
`self.Base.OnUpdate`/`RegisterVehicle(name, baseClass, table)`; the physics, obstacles, missiles, zappers, bosses and
coin logic are native C++ in `libmortargame.so`. A full port therefore means re-implementing that C++ from its
decompilation (Ghidra address minus 0x100000 = file offset), with Lua only worth embedding once the native base classes exist.
Lua 5.3 sources are available locally (e.g. `MAME4Wii/mame/3rdparty/genie/src/host/lua-5.3.0`) if needed.

`make AUTOTEST=1` in `wii/app` builds a self-playing version (fake A presses) so the loop can be checked in headless Dolphin.

### Memory budget (measured)

5323 textures = 338 Mpx. Converted for GX that is ~677 MB as RGB5A3 or ~169 MB as CMPR (4 bpp), plus 56 MB of ogg: the whole
set fits on an SD card. RAM only has to hold one level's atlases at a time (a few MB in CMPR) with audio streamed, so the
"800 MB vs 88 MB" worry is a streaming problem, not a blocker. Biggest consumers: levels 136 Mpx, menus 79 Mpx, characters 30 Mpx.
The slice currently uses RGBA8 (4 bytes/px), which is fine for the handful of sprites it loads but not for a full level set.

`mkspr.py --cmpr` writes GX_TF_CMPR (`SPR1`, 4 bits/px, 1-bit alpha) and the app loads both `SPR0` (RGBA8) and `SPR1`.
The warehouse background tiles use it (32 KB instead of 256 KB each); verified visually in Dolphin. Use it for opaque or
hard-edged art; soft-alpha sprites (glows, flame) should stay RGBA8 or move to RGB5A3.

Of the 56 level files with backgrounds, 34 have every background tile resolved to a named sprite (the other 22, such as the aquarium, St Patrick's, pirate and Metal Slug variants, name tiles that are in no atlas and not on disk: that art is not in this APK, most likely downloaded as event/OTA content, so 34 is the ceiling for the base install).

## Vehicles (first prototype: Mech)

`vehicleMech.tex` is a 6x3 grid of 85 px frames (1-based, from `VehicleMech.xml`); `mech.spr` is made with
`tools/mkspr.py vehicleMech.png mech.spr`. Physics follow `Vehicles/Jetlane/Mech.xml` (gravity .36, initial boost 6,
boost .45 for 20 frames, scaled ~1.5x for 640x480). A pickup crate appears every ~15 s after 150 m; riding it gives
one free hit. The head overlay and pickup art are placeholders; the real behaviour is native code. The AUTOTEST build
starts on the Mech at 100 m so it can be checked in Dolphin (verified: ride, hop, hit -> back to Barry).

The Hog (`Vehicles/Jetlane/Hog.xml`: 4x7 grid of 64 px frames, gravity .3, boost 6.5/.6) rides the same way as the Mech;
the crate picks one of the two at random. Its fire/mount animations are not used yet.

Head placement on vehicles is real data: `Characters/Vehicles/*/Vehicle*Data.xml` lists, per vehicle frame, the head's
offset from the frame centre, rotation and head-sheet frame. `tools/mkvhead.py out.h MechData.xml HogData.xml` bakes the
default skin into `app/source/vhead.h` (rotation is applied; its sign convention and the Mech's dive frames are not verified by eye). Only vehicles have such files; Barry's own on-foot
head offsets are still a guess.

Vehicle sounds (stand-ins picked by name, not by the game's own event table): `mech_turkey_form_complete` /
`santa_hog_form_complete` on mounting, `mech_yeti_jump` / `dullahan_bike_jump` on a hop. Copy them to the app folder as
`mmount.ogg`, `hmount.ogg`, `mjump.ogg`, `hjump.ogg`. Played in Dolphin without a crash, but not listened to.

Profit Bird (`Events/ChineseNewYear/xml/Vehicles/ProfitBird.xml`: 6x3 grid of 39x54 frames, gravity .2, flap impulse 5,
JUMP_COUNT 10) is the third vehicle: every press flaps, up to 10 times before landing. Vehicles now carry their own
boost-frame count (Mech 20, Hog 12, Bird 1) and jump count. `mkspr.py` makes `bird.spr` from `vehicleBird.png`;
`mkvhead.py` takes a third file and reuses the nearest lower frame where the data is sparse. Sounds for the Bird
reuse the Hog's.

## Packaging

`wii/package.sh` makes a Homebrew Channel zip (`apps/jetpackjoyride/boot.dol` + `meta.xml`) for releases, which is what
libwiiupdate installs. The game assets are extracted from the APK and must not be redistributed, so the default zip has
none; `wii/package.sh <sd-layout folder>` bundles them for your own SD card only.

More stand-in sounds (picked by name): `dud_zapper_pop` -> `zap.ogg` when a zapper appears, `mech_turkey_death` ->
`vdie.ogg` when a vehicle is destroyed.

## .uiscreen (menus) - first findings, not decoded yet

471 files in `xml/Screens/`. Layout (little endian): `FSIU` + 4 version bytes + 0x28 bytes (hash/flags, ends `ffffffff`
x2), `BSIU` (u32 len + `"1.1.3 (1)"`-style version string), `TSIU` (counted table of length-prefixed strings: class names such
as ComponentScreen/ComponentQuad/ComponentText, property names height/position/colour/dock/..., then value strings and
instance names), then component blocks `HCIU ... MAIU BAIU ... EMIU` and a final `ESIU`. Inside a block the stream is
tagged, unaligned: `0007aaa1 <prop index>` opens a property, `0007bbbe`/`0007bbb1` and `0007dee1` frame values, `0007fff1`
ends it; floats are plain IEEE (4.0, 47.0, 238.0 in `fpscounterscreen`, which also exists as a decodable bxml
`fpscounterscreen.xml` with the same tree: position, width/height, dock, alignment, text). Two files in `Screens/` are
bxml and can be compared against the binary version to pin the value encoding. Decoding the rest is the next step
toward real menus.

More on the string table: after `TSIU` comes a count, then entries `u32 len, bytes` where len is exact (the writer pads some
names to 4, so walk by the stated length, not alignment). Class names are followed by a type word with the high bit set
(`0x80000xxx`), property names by a small type id. Component records then name their class and `Id` by table index. The
plain-XML sibling `fpscounterscreen.xml` is the reference tree (component > property name/value strings) to check a decoder
against; no decoder exists yet.

PLUS pauses and resumes during a run. The AUTOTEST build fakes PLUS at ticks 300 and 480 of each 600; verified in Dolphin (distance frozen under PAUSED, then the run continues).

`tools/uistrings.py file.uiscreen` lists a screen's string table (all 471 parse). For `Pause.uiscreen` it shows the
component classes (Window, Button, Quad, Text, Texture, TableLayout, Swipie, ...), data bindings (`this.score`,
`this.missionText`, `this.buttonCashIn`) and trigger names (`PopUpOpen`, `onScreen`, `offScreen`, `close`). Positions, sizes and
texture references live in the tagged stream after the table, still undecoded.

## Level foreground (chunks)

`LevelFiles/*.xml` also list `<chunk>` elements: slices `[u0,u1)` of the level's `*FG_n.tex` textures (the floor, crates, shelves,
ceiling trusses; glass frames in the forest) with a `probability`, an optional `yOffset` and transition flags. `mklevel.py` now
exports them as `F file w h top prob` (CMPR strips; `top = 256 - yOffset - h/2` in the game's 512-high y-up frame, so 256-high
pieces sit bottom-aligned) and the app strings them together in a fixed pseudo-random order at full scroll speed, x1.5,
frame top at screen y -7 (Barry's feet land on the floor line). Transition chunks and the per-chunk `<object>` decorations
(posters, cobwebs, vents, lights) are not placed yet. 30 of the 34 levels have foreground. Checked in Dolphin on the warehouse
(crates, shelves, trusses) and forest (glass frames); the other levels were not looked at one by one.

## Sound events: `xml/SoundFiles.xml` is the real table

201 `<AudioPackage name=...>` entries map event groups to ogg names (`MECH_PACKAGE`: mech_step_1..4, mech_jump, mech_boost_*,
mech_land; `HOG_PACKAGE`: bike_start, bike_engine_2, bike_jump, bike_land; `BIRD_PACKAGE`: bird_flap_1..3, bird_land,
bird_vox_*; `VEHICLE_GENERIC_PACKAGE`: vehicle_form_*, mech_explode; `LASER_PACKAGE`, `MISSILE_PACKAGE`, `COIN_PACKAGE`, ...).
The vehicle sounds now come from these packages (mount `vehicle_form_complete`/`bike_start`/`bird_vox_1`, jump `mech_jump`/
`bike_jump`, flap `bird_flap_1`, destroyed `mech_explode`), so the earlier "no sound-event table" gap is closed for
vehicles. Which package a game event plays is still decided in native code, so the mapping of events to packages is my own.
The zapper has no package here and keeps its stand-in. Not listened to on a real console.

Also from the table now: jetpack loop/start/stop = `TRADITIONAL_JETPACK` (jetpack_plain_*), coin pickups cycle
`COIN_PACKAGE` coin_pickup_1..3, Mech footsteps (`mech_step_1`) while running. Compiled and run in Dolphin without a crash, not listened to.

Lasers and missiles use `LASER_PACKAGE` (laser_warning, laser_start when the beam goes live) and `MISSILE_PACKAGE`
(missile_warning, missile_launch, rocket_explode_1 on impact; the missile now disappears when it hits). Run in Dolphin without
a crash, not listened to.

Gravity Suit (`Vehicles/GravitySuit/VehicleGravitySuit.xml`: 6x4 grid of 42 px frames; animations drive 1-6, fall_down 7-8,
fall_up 13-14) is the fourth vehicle: a press flips gravity between floor and ceiling and the suit is drawn upside down on the
ceiling; sound `grav_change_1`. The APK has no physics or head-placement file for it, so the gravity strength (0.9/frame) is my
own, the head is not drawn separately, and the flip/land animations are unused. Seen in Dolphin on the ceiling and flipping.

Teleporter (`Vehicles/Teleporter/vehicleTeleporter.xml`: 4x2 grid of 64 px frames, teleport_out 5-8, teleport_in 8-5, idle 1-4;
sound `CFT_activate`): the fifth vehicle. It hovers; a press vanishes (invulnerable for about 0.4 s) and reappears at a random
height. The real aiming reticle (`teleportreticle`) is not used, and the 24-frame timing is mine. Seen working in Dolphin.
The AUTOTEST build cycles through all five vehicles by level.

## Progress saving

`save.txt` in the app folder holds the best distance and lifetime coins (`"%d %d"`), written at the end of each run and read at
start; the title shows them. Checked in Dolphin: the file is created on the SD image with the run's values (read back from the
image). The title text itself was not caught in a screenshot. No shop or upgrades use the coins yet.

Zapper beam art is now real: `Entities/zapEffects.tex` (512x236) holds a glow band (y 4-55) and six arc bands; `bolt.spr` is that
whole sheet (`mkspr.py zapEffects.png bolt.spr`) and the app draws the glow plus two flickering arcs along the zapper line.
Checked in Dolphin (yellow lightning beams at several angles). The zapper end nodes (`emit.spr`) are still the laser emitters.

## RAM check for real hardware (computed from the staged files, not measured on a console)

Resident at once: app sprites 2.7 MB (RGBA8; the zapper sheet 0.5 MB and Mech sheet 0.5 MB are the largest), the current level
at most 0.93 MB (CMPR backdrops and foreground; worst of the 34 levels), decoded sound effects 1.4 MB (23 files, all mono), plus
two 8 KB-ish Tremor streaming buffers for the music (the 131 s music file is streamed, 22 MB if it were decoded, so it must stay
streamed). About 5 MB of textures and audio in total against 24 MB MEM1, so there is room, but the first run on a console is
still the real test (fat access speed and loading time between levels are unknown).

Player sounds from `PLAYER_PACKAGE`: `Player_hurt_1`, alternating `run_metal_left_1`/`run_metal_right_1` footsteps on foot,
`jetpack_land_1` on touching down. Bug fixed along the way: on the frame a run ended, a laser still overlapping Barry re-armed the
death timer, so the next run died instantly at 0 m; the timer is now cleared on restart and only armed while playing. The
AUTOTEST build appends one `run lvl .. dist ..` line per run to `dbg.txt` so this can be checked without screenshots.

`xml/vehicleProbability.xml` lists the real vehicle set (Mech, Hog, GravitySuit, Teleporter, ProfitBird, Dragon, JetSki, Sled,
Delorean, HoverBoard, Starship, Ecto, IceSkates; `spawnRange` 650) and `Vehicles/*.xml` give per-vehicle `<probability>` (Mech 20, Hog 20,
ProfitBird 50). Crate spawns now pick by those weights (20 where the APK has no file). The weighted pick compiles but the
AUTOTEST build forces a vehicle, so it was not exercised in Dolphin; the other eight vehicles are still not implemented.

Title screen uses the game's own logo art: `textures/logo/default/logo_text.png` (downscaled to 360x218) and `logo_barry.png` (192x270),
made with `mkspr.py` into `logot.spr` / `logob.spr`; the app falls back to text if they are missing. Checked in Dolphin.
Capturing Dolphin for checks: `wii/tools/winid.swift` (`swiftc -O winid.swift -o winid`; `screencapture -x -l $(./winid <pid>)`)
grabs only Dolphin's own window; a fixed screen region is unsafe because it can include other apps.

## One-command data build

`wii/make_sd.sh <assets> <png> <sprites> <out>` rebuilds the whole app data folder (all `.spr`, the lasers/missile/zapper/pattern
tables, the 34 levels with foreground, the title logo and the 26 sound files) from the extracted APK, replacing the hand-run
commands listed earlier in this file (those are out of date). Checked by building into a fresh folder and comparing it
byte for byte with the folder used for the Dolphin runs: identical apart from `boot.dol`. Steps before it: `tex2png.py <assets>
<png>` and `atlas_extract.py <assets> <sprites> [libmortargame.so]`.

## CI

`.github/workflows/wii.yml` builds the app in the `devkitpro/devkitppc` container on every push that touches `wii/` (installs
libvorbisidec/libogg, clones and builds quatric/libwiiinput and libwiiupdate, runs `make -C wii/app`, then `wii/package.sh`) and
uploads the asset-free Homebrew Channel zip as an artifact. The first run passed. It does not create a GitHub release, so
libwiiupdate has nothing to install yet; attaching the zip to a tagged release is left for when a build has been tried on a console.

`python3 -I wii/tools/test_tools.py` checks the converters without the APK (hash normalisation; SPR0/SPR1 headers, padding, size and crop).

### .uiscreen tables, decoded

`tools/uistrings.py` now parses the tables after `TSIU` as eight counted groups (`u32 n`, then n entries of `u32 len + len bytes`:
NUL-terminated text and 0-3 type-id bytes, e.g. `position` carries `f8 3c 00`). All 471 files parse into eight groups with the same
roles: 0 aliases (`~default`), 1 component classes (74 distinct names across all screens), 3 events/triggers, 4 property names
(135 in Pause), 5 and 7 unknown, 6 instance ids (251 in Pause). The component stream after the tables (`HCIU` records with tagged,
unaligned property data) is still undecoded, so no positions, sizes or textures yet.

`tools/uischema.py <assets>/xml/Components <Class>` prints the ordered property schema of a UI component class (base chain first,
with type and default): `ComponentFPS` has `id, comment, isPickable, updateMask, ... scale, rotation, position, origin, dock ...`;
`ComponentText` has 114 properties. All 91 `Component*.xml` definitions load. These types and defaults are what the component
stream in a `.uiscreen` has to be decoded against; the stream's `0007aaa1 <n>` record numbers did not map directly onto these
indices in a first try on `fpscounterscreen`, so the stream layout is still open.

### .uiscreen components, decoded (471 of 471 files)

`tools/uidecode.py x.uiscreen` lists the components of a screen with their class, id and property values. The stream, as found by
comparing `fpscounterscreen.uiscreen` with its XML twin (identical positions 4,4,0 / 123,3,0, sizes 238x47 / 60x41, texts FPS and FPI,
alignments, dock Fill/None, colour 2,2,2,45): each component is `HCIU 0 <id> 0xcb980001`, a `TAB 2 5 <id> <hasParent> <class>
<sibling> 0` header and `TAB 2 1 0 0xcb980b91 EMPS`, then ten typed slots (`0x7aaa1 <type>`, empty `0x7bbbe` or `0x7bbb1 <n>` followed by n
`TAB 2 2 <property name index> ...` entries, closed by `0x7fff1`), then `MAIU BAIU 0 EMIU 0 0 0 0xcf980001`. Slot types: 1 bool, 2 enum
string, 3 vector3, 4 vector2, 5 float, 6 rotation, 7 int, 8 colour (4 floats), 10 text; strings are `u32 len + bytes`. Trigger and event
sections between components (`HTVE`, `CEIU`, `FEIU`, SetProperty actions) are skipped, not decoded, and nesting is only available as the
`hasParent`/`sibling` fields, not yet turned into a tree. All 471 screens decode to the end (about 253k properties; 797 of them are kept as raw word lists). Entry ends are found by byte search for the next `TAB 2 2`, since strings are not 4-aligned. Texture properties (`texture`, e.g. `textures\Hud\...tex`), position, size, colour and text decode now; button actions live in triggers that are not decoded, `hasParent` is an index into the component list, so `uidecode.children()` gives the tree. In Pause.uiscreen the Window and most content start hidden (`visible` false) and are switched on by triggers (PopUpOpen etc.), so a menu cannot be drawn without decoding those; no menu is drawn from this yet.

### Hoverboard

Sixth rideable vehicle (`hover.spr` from `HoverBoard/hoverboard.png`: a 4x3 grid of 34x47 frames with the rider baked in, row 0 riding, rows 1-2 airborne). Its physics reuse the Hog's numbers and its crate weight is 20; both are guesses, since the real behaviour is native code. Checked in Dolphin with the fake-input build.
