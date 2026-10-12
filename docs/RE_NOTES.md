# MediEvil: Resurrection (PSP, UCES00052 EU) - Reverse Engineering Notes

## 1. Disc Image
- ISO9660, volume `MEDIEVIL`, 1.45 GB. Extraction: `python3 -I tools/iso_extract.py <iso> extracted`
  (macOS bsdtar fails to read this image). Full file list: `docs/iso_filelist.txt`.

## 2. Executable Code
| File | Description |
|---|---|
| `SYSDIR/EBOOT.BIN` | `~PSP` encrypted loader `medboot` (18 KB) |
| `SYSDIR/BOOT.BIN` | Same `medboot` in decrypted ELF: loads PRX modules and `main.prx` |
| **`USRDIR/prx/main.prx`** | **Full game executable**. Decrypted MIPS Allegrex ELF (PRX, relocatable) |
| `USRDIR/prx/*.prx` | Sony system modules (atrac3+, mpeg, sascore, adhoc) - not ported, stubbed/replaced |
| `USRDIR/UCES000{01,06,15}` | Bonus game demos (WipEout Pure, Ridge Racer, etc.) - ignored |

`main.prx`: `.text` 0x2C60C0 (~2.9 MB code, ~700k instructions), `.rodata` 0x77AC8, `.data` 0x12FD0,
`.bss` 0xEBE74. `.symtab` is stripped - no symbols. Contains `.ctors` (~640 static
constructors -> C++), `.eh_frame`, `.gcc_except_table` -> GCC toolchain (psp-gcc / SN Systems).
Relocations are `.rel.*` type 0x700000A0 (PRX) - needed to load into Ghidra properly.

Imported libraries (port integration points - HAL):
sceGe_user (GE graphics), sceDisplay, sceCtrl, sceAudio, sceSasCore, sceAtrac3plus,
sceMpeg (.pmf video), IoFileMgrForUser, ThreadManForUser, sceUtility (saves/dialogs),
sceRtc, scePower, sceUmdUser, sceNet* (adhoc multiplayer).

## 3. Resource Archive `USRDIR/gamedata.bin` (585 MB)
All offsets in 2048-byte sectors.

**Header:** `"TOCS"` u32 0x30, u32 81540, u32 1, followed by 48 entries of 40 bytes each:
`char name[32]; u32 tocSize; u32 tocSector;` - one group per level
(`global`, `01_danscrypt`, `02_graveyard`, ..., test groups `ent_test`, `99_testbed`).

**Group TOC** (`"TOC0"`):
```
u32 magic 'TOC0'; u32 nBlocks; u32 nAssets; u32 0
Block  [nBlocks] 8B : u24 sector; u8 flags; u16 assetIndex; u16 nextCopy(0xFFFF=none)
Asset  [nAssets] 20B: u32 nameHash; u32 size; char type[4]; u16 kind(1=file, 2=group);
                      u16 block; u16 bstLeft; u16 bstRight
```
- `left/right` - binary search tree keyed by `nameHash` (verified), no hierarchy here.
- Data starts at `Block[asset.block].sector`, **uncompressed**. Blocks with `assetIndex != i` are duplicate
  copies of the same data placed across disc sectors for streaming.
- The second half of the TOCS table (indices >= 28) duplicates group names - likely copies for UMD layer 2
  or variants; not yet fully verified.

**Name Hash** (`main.prx` `FUN_001be1e4`): `h = h*129 + (signed char)c`, initial 0, computed on filename
with extension, without path. Type = first 4 characters of extension in uppercase (`FUN_00222310`).
`tools/namehash.py` recovers names using the binary string dictionary (129 is odd -> suffix can be
analytically stripped): currently 1,263 / 7,788 unique hashes resolved. Still unnamed: INST/SUBZ/TCD/ENTS/TNV/VOL/LOC*.

Extraction: `python3 tools/gamedata_extract.py extracted/PSP_GAME/USRDIR/gamedata.bin assets --names docs/names.tsv`
-> `assets/<NN>_<group>/<TYPE>/<idx>_<name>`, and `_index.csv` in each group. 13,120 files, 253 MB.

Payload magic numbers: ANIM->`ACT4`, PSM->`PSM0` (texture), INST->`BOX0`, SKEL->`SKE0`, MESH->`MOD0`,
SUBZ->`SBZ0`, ENTS->`ENT0`, LAND->`LND0`, VOL->`MAP0`, LOC/NLOC->`LON1`, RLOC->`LOR0`, TNV->`TNV%`, BNK->no magic.

Type breakdown (21,634 records):
SUBZ 60 MB (level subzones), BNK 44 MB (sound banks), SKEL 41 MB, PSM 39 MB (PSP textures/models),
ANIM 23 MB, TCD/TNV (collision / navmesh?), MESH, FLIP (flipbooks/UI), ENTS (entities),
LAND/SNET, LOC/NLOC/RLOC (localization), SPL (splines), LIT/LTR/CCV (lighting, vertex colors).
Each type still needs to be reversed individually.

**Video:** `USRDIR/movies/*.pmf` - standard PSMF container (H.264 + ATRAC3+), converts via ffmpeg.

## 4. Ghidra
- Ghidra 12.1.3 (brew) + extension **ghidra-allegrex v21.4** (`~/Library/ghidra/ghidra_12.1.3_PUBLIC/Extensions`).
- Project: `ghidra/MediEvil` -> `main.prx`, loader "PSP Executable (ELF)", language `Allegrex:LE:32`. Analysis time ~2 min.
- Headless scripts in `tools/ghidra_scripts`:
  - `ExportOverview.java <outdir>` -> `docs/ghidra/{functions,imports,strings,fourcc_refs}.tsv` (10,694 functions);
  - `Decompile.java <outfile> <depth> <addr|dataAddr>...` -> C pseudocode for functions (or callers referencing data).
  ```
  JAVA_HOME=/usr/local/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home   /usr/local/Cellar/ghidra/12.1.3/libexec/support/analyzeHeadless ghidra MediEvil -process main.prx     -noanalysis -readOnly -scriptPath tools/ghidra_scripts -postScript Decompile.java out.c 1 001be1e4
  ```
- Identified functions: `001be1e4` hash129, `002723bc` strlen, `00272004` sprintf, `00272104` strcat,
  `0022223c` basename, `00222380/00222310` extension->FourCC, `00222740` asset loader by name
  (for ANIM also tries `<name>.comp`), `0020aba8` localization chunk handler.
- TODO: sce* imports currently unnamed (`FUN_002c6xxx` in `.sceStub.text.*`) - resolve NIDs.

## 5. Plan
1. [x] Unpack ISO, identify executable code and archive format.
2. [x] `gamedata.bin` extractor, name hash identified (partially recovered filenames).
3. [x] Ghidra + ghidra-allegrex, `main.prx` analyzed. [ ] resolve sce* import NIDs.
4. Locate chunk parsers for each asset type in code (search FourCC constants) -> document formats.
5. Port strategy selection:
   - **(a) Static recompiler** MIPS->C (like N64Recomp / psxrecomp) + HLE layer for sce* API and GE->OpenGL/Vulkan;
   - **(b) Full C++ decompilation** function-by-function (traditional decomp project, long timeframe);
   - **(c) Cleanroom engine** on top of reversed file formats (fastest visual progress, but game logic rewritten from scratch).
   Recommendation: start with (a) - most direct path to a natively running game.

## 6. Geometry and Texture Formats (Verified via Render)
Chunk: `tag[4], u32 size, payload` or container `tag[4], u32 0, u32 size, children`. Dump tool: `tools/chunkdump.py`.

**PSM0** (texture): `'PSM0', u16 mip(0|3), u8 GE format (1=5551, 4=CLUT4, 5=CLUT8), u8 flags (0x80=swizzle 16x8),
u16 w, u16 h, u32 ?`; for CLUT - 16 B palette header, RGBA8888 palette starting at 0x20, followed by raw pixels. `tools/psm2png.py`.

**FRG0** (geometry fragment, shared by SUBZ and MESH):
`VFT0/VFT1` (u32 GE vtype, u32 stride[, u32 flags]) * `TXF0` (float uScale, vScale, uOff, vOff) *
`FVT1` ("PAD" padding to 16 B, followed by vertices) * `FSU0` (u16 triangle strip lengths, 0-terminated).
Common vtypes: 0x1BE (UV u16, color 8888, normal s8, position float; 24 B), 0x1A2 (no color; 20 B).

**MESH/MOD0**: `MOD0{NAME, MDI2, FMH0{NAME, MFL1, FRG0..., TXI1}}`, followed by `TEX0`, `PADS`.
**SUBZ/SBZ0**: `SBZ0{BOX0, DPD0, SZR0{FRG0*N}, OCT0{OMM0|OCM0, OCT0..., FMH0{MFL1, FRR0, TXI1, CNE0}}, INS0{...}}`, `TEX0`.
- `FRR0`: u16 fragment indices into SZR0 pool (0xFFFF terminator; bit 0x8000 unknown).
- `TXI1`: per referencing fragment `{u8 index in TEX0, u8 0xFF}`, ends with `FF FF`.
- `TEX0`: entries `0x03 "T:name.psm" 0`; name -> hash129 -> PSM within the same group.
- `INS0` (`INN0/INM1/INL4`) - prop placement, not yet fully reversed.

## 7. Viewer (`src/`, matching SilentHillOrigins-LevelViewer)
C++17, CMake, SDL2 + GLEW + GLM + ImGui. `medievil-core` (headless, no GL): GameData, Chunk, Texture, Geometry.
```
cmake -S . -B build -G Ninja && cmake --build build
./build/medievil-viewer                          # interactive
./build/medievil-viewer --group 2 --shot out.png # offscreen level render to PNG
./build/medievil-viewer --model all_switch.mesh --shot out.png
```

## 8. Prototype v0.26 (Feb 15, 2005)
`iso/proto/Medievil 0.26 (Feb 15, 2005).iso` -> `extracted_proto026/` (file list: `docs/iso_filelist_proto026.txt`).
- Executable in `SYSDIR/BOOT.BIN` (unencrypted ELF), no standalone `main.prx`. `.text` 0x2A9DD4 (retail: 0x2C60C0).
- **Stripped symbols**: `.symtab` 0x10, DWARF absent - stripped identically to retail. No RTTI either.
- 522 strings not present in retail: devkit screenshot paths `host0:%s_z%04u.tga`, `jump_to_level`,
  `select_language`, `Persistent data at 0x%0X`, `Text Message: %s`, sounds/effects cut from retail.
- `gamedata.bin` 506 MB, header `TOCS1` (retail uses `TOCS0`): TOC layout is likely slightly different.
