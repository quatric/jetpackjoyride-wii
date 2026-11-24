# Wii port (work in progress)

Not a wrapper: the Switch port loads Android ARM64 code, which cannot run on the
Wii's PowerPC. This directory holds a from-scratch reimplementation effort that
reuses the original game's assets (v1.104.2, `assets/assets.zip` inside
`split_InstallPack.apk`).

## Asset formats found so far

| ext | status |
|-----|--------|
| `.tex` | Header `TEX\1`; raw packed pixels, no mips. 4444 / 5551 / 888 / 8888 decoded by `tools/tex2png.py` (4651 of 5323). Files with flag `6` (672, e.g. `textures/RewardSystem/HUD`) use an unidentified 1.5 byte/px block format (8-byte blocks, DXT1-like but not plain DXT1). |
| `.atlas` | `TextureAtlasTool v1.1.3.03` header, body is encrypted/obfuscated; key is in `libmortargame.so`. |
| `.uiscreen` | Chunked binary (`FSIU`/`BSIU`/`TSIU`), screen components. |
| `.json`, `.lua`, `.xml`, `.ogg`, `.ttf`, `.vs/.fs` | plain; shaders are GLES and must be replaced by GX TEV setups. |

## Usage

    python3 -I wii/tools/tex2png.py <extracted assets dir> <out dir>
