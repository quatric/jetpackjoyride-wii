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
    python3 -I wii/tools/mklevels.py <assets>/xml/LevelFiles jj_sprites levels   # converts the 11 levels of the regular rotation, writes count.txt
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

### Ice Skates

Seventh vehicle (`skate.spr` from `IceSkates/iceskates.png`, a 4x4 grid of 38x50 frames, rider baked in; row 0 skating, row 1 airborne). Physics are the Hog's, a guess. Checked in Dolphin.

### Ecto

Eighth vehicle (`ecto.spr` from `Ecto/Ecto/ecto.png`): the sheet is one static 256x128 frame with no rider, so no animation and no head. Physics are the Hog's, a guess. Checked in Dolphin.

### DeLorean

Ninth vehicle (`dlr.spr` from `delorean/delorean.png`): one static 157x62 frame, no rider; the glow and tube layers are not drawn. Hog physics, a guess. Checked in Dolphin.

### Starship

Tenth vehicle (`ship.spr` from `starship/starship.png`): one static 232x98 frame, no rider; glow, jet and speed-table layers are not drawn. Hog physics, a guess. Checked in Dolphin.

### Sleigh

Eleventh vehicle (`sled.spr` from `sled/sleigh.png`): a real 4x2 animation sheet of 76x64 frames, drawn without the reindeer (separate 256x256 sheet, not analysed). Hog physics, a guess. Checked in Dolphin.

### Jetski

Twelfth vehicle (`jski.spr` from `Jetski/jetski.png`): one static 64x32 frame; the handle and the water are not drawn, and the rider is not shown. Hog physics, a guess. Checked in Dolphin.

### Dragon

Thirteenth vehicle, so all 13 in `vehicleProbability.xml` can now be ridden. It is assembled from the real default-dragon sprites (`drh.spr` head, `drb.spr` body, `drf.spr` four wing frames): head in front, three body segments trailing in a sine wave, wings on the middle one. The layout is mine, since the real offsets live in native code; no jaw, horn, nose or fire. Hog physics, a guess. Checked in Dolphin.

### .uiscreen events

`uidecode.events()` now fills `component["events"]` with `(event, [(command, [string args])])` read from the `HTVE`/`CEIU` sections (numeric operands are still skipped). Across all screens `OnButtonPressed` is the commonest event (881) and `InvokeTrigger` the commonest command (1176), e.g. the pause buttons invoke `Triggers.Buttons.Resume`, `.Retry` and `.Quit`. Known gap: child lookup via `hasParent` does not reach everything under `PauseContent` (only its title text shows), so a menu cannot be laid out from it yet.

Correction to the above: `Component` is just the generic container class, so the parent links are not shown to be wrong. Names are the unreliable part (the id table holds 251 names for 643 components, and the Resume button comes out named `Mission2`), so a menu is found by its events and properties instead of by name. That works: the three pause buttons are the `Button` components whose `OnButtonPressed` runs `InvokeTrigger ...Triggers.Buttons.Quit/Retry/Resume`; each is 216x216 on `buttonBacking.tex`, with an icon (`paused/buttonExit`, `buttonRestart`, `buttonResume`, 128px at y -20) and a `PAUSED_*` label (y +68).

### Pause menu

The app's pause screen now shows those three buttons at 0.6 scale: left/right selects, A or 2 activates Resume, Retry (restarts the run) or Quit (back to the title); PLUS pauses and unpauses. The layout and art are the game's own, the spacing and scale are mine, and the dim backdrop and the button animations are not done. Checked in Dolphin for the drawing; the button input was not exercised by the fake-input build.

### Title START button

The title screen's prompt is now the game's own `startPressed` button from `TitleScreen_SplitTest.uiscreen` (332x96 `buttonBacking`, the green glow bar at x -144, "START" at x +38), drawn at 0.75. The glow bar sheet is one 64x128 image, not a play icon. No Shop/Events buttons yet; those need their own screens. Seen in Dolphin; the results screen (`ResultsScreen_New`, 1958 components of leaderboards, social and rewards panels) was looked at and is not a practical next port.

### Shop: Coin Magnet

The title screen has a SHOP button under START (up/down selects, A opens). The shop sells one gadget, the Coin Magnet, with the real icon (`perks.xml` lists 19 gadgets but gives no prices, so the price of 500 coins is mine, paid from the lifetime coins in `save.txt`, which now has a third number for ownership). With the magnet owned, coins within about 130 px of the player are collected. Checked in Dolphin with the fake-input build, which now selects SHOP, opens it, buys and goes back before the first run: starting from 600 coins the log shows `mag 1` and 100 coins left plus each run's pickups (43, then 103, then 52), and `save.txt` ends with ownership `1`. Whether the magnet radius feels right is not judged.

### Shop: Slow Missiles

A second gadget, Slow Missiles (real icon; missile speed x0.72, the `value1` in `perks.xml`), for 400 coins (price mine). Up/down in the shop switches item; `save.txt`'s third number is now a bitmask (1 magnet, 2 slow missiles; old saves still load). The fake-input build buys both (1000 coins start, 100 left, save ends with `3`). The slower missile itself was not isolated in a test.

### Controls

A, B, 1 or 2 hold the jetpack (and start or restart a run); PLUS pauses; HOME quits. In the pause menu and on the title screen, the d-pad selects (left/right in pause, up/down for START/SHOP and between shop items), A or 2 confirms, B or 1 backs out of the shop. Menu directions are the raw `WPAD_BUTTON_UP/DOWN/LEFT/RIGHT` bits, so with the Wii Remote held sideways the physical d-pad directions are rotated relative to what is drawn; that has not been tried on a real remote, and a Classic Controller or GameCube pad should behave as drawn.

### Shop: Dud Missiles

Third gadget, Dud Missiles (real icon; the first 3 missiles of each run do no harm, `perks.xml` value1 3), 450 coins (price mine). The shop now cycles three items with up/down; `save.txt` bit 4 marks it. The fake-input build buys all three from 1500 coins (150 left, save ends with `7`).

### Death tumble

When Barry is hit on foot he now falls and bounces during the 60-frame death freeze, using `settings.xml` `DEATH_GRAVITY` 0.24 and `MIN_BOUNCE_VELOCITY` 4 (scaled by the 1.5 screen factor; the bounce damping is my guess). Distance and the run log are unchanged. Seen in Dolphin: after a mid-air hit he ends on the floor. `settings.xml` also lists `BARRY_GRAVITY` 0.15, `BARRY_BOOST` 0.36, `BARRY_FRICTION` 0.98 and `IMMUNITY_TIME` 700; the jetpack feel still uses my own earlier numbers because the units of those are not known.

### Shop: Vehicle Magnet

Fourth item: while riding a vehicle, coins within about 190 px are pulled in (the game sells per-vehicle magnets: "Upgrade your sweet ride by adding a coin magnet"). One item covers all 13 vehicles here, it reuses the Coin Magnet icon, and the 600-coin price is mine. `save.txt` bit 8 marks it. The fake-input build buys all four from 2000 coins (109 left after the first run, save ends with `15`); the pickup effect itself was not isolated.

### Colossatron (my own boss)

Every 2500 m from 1500 m the game sends in a Colossatron: the real head and body-segment art as a snake of 7 pieces that sweeps in from the right in a sine wave; touching a piece is a hit (a vehicle absorbs it as usual), and outlasting it gives 25 coins. **This is my own design**: the real boss logic is native code, so the speed, wave, spacing and reward are guesses, and it fires the real `sonicBoom` swoosh from its head every 100 frames (7 px/frame, a hit like any other; timing mine), and has no jaw, health or music (`Music_Colossatron.ogg` exists but is not used). The test build triggers it at 300 m; seen on screen in Dolphin, the reward path was checked with a log counter (one boss outlasted, coins 59 -> 84, the 25-coin reward); the hit path too (two later runs logged cumulative effective boss hits 1 then 3, each costing the vehicle or the run).

### Shop: Rubber Bones

Fifth item, with the real icon: when you die on foot the tumble bounces further (`settings.xml` has `DEATH_BOUNCE_DAMPENING_RUBBER` 1.6 against 0.99 normally; I scale that onto my 0.5 bounce factor to get 0.8). Cosmetic only. 350 coins (price mine); `save.txt` bit 16. The fake-input build buys all five from 2500 coins (284 left after the first run, save ends with `31`); the bounce itself was not compared in a test.

The boom is drawn and flies (seen in Dolphin; the test log counted 2, 3 and 4 launched and passed across three runs), but no run was hit by one, so its hit path is unchecked.

### Game-over summary

The game-over screen now also shows the run's coins and the running total, and a blinking NEW BEST when the distance beat the saved best. Plain text on the existing screen; the real results screen (leaderboards, rewards, social panels) is not ported. Seen in Dolphin.

### Memory check

The sprite files loaded at start now total about 4.4 MB (largest: `mech.spr` 0.5 MB) plus about 0.25 MB of encoded sound effects (a few MB once decoded, my estimate; the 1.5 MB music file is streamed, not loaded), and one level's backdrop and foreground sheets on top. That runs fine in Dolphin (24 MB MEM1 + 64 MB MEM2), but I have not measured the heap on a console or checked which of the two memory areas libogc's `malloc` draws from, so a real Wii could run tighter than Dolphin suggests.

### Music switching

The music thread can now change tracks (`audio_switch`): `Music_Shop` while the shop is open, `Music_Colossatron` during the boss, `Music_Hog` while riding the Hog, otherwise `Music_Level` (all from the APK; which plays when is my mapping, and the title has no `Music_Menu` because the APK ships it only as m4a). The test build logs each switch: shop at tick 91, back at 171, boss at 781, back at 1194, and the run finished normally. I could not listen to Dolphin's audio, so that the right track is audible is unconfirmed.

### Obstacle pacing

The gap between laser patterns used a fixed 60 frames. It now follows `LevelPacing.xml`: `MIN/MAX_SPAWN_INTERVAL` per distance band (280-470 at the start, tightening to 150-230 from 9500 m). I take those units as the game's own pixels, so x1.5 to ours and /4 for the 4 px per frame scroll, giving roughly 105-176 frames at the start; that unit reading is a guess, and the file also names the gates, missile and vehicle tables, whose spacing I did not change. Runs in the test build still complete (894 m and 748 m).

### Distance scale

The distance counter was `scroll / 2`, about 120 "metres" a second, which put every distance-based table in the APK (laser and missile start distances, pacing bands, vehicle spawn, the boss) far too early. It is now `scroll / 27`: `settings.xml` `DISTANCE_DIVISOR` is 18 and our scroll is the game's pixels times 1.5, giving about 9 m/s at 4 px per frame, so 1000 m takes under two minutes. The unit reading is an inference from that one value, not checked against the real game. The fake-input test build now dies at 43-51 m (its boss trigger there is 30 m and the fake input cannot dodge), so its old 700-1200 m runs no longer compare.

### Immunity after losing a vehicle

The grace period after a vehicle is destroyed is now 42 frames: `settings.xml` `IMMUNITY_TIME` is 700, which I read as milliseconds (it was 90 frames). The unit is a guess. The test build still runs normally.

### Shop: Coin Multiplier

Sixth item, with the real `gem` icon: every coin counts double. `perks.xml` lists `PERK_COIN_MULTIPLIER` with value 5 but nothing says what the game does with it, so the effect is my own reading; 700 coins is mine too, and `save.txt` bit 32. The fake-input build buys all six from 3500 coins and the save ends with 500 coins and `63`; the doubling itself was not isolated in a test.

### Boss health

The Colossatron now has 3 health: while riding a vehicle, ramming its head costs it one (40 frames between hits) and the third defeats it for 100 coins; body segments, its boom and touching it on foot still hurt you. **This rule is my own.** In the fake-input test build (which cannot aim) the boss was never rammed, so kills stayed at 0 and the defeat path is unchecked; the same runs did log one boom hitting the player, so that hit path now is checked (a test-sequence slip of mine, a clashing key at tick 180, had made the title reopen the shop, fixed).

Follow-up on boss health: my first ramming rule could never kill it (the head crosses the player in about 32 frames, shorter than the 40-frame cooldown, so one pass gave one hit). A ram now also knocks the boss back 130 px so it can be rammed again. The test build now hands the vehicle and steers it into the head whenever the boss is on screen (test only); with the boss at 15 m the log shows `kills 1` and a run to 237 m, so the three-ram defeat and its 100 coins now work. Against a human the rule is untried.

### Shop: Vehicle Start

Seventh item, with the real `vehicleStart` icon (`PERK_START_IN_VEHICLE` in `perks.xml`): every run begins riding a random one of the 13 vehicles. 800 coins (mine), `save.txt` bit 64. The fake-input build buys all seven from 4500 coins (1358 left after the first run, save ends with `127`) and a capture at 39 m shows the run already on the Mech; the random pick was not checked across vehicles.

### Shop: Gravity Belt and Dud Zappers

Items 8 and 9, both with their real icons and both my own readings of `perks.xml`: **Gravity Belt** (value 0.22) lowers the vertical speed cap by 22
### Shop: Gravity Belt and Dud Zappers

Items 8 and 9, both with their real icons and both my own readings of `perks.xml`: **Gravity Belt** (value 0.22) lowers the vertical speed cap by 22 percent, for finer control; **Dud Zappers** (value 1) makes the first zapper of each run harmless. 450 and 500 coins (mine); `save.txt` bits 128 and 256. The shop is now table-driven (`shopName`, `shopDesc`, `shopPrice`, `shopOwn`), so another item is one row plus its effect. The fake-input build buys all nine from 6000 coins (1882 left after the first run, save ends with `511`); neither effect was isolated in a test.

### Shop: X-Ray Specs and Brick Bones

Items 10 and 11, real icons, my own effects: **X-Ray Specs** (`PERK_SHOW_VEHICLES`) makes vehicle crates come twice as soon (the wait between them is halved); **Brick Bones** (`brick` icon, the `DEATH_BOUNCE_DAMPENING_BRICK` 0.6 tunable) makes the death tumble bounce only a quarter as much. 500 and 300 coins (mine); `save.txt` bits 512 and 1024. The fake-input build buys all eleven from 8000 coins (the save ends with 2450 coins and `2047`); the effects were not isolated in a test.

### Shop: Fruit Missiles

Item 12, real `fruitMissile` icon (`PERK_FRUIT_MISSILES`, value 3 in `perks.xml`): every missile you dodge pays 3 coins. That is my own reading; the APK does not say what the perk does. 400 coins (mine), `save.txt` bit 2048. The fake-input build buys all twelve from 9000 coins (save ends with 3706 coins and `4095`); the payout itself was not isolated in a test. Left unbuilt from `perks.xml`: Nerd Repellent, Boost Machine, Token Magnet, Token Gift, Lucky Token, Flying Pig, Robot Dog and Icy Exterior, which rely on tokens, scientists, pets or a slot machine that this port does not have.

### Shop: Icy Exterior

Item 13, real `icySkin` icon (`PERK_ICY_EXTERIOR`, value 3.5): after a hit on foot the world keeps scrolling for the 60 death frames while slowing, so the distance counts a few metres further. The slide numbers are mine (`settings.xml` has only `DEATH_X_GROUND_FRICTION_ICY`); 350 coins (mine), `save.txt` bit 4096. The fake-input build buys all thirteen from 10000 coins (save ends with `8191`); the best distance was 229 m against 226 m earlier, but runs vary, so the slide is not isolated. Seven `perks.xml` gadgets remain: Nerd Repellent, Boost Machine, Token Magnet, Token Gift, Lucky Token, Flying Pig, Robot Dog.

### Shop: Robot Dog

Item 14, the real Robot Dog art (`robodog-alt.png`, a 4-frame trot from row 2 of its sheet) and shop icon: a dog trots at the left edge of the screen and picks up coins that pass near the ground in front of it. The game'"'"'s `PERK_ROBOT_DOG` says nothing about what the dog does, so that is my own reading, as is the 600-coin price (`save.txt` bit 8192). Seen in a Dolphin capture trotting on the ground once Barry is out of a vehicle; the fake-input build buys all fourteen from 12000 coins (save ends with `16383`); the ground pickup was not isolated in a test. Six `perks.xml` gadgets remain: Nerd Repellent, Boost Machine, Token Magnet, Token Gift, Lucky Token, Flying Pig.

### Tokens, Token Magnet and Token Gift

A spin-token pickup now exists: the real `RewardSystem/Tokens/pickup.png` gem drifts in about every 8 to 13 seconds at a random height and pays 10 coins when you touch it. How often it comes and what it pays are mine (in the game tokens feed the reward system, which this port does not have). The shop gained **Token Magnet** (the gem is pulled in from about 200 px) and **Token Gift** (a token pays 30 instead of 10), items 15 and 16, both with their real icons and my own readings of `perks.xml`; 500 coins each, `save.txt` bits 16384 and 32768. The fake-input build buys all sixteen from 14000 coins (save ends with `65535`) and one run logged a token collected (`tok 1`); the magnet's pull and the triple payout were not isolated in a test. Four `perks.xml` gadgets remain: Nerd Repellent, Boost Machine, Lucky Token, Flying Pig.

### Shop: Boost Machine and Lucky Token

Items 17 and 18, real icons, both tied to the spin token and both my own readings: **Boost Machine** makes a token also carry you 10 m ahead; **Lucky Token** makes tokens come twice as often. 600 and 450 coins (mine); `save.txt` bits 65536 and 131072. The fake-input build buys all eighteen from 17000 coins (save ends with `262143`); neither effect was isolated in a test. Two `perks.xml` gadgets remain: Nerd Repellent (this port has no scientists to repel) and Flying Pig (no standalone art found).

## Game-over panel

The game-over text is now drawn over the real `menus/popupBacking` panel from the APK. The text and layout are mine; the real results screen (leaderboards, rewards) is not ported.

## Real text

`tools/strtab.py` decodes the APK's `.str` string tables (`translations_header.str` holds the keys, `translations_<Language>.str` the text; the last word of each header record is the index of its text). The shop now uses the game's real gadget names (`PERK_*_NAME`), e.g. Gemology, Missile Jammer, Dezapinator, Flash. The short descriptions are still mine. Vehicle Magnet is my invention and has no real counterpart. (An earlier note here also called Token Magnet an invention; that was wrong: `perks.xml` lists `PERK_TOKEN_MAGNET`, "Magnetic Tokens", and the shop now uses that name and its real description.)

The shop now shows the game's real gadget descriptions (`tools/mkgadgets.py` writes `gadgets.txt` from the string tables; the app word-wraps them). The font has no punctuation glyphs, so commas, periods and hyphens do not draw. The real descriptions describe the real gadgets; what each gadget does in this port is still my own reading, so some will not match their text.

## Bug fixes

- Sound: only the first 16 of the 26 effects ever loaded, so missile and boss explosions, footsteps, landing, the Gravity Suit flip, the teleport, two coin variants and the purchase sound were silent. The limit is now 32, and each effect keeps only the memory it needs instead of a 512 KB buffer.
- Sound: the jetpack loop played once and then restarted with the jet-start sound; looping voices now loop.
- Text: the font has only letters, digits and space, so "LEVEL 1/1" read "LEVEL 11" and words with hyphens ran together. Missing characters now leave a space, and the HUD and shop prompts no longer use `/` or `:`.
- The HUD is hidden on the title and game-over screens.
- Quitting from the pause menu no longer leaves the boss, its boom, a vehicle, a missile or a token on the title screen, and the boss or Hog music no longer carries over to the title or game over. Restart clears a leftover token or boom.
- Game over ignores buttons for the first 45 frames, so a mashed jetpack button does not skip it.
- Free Ride only picks a vehicle whose art loaded.
- Game-over panel re-spaced so the title no longer touches the distance row.

## Flying Pig and Air Barrys

Two more of the game's real gadgets are in the shop, with their real names, icons (`ItemIcons/gadgets/flyingPig.png`, `airBarrys.png`) and descriptions:

- Flying Pig: a golden pig (the real `Entities/distanceMarkerPig` sheet, its two flying rows) crosses the screen about every 20-30 seconds; touching it pays 30 coins. How often it comes and what it pays are mine. In the test build it spawned 4 times and was collected 3 times in one run.
- Air Barrys: pressing the jetpack while standing gives a kick off the floor. `perks.xml` has no number for it; the strength is mine. Not exercised by the test build.

## Scientists and Nerd Repellent

Lab staff now walk along the floor in both directions: scientists and the hard-hat workers, drawn from `Characters/Barry/{scientist,worker}{Body,Head}` with the same 4x8 rig as Barry (row 0 walking, row 2 when knocked over). Jetpack fire from above, or running into them in a vehicle, knocks them flat. Spawn rate, walking speed and the hit rule are mine; the game's own staff come from a body-parts atlas (`Characters/workers0`) with their animation in native code, so this is a stand-in using the costume sheets. A test run knocked over 13.

Nerd Repellent (real name, icon and description) is now in the shop: with it, no staff appear.

## Missions

`tools/mkmissions.py` reads the game's `xml/MissionSystem/Missions.xml` (bxml) and the string tables at build time and writes `missions.txt` to the SD card: the 33 of the 63 active missions whose type this port can track (games played, coins and tokens per game or in total, distance targets, finishing between two distances, reaching a distance without coins or without a vehicle, distance on foot, along the ceiling or in vehicles, gravity flips, vehicles collected, buying an item). Their text and numbers are the game's own. Three are active at a time, as in the game; progress is kept in `missions.sav`, a banner shows when one completes, and the pause screen lists the three with progress for the cumulative ones. The order missions come in (file order) and the absence of rewards (the game's stars and levels, `MissionRewards.xml`) are this port's simplifications. Types needing things this port lacks (high fives, near misses, alarm lights, the slot machine, named vehicles, consumables) are left out.

`strtab.py` fix: the third word of a language record is the length in bytes, not the second (characters); strings with the game's icon glyphs were cut short. U+E005 is the metre glyph and is written as `M`.

In the test build two missions completed in one run, were replaced by the next two, and the new coin total already counted that run.

## Real tuning for the pig and the lab staff

- Flying Pig now follows `xml/flyingPigs.xml`: the first pig comes between 200 and 400 m, at most two per 950 m "kilometre", never more than 770 m without one, and it drifts up and down at most 0.5 (game units). The file's `probability 4` is read here as a 4% roll every 10 m (my reading). The 30-coin payout is still mine.
- Lab staff now follow `xml/WorkerInfo.xml`: a 40% spawn roll, walking at 0.5-2 or (20%) running at 2-6, an alert distance of 200-500 from Barry after which 60% (75% past 1000 m) run off, and when knocked over a launch of 4-8 at -45..45 degrees, gravity 0.5, spin 8-45, Y friction 0.98, bounce dampening 0.7 and ground friction 0.94. How often the spawn roll happens (once a second) and what knocks them over are mine; cowering, tripping and curling up are not done.

Near misses and high fives: missions now also include near misses with missiles or zappers (in one game or in total) and high-fiving lab staff, bringing the tracked missions to 42 of 63. A near miss is a missile passing within 70 px or a zapper beam within 45 px without hitting; a high five is running past a standing worker on foot. Both rules and thresholds are mine. In the test build one zapper near miss was counted; the missile and high-five paths were not exercised, because that run spends most of its time in vehicles.

## On-foot test build

`make -C app AUTOTEST=2` builds a second self-playing variant that stays on foot: no Free Ride, no vehicle pickups, short jetpack hops so Barry mostly runs along the floor, unhittable (the fake input cannot dodge) and ending itself at 400 m. It exercises the paths the vehicle-heavy `AUTOTEST=1` run misses. One run logged 5 high fives, 17 Air Barrys kicks, a zapper near miss, 5 tokens, 5 staff knocked over and 2 completed missions. The missile near-miss path is still unexercised.

## Spin tokens and the Final Spin

- Spin tokens now follow `xml/slotTokens.xml`: the first comes between 200 and 450 m, at most three per 1000 m, never more than 1050 m without one, moving up and down at most 1.8 (game units). `probability 2.8` is read as a 2.8% roll every 10 m (my reading). Tokens no longer pay coins directly.
- At game over, each token collected is one Final Spin before the results panel (A spins). The game's prize table lives in native code; the one here is mine: half the spins win nothing, the rest win 100, 250, 500 or (2%) 1000 coins.
- Token Gift and Lucky Last now do what their real descriptions say: a free spin every game, and the last spin always wins (at least 250 here).
- The two slot missions (cash in tokens, win prizes) are tracked, bringing the total to 44 of 63.
- Test build: tokens were collected and spun in both runs (one run won 2 of its spins).

## Alarm lights

The level files put a red alarm light (`LIGHT_HALLWAY_ALARM`) on a quarter of the wall pieces. Its art is not among the extracted textures, so the light here is a plain red glow generated in code, blinking; it sits on 25% of the foreground pieces (a fixed hash, so each stays put) at one wall height of my choosing, since this port does not carry the levels' per-piece objects. Flying within 40 x 50 px of one counts as brushing past it, which feeds the two alarm missions, bringing the tracked missions to 47 of 63. A test run brushed past 4.

Named-vehicle missions: the missions that name a vehicle (collect the Mech N times; fly a distance in the Mech, Bad Ass Hog, Bird, Gravity Suit or Teleporter in one run or in total; knock down staff with the Mech) are tracked, with the game's names mapped to this port's vehicles. That brings the tracked missions to 57 of the 63 active ones; the rest need consumables (head starts, revives) or the machine-gun jetpack, which this port does not have. A test run logged 109 m in the Mech, and 69 m in the Mech plus 31 m on the Hog, with one pickup counted.

## Death slide, and three more missions

- When Barry is hit he now slides on along the floor the way `settings.xml` describes: he keeps the run speed, loses it to `DEATH_X_GROUND_FRICTION` 0.94 per frame on the ground (Insta-ball 1.0, Half a Brick 0.1, Freeze-o-matic 1.0) and none in the air (`DEATH_X_AIR_FRICTION` 1.0), and the run ends when he stops. A frictionless slide is cut off after 4 s (mine). Previously the world simply stopped. In the on-foot test run he slid 2 m, as 0.94 friction from the run speed gives.
- Missions now also track sliding on your face (total metres slid), smashing through the wall (games started from the menu) and using any gadget (starting a run with one owned), bringing the total to 61 of the 63 active missions. The two left need a head start or a bomb consumable.
- The vehicle test build (`AUTOTEST=1`) keeps re-mounting Barry during the boss, so it never slides; the on-foot build is the one that checks it.

## Head Start

The Head Start consumable is in the shop (icon `ItemIcons/skip.png`, the first sentence of its real description). Each purchase is one use and the count is kept as a fourth number in `save.txt`. Starting a game from the menu with one in stock uses it and skips the first 750 m, as its text says; the real one flies you there, this one jumps, and the 2 seconds of cover after it and the 250-coin price are mine. Using one counts for the head-start mission, so 62 of the 63 active missions are tracked; the last needs a bomb consumable. In the test build the first run started at 750 m and ended at 841 m, with one Head Start left afterwards.

## Stars and ranks

Completing a mission now earns stars equal to its `DIFFICULTY` (1-3) in `Missions.xml`, and every 10 stars is a new rank, using the game's 16 rank titles (`MISSION_LEVEL_1..16` in the string tables, written to `ranks.txt` at build time). A rank-up shows a banner and pays 250 coins times the new rank. The star value, the 10-star step and the payout are mine: the game keeps those rules in native code. The rank and stars show on the title screen and the pause screen and are saved with the missions. In the test build (started one star short) the first completed mission ranked up from the first title to the second. Mission and rank banners no longer draw over the pause menu.

## Final Blast

The bomb consumable (the game's "Final Blast", shop icon `ItemIcons/bomb.png`) is in the shop, bought one at a time and kept as a fifth number in `save.txt`. When Barry stops sliding after a crash and you have one, "PRESS A TO BLAST" shows for 2 seconds; pressing A uses it and blows him on along the floor (the Colossatron boom art as the flash). The game has no description of what it does beyond its name and a BLAST button in the HUD, so the window, the launch (16 forward, 14 up, in screen px per frame), the 300-coin price and the shop line are mine. Using one counts for the last mission type, so all 63 active missions are now tracked. In the on-foot test runs each crash used one (3, then 2, then 1 left) and stretched the 2 m slide to 11 m.

The on-foot test build now sends a missile at every chance, so the missile near-miss path runs too: a run logged one missile near miss.

Results panel: the decoded `ResultsScreen_New` overview lists distance, what was collected (coins and tokens), equipment and "assists" (gadgets). The game-over panel now shows the run's tokens next to its coins and the number of gadgets used beside the total. The leaderboards, rewards and social parts of that screen are still not ported.

## Level rotation, title room, music fix

- Levels: of the 34 resolvable level files, only the regular rotation is built now (`Room1` and its City/Forest/Volcano/
  Warehouse window variants, warehouse, cave, caveVolcano, forest, hills, sewer; list in `tools/mklevels.py`). The licensed and
  seasonal event stages and the `TV` duplicates build their backdrops from atlas fragments, which the strip-layer model drew
  as scattered pieces. All 11 were cycled through in Dolphin and render whole.
- Title: drawn over the lab room it sits in in the real game (`bgTitle` atlas, top-left 672x512 minus its dark margin,
  stored as `title.spr`) instead of the first level, with the logo, START and SHOP over it and best/rank below the floor.
- Music: `ASND_AddVoice` answers `SND_INVALID` (not `SND_BUSY`) while the previous `ASND_SetVoice` is still pending; the
  stream loop took that as success, so it decoded far ahead of playback and overwrote the buffer being played, leaving the
  voice silent. It now keeps three buffers (playing, queued, decoding), retries until the add succeeds and only restarts the
  voice on a real underrun; the thread stack went from 16 to 64 KB for Tremor. Checked in Dolphin: 157 buffers queued over
  about 14 s of play (14.6 s of audio), no restarts.

## Title menu from the decoded TitleScreen

`tools/uidecode.py` decodes every `.uiscreen` (all 471 complete); its `__main__` block ran before `events` was defined, so running it as a script crashed - fixed. `TitleScreen_SplitTest` has 1598 components. Its `mainMenu` is a 310-wide panel at x -450 from the centre of the 960x640 screen (the left side), with a `hazzardStrip` of `textures/menus/backings/hazzardTile` (tiled 128 px tile) behind the START and SHOP buttons. The Wii title now draws that panel and puts START and SHOP in it, with the logo and Barry to its right over the lab room. Positions are scaled by eye to 640x480 rather than computed: there is no docking/layout engine yet, so the gradient overlays, rim light, events, rewards and settings buttons are not drawn. Checked in Dolphin.

## Results screen layout

The game-over screen now follows the decoded `ResultsScreen_New`: two halves of the 960-wide screen, `LeftSide` (the overview panel: distance, best, collected, totals) and `rightSide` (coin balance at the top, the play-again button at the bottom). Panel art is `popupBacking` for both; the real one uses its own backing sprites, and the leaderboards, missions tab, profile block and social buttons are not drawn. The text is mine, positions scaled by eye. Checked in the Dolphin AUTOTEST run.

## Pause header

`Pause.uiscreen` puts a 128-high header (`PauseHeaderBacking`: three `menus/paused/titleBar` pieces, left/middle/right) across the top with the title "PAUSED" in the middle, the score and gadgets on the left and buttons on the right. The pause screen now draws the three pieces and moves "PAUSED" into the bar. The score/gadgets/buttons are not moved into it, the dim backing (`black.tex`) is not drawn, and the HUD text still overlaps the bar's left end. Checked in Dolphin.

The pause screen now also dims the game behind it (a generated translucent black quad standing in for `black.tex`, as `BlackBacking` does). Checked in Dolphin.

## Layout tool

`tools/uilayout.py x.uiscreen` prints absolute rectangles (960x640, top-left origin) for every component that is visible in the file's initial state. The rule it uses, checked on `ResultsScreen_New` (`LeftSide` lands on x 0-480 and `rightSide` on 480-960): `position` is the offset of a component's centre from its parent's centre; `Fill` takes the parent's rect and `Top/Bottom/Left/Right` sit flush to that edge. Docks `DistributeH/V`, `TopLeft` and table layouts are not modelled. It cannot give resting positions of things that slide in: the title's `mainMenu` comes out at x -125 (off the left edge) because a trigger animates it in, and those trigger values are not decoded, so the Wii title panel's position is still by eye.

`tools/uipreview.py x.uiscreen <png dir> out.png` draws a screen with those rectangles (textures stretched to their rect, quads filled, text shown as its string key) so a layout can be checked by eye. On `ResultsScreen_New` it reproduces the two panels, the profile avatar, the coin balance and the bottom buttons in the right places; every tab and state is drawn at once (the file's trigger-driven states are not decoded), so alternatives overlap, and nine-patch windows are stretched rather than sliced.

`uipreview.py` skips device-notch guides and input blockers. Finding for the shop: `TheStash.uiscreen` is only the frame (back button, tab header); the item grid comes from `StashItems.uiscreen` and the `DataSource*` components at run time, so the real shop cannot be drawn from one screen file. A faithful shop needs those data sources (the item lists in `perks.xml` and the shop config) wired to `StashItems`' per-item component, which is the next piece to decode.

## Barry's flight constants: what is and is not in the data

`xml/settings.xml` (plain XML, not bxml) lists `BARRY_GRAVITY` 0.15, `BARRY_BOOST` 0.36, `BARRY_FRICTION` 0.98, `BARRY_VELOCITY_SCALAR` 0.5, `DISTANCE_DIVISOR` 18, `IMMUNITY_TIME` 700. Checked against `libmortargame.so`: the strings `BARRY_GRAVITY`, `BARRY_BOOST`, `BARRY_FRICTION` and `BARRY_VELOCITY_SCALAR` do not occur in the binary at all, so the game does not read them (stale entries from an older version); `DISTANCE_DIVISOR` does occur. The app's distance is `scroll / 27` in Wii pixels, which is 18 in the game's 480x320 units (x1.5 scale), so that one already matches. Barry's real jetpack acceleration, terminal speed and friction are therefore in native code, and the app's values (+0.6 / -0.9 per frame, cap 9) remain my own until that code is decompiled.

## Barry's physics: decompiled (`Player::Jetpack_Update`), and the run-speed ramp

Decompiled `Player::Jetpack_Update` (0xeb0d94 in the .so; Ghidra 0xfb0d94). Per frame, with `dt` the frame time: gravity and thrust are applied as `v -= dt * ... ` and `v += dt * thrust * ...` (`JetpackInstance::GetThrust`, scaled by the ascend/descend linear and scalar modifiers), then the velocity is damped by `v *= powf(friction * jetpackFriction, dt * 60)` (the jetpack's `friction_ascend` when rising, `friction_descend` when falling) and clamped to `[-velocity_max_descend, velocity_max_ascend]`. Lead Boots (the Gravity Belt gadget) swaps in a second velocity slot. So the real model is thrust and gravity with exponential friction and speed caps, not the app's fixed +0.6 / -0.9 steps.

The numbers are not in the APK. `JetpackConfig` reads its `friction_ascend`, `friction_descend`, `velocity_max_ascend`, `velocity_max_descend`, thrust and gravity attributes from `Configs/jetpacks/<name>_1.json` (config category `"jetpacks"`), and that folder is not in `assets.zip` (the same way the 22 downloaded levels are missing). `JetpackConfig::GetVelocityMaxAscend` is a plain field read, so there is no default in code. The `BARRY_*` entries in `settings.xml` are dead (see above). So the exact flight feel cannot be recovered offline; the app keeps its own values, now known to be the wrong shape rather than just the wrong numbers.

What the data does give: `Configs/game_mode_rules/classic_1.json` has `start_velocity` 175, `end_velocity` 480 and `velocity_stop_distance` 2250 (units/s of the 480x320 frame, metres). The app ran at a constant 4 px per frame (the starting speed); it now ramps from 4.375 to 12 px per frame over 2250 m (linear, my reading: the file gives no curve), and the foot/ceiling/vehicle distance counters for missions use the same speed. Checked in Dolphin (the self-playing run still plays through to game over).

## In-run pickups (Configs/pickups)

`Configs/pickups/manager_1.json`: the first pickup comes at 300 m, then one every 500 m plus 0-500 m random, chosen by `spawn_weight` (25 each) from Coin Special, Boost, Shield, Slow Motion, Revive and Final Blast. Two of those are not real in 1.104.2: `Pickup_Revive` and `Pickup_FinalBlast` are `PICKUP_COMING_SOON` entries with weight 0 and no levels. (So the Final Blast consumable in this port's shop and its crash blast are my invention built on a mission type; the game's own Final Blast pickup does not exist yet in this version.) Slow Motion is only offered from mission level 5.

Added, at upgrade level 1 of each (all numbers from the pickup JSON): Shield absorbs one hit (`numHits` 1); Boost carries Barry 100 m at speed 2000, slowing over the last 30 m, and he cannot be hit meanwhile; Coin Special pays 300 coins over 60 m (its gem share and coin-stream art are not done). Slow Motion (7 s at half speed, level 1) is not done. Upgrade levels are not modelled; the pickup sprites are the whole 128x64 sheets squeezed to 64x32. The test build spawns the first pickup at 40 m and logs `pickup spawn` / `pickup got` lines to `dbg.txt`; one run logged a spawn at 750 m collected at 783 m and the next spawn scheduled for 1560 m.

Slow Motion pickup (level 1: 7 s, time multiplier 0.5, `Pickup_SlowMotion_1.json`) is in the pickup rotation from mission level 5. It halves the run speed only (obstacles and Barry's own movement keep running at full speed), so it is a rough version of the real time scale. The test build now starts at rank 5 so the pickup can appear. In two Dolphin runs it was built and the other pickups still spawned and were collected, but a Slow Motion pickup did not come up by chance, so that effect has not been seen working.

## The Final Spin uses the game's own slot table

`Configs/slots/default_1.json` is the real prize table (earlier notes said it lived in native code; wrong). Weighted outcomes (total 985): COINS_100 120, COINS_500 90, COINS_1000 50 (these pay the file's `rewards` 200 / 1000 / 2000 coins), EXPLOSION1/2/3 120 / 75 / 40 (the near misses: no prize), REVIVE 100, TOKENS 70, DOUBLE_COINS 120, START_500M 100 listed twice. The spin now draws from that table. Where this port has no equivalent, it does the nearest thing and says so on screen: REVIVE pays nothing (the game's own Revive pickup is "coming soon" in this version), TOKENS adds one more spin (amount is mine), DOUBLE_COINS adds the run's coins again, START_500M starts the next run at 500 m (with 2 s of cover, mine). Not done: the entries' `afterWinProbPercent` (the chance of a repeat after a win), `intensity` (the wheel's near-miss animation) and the three SLOT_SHOP_ITEM entries, which have probability 0. Lucky Last still forces a win on the last spin (200 coins). Test build log from one run: 5 spins, 3 coin wins (200 / 200 / 1000), one BOOM, one Extra Token that added a spin, and one Start 500 m after which the next run's first pickup spawned at 500 m.

## Ranks use the game's level table

`Configs/mission_system/mission_level_system_1.json` is the real rank table (earlier notes said the rules were in native code; wrong): 15 levels, each with `AdvanceStars` (3, 3, 3, 5, 5, 7, 7, 7, 9, 9, 11, 11, 11, 13, 13) and `CoinReward` (400, 800 ... 6000 in steps of 400). The app now uses it instead of 10 stars per rank and 250 x rank coins; finishing a level pays that level's coins, and the 16th rank is the last. Still mine: how many stars a mission gives (the app uses its `DIFFICULTY` 1-3; no config says), and that surplus stars carry over. In the test build a rank 5 player one star short finished a mission and was paid 2000 coins.

## Vehicle run speed (Vehicle::UpdateLevelSpeed)

`Configs/vehicles/*_1.json` give each vehicle `START_VELOCITY`, `MAX_VELOCITY` and `VELOCITY_INCREASE` (mech 200 / 1000 / 0.22, Bad Ass Hog 400 / 6000 / 0.40, Profit Bird and Jetski 250 / 1000 / 0.09, Gravity Suit and Teleporter 250 / 1000 / 0.25, Hoverboard 300 / 5600 / 0.2, Ice Skates 300 / 5600 / 0.15, Ecto, Delorean and Starship 400 / 6000 / 0.40, Sled 200 / 800 / 0.15). Decompiling `Vehicle::UpdateLevelSpeed` (the output came back cut off) shows what they do: while riding, the level speed grows by `INCREASE * sin(ease)` every frame, the ease angle going 0 to 90 degrees in half a second, capped at `MAX`; after the ride the speed returns by a factor 0.975 per 1/60 s. The app now does this: mounting sets the speed to the vehicle's start value, riding accelerates it, and dismounting eases back to the level ramp. Two parts are my reading: that the ride starts at `START_VELOCITY` (the part of the function that sets it was cut off) and that it returns to the level ramp (the decompile decays toward a field I could not identify). The Dragon (Colossatron) boss and the Mech have their own handling and were not changed beyond this. The test log shows mounts at the start speeds (skates 300, mech 200) and the runs still reach game over.

## Vehicle obstacle sets (LevelPacing.xml streaks)

`LevelPacing.xml` has a streak per vehicle that swaps the obstacle, missile and coin files while you ride (`Streaks/MECHObstacles.xml`, `HOGObstacles.xml`, `GRAVObstacles.xml`, `HOVERObstacles.xml`, `ICESKATESObstacles.xml`, `ECTObstacles.xml`, `DELObstacles.xml`, `SLEDObstacles.xml`, `JETSKIObstacles.xml`). The zapper files are in the same format as `ObstaclePatterns.xml` but differ (smaller scales, other weights; the Hog's has 4 patterns). The app now builds one zapper set per vehicle (`zv<N>.txt` by vehicle index) and uses it while that vehicle is ridden, going back to the normal set on dismount, clearing any zapper on screen at the switch. The Profit Bird, Teleporter and Starship have no obstacle file of their own and keep the normal set. Not done: the vehicles' own missile files (`*Missles.xml`, with their own probability tables) and coin files (`HOGCoins.xml` and others), and the streak lengths (`minIntervalCount` / `maxIntervalCount`, which say how many spawn intervals a vehicle streak lasts). Test log: skates switched to their 7-pattern set, the hoverboard to its 4-pattern set, the mech to its 7-pattern set, each back to set 0 on dismount.

Vehicle missiles: the streak missile files (`Streaks/MECHMissles.xml`, `HOGMissles.xml`, `GRAVMissles.xml`, `HOVERMissles.xml`, `ICESKATESMissiles.xml`, `DELMissles.xml` for the Delorean and the Ecto, `SLEDMissles.xml`, `JETSKIMissles.xml`) give each vehicle its own missile chance, tracking speed and shot speed, and the app now switches to them with the obstacle set: mech 20% (chase 1.6, speed 10), Hog 30%, Hoverboard and Ice Skates 0% (so no missiles; shot speed 5). Where a vehicle has no file the on-foot numbers stay. Each file holds a single probability from 0 m, so there is no distance ramp inside a ride. A missile already in flight when you mount or dismount keeps going. The coin files (`HOGCoins.xml` and others) and the streak lengths are still not used. Test log: skates and hoverboard 0%, mech 20% from 0 m, on foot back to 15% from 200 m.

## Coin patterns: what the configs say (not ported)

`Configs/coins_patterns/CoinPatterns_1.json` is the real coin table: a global `probability` 14 from 50 m, then `coinPattern1..27`, a rings pattern and an I-heart-coins pattern all at weight 5 (plus event ones at 1 or 0); the app's chained `pattern.txt` of the 27 standard patterns in a fixed shuffled order is close to that set but not the same selection. Each vehicle has its own file with its own patterns and weights: `Streaks_HOVERCoins_1.json` (global 9 from 1 m; `coinPatternHover1-8` at 15, two of them only from 50 / 100 m, the standard ones at 1), `Streaks_MECHCoins_1.json` (global 4; the standard 26 at 5), and similar for Delorean, Ecto, Gravity Suit, Ice Skates and Jetski. The app preloads one long coin field and does not spawn coin patterns per interval, so vehicle coin sets (and what the global probability means: a chance per spawn interval, per metre or something else is not stated in the file) would need that spawning reworked first. Not done.

## Spawn events (Level::SetupNextSpawner, decompiled)

The coin question is answered: `SPAWN_COIN_PROB` and friends are not separate percent rolls. `LevelPacingSystem::Update` sets four global weights from each manager's distance table (`SPAWN_COIN_PROB`, `SPAWN_LASER_PROB`, `SPAWN_MISSILE_PROB`, `SPAWN_GATE_PROB`), and `Level::SetupNextSpawner` makes one roll per spawn event over their sum (the laser weight only counts when no vehicle or boost is active and no zapper or missile is on screen) and picks exactly one of coin pattern, laser, missile or gate. The gap to the next event is `MIN_SPAWN_INTERVAL + (MAX - MIN) * random` of distance travelled (the table by distance already in `spawnGap`), plus the extent of what was just spawned.

The app used three independent timers (laser gap, a missile roll every 120 frames, a zapper roll every 150 frames). It now uses one event scheduler: weights coin 14 (from 50 m; 4 / 9 / 30 while riding, from the vehicles' coin files), laser from the `LazerPatterns.xml` root table (0, 10 from 400 m, 6, 4, 2, 1, 0.5 by distance; `lasers.txt` now has a `G` line for it), missile from the missile file (15 from 200 m on foot, the vehicle's own while riding), gate from the obstacle file (80). A coin event spawns nothing here, because the coin field is still the preloaded chain. Zappers, vehicle pickups and spin tokens now move with the real world speed instead of a fixed 4 px per frame. My approximations: the spawned extent is 240 / 150 / 150 frames of travel for a laser / missile / gate, the first event comes after 400 px, and nothing models the pickup and boss pauses in the pacing. Test log of one run: 7 gates, 2 missiles, 2 coin events by 1408 m, no lasers (they need an on-foot, clear-screen event and are about 9% of rolls).

## Barry's flight numbers: closed

Checked the last place they could hide. The starter jetpack is built as `JetpackConfig(JetpackType)`, which loads config category `"jetpacks"` file `"default"`, i.e. `Configs/jetpacks/default_1.json`. That file is not in `assets.zip` or `base.apk` (searched both). `JetpackConfig::Deserialise` reads `thrust`, `gravity`, `friction_ascend`, `friction_descend`, `velocity_max_ascend`, `velocity_max_descend`, `freefall_force`, `reverse` and others one by one and only overwrites a field when the JSON has a number for it; the constructors zero the fields and there are no default constants in the code. So Barry's thrust, gravity, friction and speed caps exist only in downloaded content, and the app's own flight values stay until a copy of that file is available (it would take a captured `Configs/jetpacks/default_1.json` from a running install, or an OBB / remote-config download).

## Dynamic coin patterns
Coins are no longer one looping preloaded field. `pattern.txt` is parsed into templates; each coin spawn event (see spawn events above) places one random template at the right edge at a random height (40..170 px), into a pool in world coordinates that is freed once off-screen and cleared on run start. The next event gap includes the pattern's width. Vehicle-specific coin sets/weights are still not used.

### Vehicle coin sets
`tools/mkcoinset.py` converts `Configs/coins_patterns/Streaks_<V>Coins_1.json` into `cv<N>.txt` (per-pattern weights with startDistance steps in metres ridden, plus TopHalfWeight/BottomHalfWeight for the spawn height). The app loads `cv<N>.txt` on mount and the standard set again after the ride. Standard-set weights are uniform (5), so only vehicle sets are weighted. HOG, CFT and SAM have no coin config in the shipped files and use the standard set. Verified to load without errors in headless Dolphin; a later run logged coin events during a ride using the vehicle set (`cv 1`).

### Slow Motion is now a real time scale
The pickup (7 s, multiplier 0.5) now skips the whole world update on every other frame, so Barry, obstacles, missiles and scrolling all run at half speed (reusing the pause path to render without updating). The 7 s countdown is real time. The test build forces it once; the log showed 210 game frames over the 420 real frames of the effect (`slowmo start/end` lines). Sound and animation tied to `frame` slow down with it.
