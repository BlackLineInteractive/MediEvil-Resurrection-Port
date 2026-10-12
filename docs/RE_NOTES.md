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
- `TXI1`: one `{u8 index in TEX0 (0xFF = untextured), u8 flags (0xFF, sometimes 0xFE)}` per FRR0 ref.
  Entry count = ref count; `FF FF` also occurs mid-list (untextured fragment), so it is **not** a terminator.
  Untextured fragments are the black caps over rooms, never seen from inside the game.
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

## 9. main.prx Symbol Mapping
- **Imports**: all 195 NIDs identified (`tools/prx_imports.py` + `docs/psp_nids.tsv`, 1,760 NIDs from PPSSPP
  Core/HLE @12f2ac3) -> `docs/names_functions/imports.tsv`.
- **Missing functions**: auto-analysis missed virtual methods (only referenced by vtables).
  `tools/find_missing_functions.py` + `CreateFunctions.java` -> increased from 10,694 to **15,733 functions**.
- **Vtables**: `tools/vtables.py` -> `docs/ghidra/vtables.tsv` (1,811 tables, 831 with identified constructors).
  Class names via strings: `tools/name_classes.py` -> `docs/ghidra/classes.tsv` (currently 13: minigame challenges,
  CZLManagerBrain). No RTTI present.
- **Manual names**: `docs/names_functions/manual.tsv`. Applied to Ghidra project via:
  `-postScript ApplyNames.java docs/names_functions/<file>.tsv` (overwrites only FUN_*).
- **Decompilation**: `decomp/ghidra_full/<Name>_<addr>.c`, `decomp/index.tsv` (GhidraDecompileEverything from SHO).

### Game Boot Sequence
`module_start` (0x19ADA0) -> thread `user_main` -> crt0 newlib -> `main` (0x20CF8) -> **`GameMain` (0x20E5C)**:
subsystem init -> `redirection.bin`, `movie_sound_overrides.bin` -> `CGame` (0xB8) -> intro video `00_cambridge_logo` ->
state 0x78 with loop `vtable[2]()` + `EndFrame` -> intro cutscene `e_fm_cs_001` -> main game loop (loops back to
start when game restarts).

### Miscellaneous
- `SaveGame_Disable/Enable(name)` (0x315D4/0x31564) - each boss/event disables saving under its own name
  ("Pumpkin king", "Dragon King", "Stone wolves", "MadAxeMan", ...) -> entry points into boss logic.
- Third-party code: SCE RT DME/Medius (multiplayer middleware, `$Header: /projects/rtime/CVS/dme_*`) - do not port.
- Uppercase entity attribute keys (`CLDLIFE`, `CRYATTTIME`, `COLLSCALE`, ...) - used by ENTS chunks.

## 10. Port: Subzones, Placement, Materials, Flipbooks (`src/Core/World.*`)
| main.prx | C++ | Description |
|---|---|---|
| `FUN_001f651c` CSubZone_Load | `SubZone::Load` | SZR0 pool -> OCT0 -> INS0 |
| `FUN_001f68f8` CSubZone_LoadInstances | `LoadInstances` | INL4/INN0/INM1 |
| `FUN_001f6d08` | `SubZone::InstanceModelHash` | name `"<subzone>_<model>.inst"` |
| `FUN_002182bc` CMeshHolder_SetMaterial | `MaterialFlags::Decode` | MFL1 bits |

- **FRR0 `0x8000|n`** = n-th FRG0 inside the same FMH0 (in SUBZ and INST). Skipping these caused holes in terrain.
- **INS0**: `INL4` = `u32 n; float lod[n]; u16 mode[n] (low byte: -1 none, 0 always, otherwise distance-based);
  names` (numeric IDs like `000048`); `INN0` = u16 model index per instance; `INM1` = PAD + 2xmat4 per
  instance (first is world transform; second holds 1/row scales computed by the game).
- INST name is constructed from the subzone name, but the raw string is not required: `NameHashStripSuffix(hash(SUBZ), ".subz")`
  yields the prefix hash state (verified: prefix + ".subz" == SUBZ hash).
- **INST** = BOX0, DPD0, OCT0{FMH0 with local FRG0}, TEX0.
- **TEX0**: type byte before name: `0x03` PSM, `0x23` (#) FLIP.
- **FLIP**: `'FLIP', u32 frameCount, u32 fps, u32 step`, followed by frames (each a full PSM0). Water = `wateranim_abe_flip.flip`.
- Graveyard: 2 subzones, 240 instances, 0 missing models and textures.
- TODO: blending currently relies on texture alpha - actual GE state from MFL1 (byte8/mode) is not yet mapped.
  Diggable grave is a game entity (ENTS), not a static instance.

## 11. Entities (ENTS) and templates (`src/Core/Entity.*`)
| main.prx | C++ | What |
|---|---|---|
| `FUN_001d95ac` CEntityLoader_LoadENTS | `LoadEntities` | `ENT0`: u32 count, u32 recordOffset[count] (from payload start) |
| `FUN_001db478` EntityRecord_Relocate | token walk in `LoadEntities` | record: u32 ?, u32 nameOffset, token stream |
| `FUN_001d975c` CEntityManager_CreateFromRecord | `Entity::className` | class = name up to `':'`, hashed |
| `FUN_001d7f58` CEntityTemplate_ctor | `TemplateDB` | 454 static registrations |

- **Token stream** (records and templates): `u32 kind [u32 string]`: `1`/`2` bare string (component list),
  `5` `"KEY=VALUE"` / `"KEY = VALUE !"`, `3` begin block (component name), `4` end block. Records end when depth
  < 0; templates are one top-level block. In ENTS strings are offsets from the payload start, in templates
  absolute pointers into main.prx.
- **Record**: `UID`, `POS=x y z`, `ROT=x y z w` (quaternion), `ZONE`, `CONNECTIONS=<uid> n m <linktype> ... !`,
  `HIB=1` (hibernating until woken), plus component blocks (`zombiebrain{NAVNAME=...}`, `corporeal{RES=...}`).
- **Templates** hold all the design data: components, `INITHLTH`, `SIGHTRANGE`, speeds, `corporeal.RES`/`ANIMS`.
  `tools/entity_templates.py main.prx decomp/ghidra_full > docs/entity_templates.txt` (read by the viewer).
- Model of an entity = `corporeal.RES` (`.../x.group`) -> `x.mesh` (static) or `x.skel` (animated).
- **SKEL** = `SKE0{SKL0 hierarchy, JNA0 joint names, BMA0 bind matrices}` + skinned `MOD0` (vtype 0x5A2/0x45A2/0x85A2:
  1-3 u16 weights; `JNT0` = per-fragment bone palette, 8 x u8, 0xFF unused) + `VOA3`, `ACT4`. Vertices are in
  model space, so the bind (T) pose renders without bones.
- **Vertex/matrix alignment**: FVT1 and INM1 data start at the next 16-byte boundary *relative to the asset
  start* (filler is 'PAD...' or zeros; some fragments also have trailing filler). Verified on 17 230 fragments.
- Graveyard: 81 entities, 70 drawn, 1 hibernating (gold under the diggable grave), 10 without a model.
- Vertices without a colour component use 0x80 (= 1.0 on PSP).

## 12. Skeletons and animation (`src/Core/Skeleton.*`)
- **SKL0**: `u16 n, u16 n`; n x 36 B `{quat x,y,z,w; vec3 pos; u8[8]}` local bind pose; then per joint its child
  list terminated by `0xFF`. **JNA0** joint names, **BMA0** n x `{quat; vec3}` = inverse world bind.
  Conventions verified to 3e-7: `local = T(pos)*R(quat)`, `world = parent*local`, `BMA0 * world = I`.
- **Skinned vertices** (vtype 0x5A2 / 0x45A2 / 0x85A2): 1-3 u16 weights (0x8000 = 1.0) first in the vertex;
  weight k uses joint `JNT0[k]` of the fragment. Vertices are in model space (bind pose).
- **ACT4** (`FUN_0019e300`, `FUN_0019f09c`): `u32 flags, u32, float`; track lists `ROTA`/`TRAN`/`SCAL`
  (`u32 n, u32 HashName129(joint)[n]`); `u32, float duration, float frameTime (1/15), u32 frames`; then
  rotations `[frame][track]` 4 x s16 / 32767 (x,y,z,w, aligned 8), translations 3 x float (flag 0x100) or
  4 x float (aligned 16), scales 4 x float (aligned 16). Frame order confirmed by smoothness (0.2 vs 151).
- **Keys are relative to the bind pose**: `rot = bindRot * key`, `pos = bindPos + key` (pelvis key ~(0,-0.07,0)
  vs bind height 1.16). Absolute interpretation lays characters on the ground / twists them.
- Flags seen: 0x500 (1292), 0x510 (1386, 0x10 = no loop), 0x512 / 0x502 (0x2 = extra per-frame track at
  +0xA4), 0x740 / 0x750 (5 anims, 0x40 = compressed rotation stream `FUN_001a1a38`, not supported yet).
- **ATBL**: 8-byte header, then 56-byte records `{u32 animHash, u32 state, floats...}`; state 85 (0x55) =
  wait (`zombie_guard_wait.anim`). The viewer plays `<model>_wait/_idle/...` or the state-85 entry.
