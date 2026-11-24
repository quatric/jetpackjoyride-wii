# Wii port (work in progress)

Not a wrapper: the Switch port loads Android ARM64 code, which cannot run on the
Wii's PowerPC. This directory holds a from-scratch reimplementation effort that
reuses the original game's assets (v1.104.2, `assets/assets.zip` inside
`split_InstallPack.apk`).

## Asset formats found so far

| ext | status |
|-----|--------|
| `.tex` | Header `TEX\1`; raw packed pixels, no mips. 4444 / 5551 / 888 / 8888 decoded by `tools/tex2png.py` (4651 of 5323). Files with flag `6` (672, e.g. `textures/RewardSystem/HUD`) use an unidentified 1.5 byte/px block format (8-byte blocks, DXT1-like but not plain DXT1). |
| `.atlas` | `TextureAtlasTool v1.1.3.03`. **Not encrypted** (the loader, `Mortar::TextureAtlas::Load` / `SerializedTextureAtlasHeader::Read`, only uses `DataStreamReader::ReadBasicType`; no AES/LZW on this path). Layout: u32 magic `0xfe000003`, len-prefixed tool string, u32 fields (page w/h, ..., record count), then hash-keyed records. Sprite names are stored as 16-byte hashes (`TextureAtlasRecHashBuilder`), not MD5/SHA1/SHA256 of the asset paths (tested), so the exact hash function and record field order still need decompiling (`Mortar::Read(DataStreamReader&, TextureAtlasRec&)` at file offset 0x1684998 in arm64 `libmortargame.so`). |
| `.uiscreen` | Chunked binary (`FSIU`/`BSIU`/`TSIU`), screen components. |
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
