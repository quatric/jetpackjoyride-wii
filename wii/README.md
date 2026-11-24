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
    python3 -I wii/tools/mklevel.py <assets>/xml/LevelFiles/warehouse.xml jj_sprites levels/0   # repeat per level (1, 2, ...), then echo N > levels/count.txt
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
