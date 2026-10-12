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

## 13. Zone lighting (LAND / LGT1)

The GE never doubles colours: TFUNC is always written without the 2x bit (texture DL `FUN_00239f38`,
`FUN_001ea014`) and the texture function is always modulate (`FUN_00239030` defaults). Levels are drawn
with GE lighting on and `sceGuColorMaterial(7)`, so the baked vertex colour is the material:

    colour = clamp(ambient * vcol + sum_i lightColour_i * vcol * max(dot(N, dir_i), 0)) * texture

- **LAND** = `LND0 { PTL0* (portals), ZNE0* }`, `ZNE0 { NAME, LGT1, ZDT0, WND0, RZN0 }`.
- **ZDT0**: `u32 0, u8 1,1,1, u8 zoneIndex, float min[3], float size[3], u32`. The zone boxes are
  exactly the SUBZ bounds, so each sub-zone maps to one zone.
- **LGT1** (`FUN_001f3920`): the payload starts at the next 16-byte boundary relative to the asset start.
  It holds vec4s: object ambient, fog colour (0..255), fog near/far, world ambient, light colour ×2,
  light direction ×2, then `u8 on[2]`. LGT0 has no leading object ambient.
- **`FUN_00216a58`** (static time of day) puts these in globals. **`FUN_0021757c`** builds the list of
  enabled directional lights. **`FUN_00216cb4`** emits `sceGuLight(i, DIRECTIONAL, DIFFUSE, dir)` and
  sets the light diffuse colour and `sceGuAmbient`: world ambient for sub-zones, object ambient for
  models. The direction is the GE light vector, pointing *towards* the light. For example, Dan's Crypt
  lava room light 0 is (0.12, -0.99, 0.11) in orange: the glow from the lava below.
- **`FUN_001f7038`** (sub-zone draw) also adds up to 4−n point and spot lights from the LIT0 lights,
  with attenuation `sceGuLightAtt(0, 0, 1/r²)`. Not ported yet.
- Fog: `sceGuFog(near, far, fogColour)`, linear in view depth.
- Dan's Crypt lives in the global group (group 0, `01_danscrypt.land`); the lava is `lavabubble_flip.flip`.

## 14. Blending, texture alpha, shared animation sets

- **Blend mode** = `(MFL1 >> 3) & 0xFF`, stored at holder+8 by `FUN_002182bc`. `FUN_00212adc` (holder
  draw) passes it to `FUN_001ee028`, which calls `sceGuBlendFunc(op, src, dst, srcFix, dstFix)` and
  enables `GU_BLEND` when it is non-zero:

  | mode | op | src | dst | meaning |
  |---|---|---|---|---|
  | 0x00 | – | – | – | blending off |
  | 0x44 | ADD | SRC_ALPHA | 1-SRC_ALPHA | alpha blend (characters: MFL1 0xfa20) |
  | 0x01 | ADD | 1-SRC_ALPHA | SRC_ALPHA | as decoded; also set on opaque geometry with vertex alpha 255 (mausoleum arches, `0x0100f808`), so the viewer treats it as 0x44 for now |
  | 0x48 | ADD | SRC_ALPHA | FIX white | additive |
  | 0x58 | ADD | DST_ALPHA | FIX white | additive (destination alpha = stencil on the PSP) |
  | 0x54 | ADD | DST_ALPHA | 1-DST_ALPHA | |
  | 0x42 | REV_SUB | FIX white | SRC_ALPHA | subtractive |
  | 0x52 | REV_SUB | FIX white | DST_ALPHA | subtractive |

- `FUN_00212adc` also enables culling unless MFL1 bit 1 (holder flag 4) is set.
- **Texture alpha**: when PSM byte 0x0C == 1, texture flag 0x400 is set (`FUN_002391d4`), and
  TFUNC gets TCC = RGBA (`FUN_00239f38`). Otherwise the GE ignores the texture alpha, and alpha comes
  from the vertex colour only. Light-beam textures (`sf_light_beam`, `hh_light_ray`) have TCC off, with
  a vertex alpha of 0x3e–0x80.
- The texture function is always modulate. The texture DL resets UV scale/offset to 1/0
  (`0x483f8000`, `0x493f8000`, `0x4a/0x4b`).
- Viewer sorting: holders with blending on go to the translucent pass (no depth writes), except
  alpha-blended holders that have opaque vertices and a cut-out texture (fewer than half of the
  texels semi-transparent; character atlases have 13–15% soft edges). Those are drawn alpha-tested
  with depth writes, otherwise the parts of a character sort wrongly.
- **ANIMS**: the corporeal component `ANIMS = .../x_apkg.group` names the animation set
  (`x_*.anim`, `x.atbl`). Models share sets: `zombie_stalker_2` uses `zombie_stalker_1`'s.

## 15. Point lights (LIT)

- The asset is `"<subzone>.lit"`: hash it as `NameHash(".lit", NameHashStripSuffix(subzHash, ".subz"))`.
  This matches 32 sub-zones (19 unique LIT assets); sub-zones without lights have no LIT.
- LIT0 (`FUN_001f52c0`) holds a `u32` count, then 0x40-byte records at 16-byte aligned asset offsets
  (the first at `(p + 0x17) & ~15`). Record layout:
  - +0x10: position xyz, plus `r`
  - +0x20: colour
  - +0x30: `u8` flags (0x10 = spot; it then uses +0x50 direction / `FUN_001e9a94`)
- The colour (1,0,1) is a marker. It is replaced by (1, .875, .5) with flag 8 set (probably flicker).
- In the sub-zone draw (`FUN_001f7038`), the GE has 4 lights: the enabled directional zone lights
  come first, then up to 4−n LIT lights in file order.
- Each LIT light gets its diffuse colour, with ambient black, and `sceGuLightAtt(0, 0, 1/(r²)²)`.
  `FUN_00217868` stores r² at +0x3c. The effective attenuation is `min(1, r⁴/d²)`.
- Dan's Crypt lava room: one light at (−60.0, −0.6, −19.7), r 2.03, colour (1, .74, 0).

## 16. Resident TOC and Sir Daniel

- **TOCS header**: `u32 'TOCS', u32 groupCount (48), u32 residentTocSize, u32 residentTocSector (1)`,
  then the group table. Sector 1 holds a **resident TOC0** of 2889 assets that the 48 groups do not
  list: Dan (`dan.skel`, `dan.atbl`, 205 ANIMs), shared effects, and more. `GameData` appends it as the
  last group, named "resident".
- Dan is spawned by code, not by ENTS. Strings in `main.prx`: `RES=exporteddata/characters/dan/dan.group`,
  `ANIMS=.../dan_apkg.group`, and `PLUGIN=<DAN_TRACKING PERS=1 VIEWLEN=4.25 MAX_VIEWLEN=4.5 Y_OFFSET=2.5>`
  (the camera).
- `dan.skel` has 42 joints (`weapon_r` = 17, `shield` = 12) and 55 holders:
  - `dan_*`: the default body;
  - `dad_*`: a costume variant;
  - `captainshat`, `wooden_leg`, `spade`, `TorsoShape1`: props and costume parts that the game
    switches on per level.
- **Holder joint**: `((MFL1 >> 16) & 0x7F) − 1` is the joint that the holder is rigidly attached to
  (holder+9; `FUN_00218be0` multiplies by `DAT_003ee9b0 + joint*0x40`). Verified by name: thigh_l →
  hip_l, head → head, spade → wrist_r. Fragments without weights follow that joint.
- **dan.atbl** (179 records): `{animHash, state, float, float variant, float type, ...}`. The ANIM names
  are not recovered. The states were identified with `medievil-viewer --atbl-report dan`: it plays
  every record and reports the velocity of the planted ankle, which is minus the body velocity, plus
  the pelvis height range and the facing:

  | state | meaning |
  |---|---|
  | 4 | idle (var 0; 1 = stop, 110 = idle variant, 200/201 = long "bored" idles) |
  | 0 | forward: var 0 walk 2.06 m/s, 1 run 2.39 m/s (bigger bob), 2 fast run 3.26 m/s |
  | 1 | backward (1.83 m/s) |
  | 2 / 3 | strafe +X / −X |
  | 44 / 45 | forward diagonals +X / −X |
  | 46 / 47 | backward diagonals |
  | 64 | jump: var 0 take-off (0.2 s), 1 airborne (0.13 s, held), 2 landing (pelvis drops to 0.83), 3/4 loops; 10–14 / 20–24 repeat the set |
  | 28 | attacks, type = weapon. Type 2 (one-handed sword): var 0, 1, 2 = three-hit combo (0.83 / 0.80 / 1.8 s), 10–12 / 50–56 / 70–72 other sets, 100–102 loops |
  | 38 | hit reactions |
  | 97 | crouch (pelvis 0.66) |
  | 41 | stationary loop |
  | 35 | climbing / special (pelvis 0.1–2.95) |

  The animations have no root motion and the model faces +Z. The viewer plays them at speed / natural
  speed, blending walk and run on a shared phase.
- Enemy reactions use the recovered names `<anims>_trip.anim` (hit) and `<anims>_death.anim`.
- Weapons are separate meshes on joint `weapon_r`, e.g. `all_sword_short.mesh`. Joint world matrix =
  `palette[j] * inverse(invBind[j])`.

## 17. Collision (TCD)

- The asset is `"<subzone>.tcd"` (228 of 245 sub-zones), in world space. TCD0 header (0x28 bytes):
  - `u32 ?`, `u32 triOffset`;
  - `vec3 centre`, `float halfSize`;
  - `u32 vertOffset`, `u32 0`.
- After the header comes an octree of u16 entries (0x8000 = child node).
- Triangles start at `triOffset`, as `{u16 a, b, c, u16 flags}`; the high nibble of `flags` is probably
  the surface type.
- Vertices are `float3` from `vertOffset` to the end of the file; `vertOffset` can follow padding.
  Validated on all 228 files.
- The viewer ignores the octree. It builds an XZ grid and runs a simple character controller:
  - ground ray, with a 0.45 m step;
  - two wall spheres;
  - camera pull-in.

  This is not yet a port of the game's own movement code (DanBrain).

## 18. Dan control code (work in progress)

Located, not yet ported (the current `Core/Player` is a placeholder controller, NOT the game's logic):

- Pad: `FUN_001ea3bc` (sceCtrlReadBufferPositive). Pad struct +0x18 held, +0x1c pressed, +0x20 released,
  +0xdc/+0xe0 stick axes ((v-128)*0.007874016, -1..1), deadzone `FUN_001ea594`.
- danbrain: registered in `FUN_000946e4`, size 0x1900, factory `FUN_00295548`, ctor `FUN_000879b8`,
  vtable 0x329fc0 (slots: 00088310 00087c3c 00087d78 001dae5c 00088564 0028adcc 00088860 00088868 0028add4
  001c6a78 0028affc 0028b004 000884e0 001db2a8 001c6fc4 001db434 0028b00c 0028b014 0028ceac 0028b01c
  0028b050 00093870 00093974 001c7004). Code ~0x879b8-0x95000.
- dananimator: registered in `FUN_0008781c`, size 0x1090, factory `FUN_00295378`, ctor `FUN_0007f798`,
  vtable 0x329f08. Code ~0x7f798-0x8781c. Update `FUN_00080564`: components via `FUN_001d4d94(ent, 1/7/3/10)`
  -> +0x107c/+0x1080/+0x1084/+0x1088, then calls 00086754 0008645c 00080bd8 00081684 0008313c 00084058
  00083284 0008349c 00083564 000835ec 00083674 00083970, 000855fc(+0xf40,+0xf30), 000855fc(+0xf44,+0xf34),
  00085978 000852a8.
- Not found yet: the mapping from animator states to dan.atbl (state, variant). `FUN_0001149c` is an item
  counter, not anim code.
- Firefly: sfx_fairy_glow_abe.psm / sfx_fairy_trail_abe.psm, used in `FUN_00140374` / `FUN_00151048`.

## 19. Ghidra -> C++ transpiler (whole main.prx)

Instead of porting functions by hand, every function Ghidra decompiles is translated to C++ automatically.

1. `tools/ghidra_scripts/ExportForTranspile.java` (headless, ~10 min) writes `decomp/transpile_in/<addr>.c` with
   metadata: `//@G` globals with the type Ghidra used in that function, `//@L` stack locals with their offset
   from the entry sp, plus `functions.tsv`.
   `analyzeHeadless ghidra MediEvil -process main.prx -noanalysis -readOnly -scriptPath tools/ghidra_scripts -postScript ExportForTranspile.java decomp/transpile_in`
2. `tools/transpile_build.sh` runs `tools/transpile.py` -> `decomp/cpp/` (128 chunks, `decls.h`, `thunks.cpp`,
   `imports.cpp`) and syntax-checks every chunk; `tools/transpile_fix.py` maps compiler errors to functions and
   adds them to `decomp/cpp/stubbed.txt` (they become `Unimplemented()` calls). Repeat until 0 errors.
3. CMake target `medievil-game` (only when `decomp/cpp` exists; it is derived from the binary and gitignored).

Runtime model (`src/Decomp/Runtime.h`):
- main.prx is loaded at guest address 0 (= Ghidra image base, PRX relocations become no-ops), 64 MB guest space,
  heap from 0x800000, stack below 0x3E00000, inlined string literals interned in the last MB (`Memory.h`).
- `GP<T>` = 32-bit guest pointer (pointer arithmetic scaled by guest sizes; `*GP<VB<N>>` is a `VRef` that reads
  as a value and decays to its address like a C array). `U4` = `undefined4`, keeps bits on float<->int moves.
- Stack locals (`local_XX`, `auStack_XX`) live in a guest `Frame` so their addresses can be passed on.
- `x._A_B_` -> `Fld<A,B>(x)`. Calls go through `Caller<&f_addr>` (adapts arg count/types); calls by address
  (vtables, function pointers) go through `ICall` -> per-function thunks fed an EABI register image
  (ints a0-a3/t0-t3, floats f12-f19).
- VFPU: Ghidra keeps `vpfxs/vpfxt/vpfxd` as calls in program order, so the runtime keeps prefix state and the next
  VFPU op consumes it. Lane encoding (ghidra-allegrex `Vpfxst`): bit4 neg, bit3 abs, bit2 constant, bits0-1
  swizzle; constant lanes are 0, 1, 2, 1/2 (Ghidra's encoding cannot tell them from 3, 1/3, 1/4, 1/6). d lanes:
  bit2 write mask (not honoured), 2 = saturate [0,1], 4 = [-1,1]. `vsin/vcos` take quarter turns (callers
  pre-multiply by `vcst 5` = 2/pi). Matrices are 64-byte column-major values.
- Imports (`/* lib NID 0x... */` 8-byte stubs) are weak `U4 f_addr(U4 a0..a7)` stubs; define the same symbol in a
  hand-written file to implement one.

- Float/int casts: Ghidra prints a bit reinterpretation (p-code CAST) and a numeric conversion (TRUNC,
  INT2FLOAT) the same way. The export prefixes reinterpreting casts with `__bc_` (the CAST op hangs on the
  operand token, or on the op whose output feeds it); the transpiler turns them into `BC<T>`; an unmarked
  `(float)` becomes `NumF` (numeric; `undefined4` sources are rereads).
- Register pass-through: a wrapper such as newlib `acosf` calls `__ieee754_acosf()` with no arguments because
  f12 is never touched. Calls with fewer arguments than the callee takes get the caller's own parameter in the
  same EABI register (an `undefined4` parameter may stand for a float register).
- MIPS EABI 64-bit argument alignment: 64-bit integers (`long long`, `SceOff`) require alignment to even-odd register pairs ($a2:$a3). In calls like `sceIoLseek(int fd, SceOff offset, int whence)`, $a0 is fd, $a1 is skipped padding, $a2:$a3 holds offset, and $t0 holds whence. Ghidra prototypes now explicitly model this (`int fd, int _pad, uint offset_lo, uint offset_hi, int whence`), fixing seek offsets from corrupting TOC reads.
- `Boot()` loads the image, `RunStaticCtors()` runs `.ctors` (0x3527a8, 642 entries, all 640 run cleanly).
  634 functions that only `.ctors`/vtables point at were created with `CreateFunctions.java`
  (`?addr` = only if no function contains it, to keep switch targets intact).

Status: 16368 functions, 191 auto-stubbed (1.2%), 195 imports; build ~90 s with 10 cores.
`medievil-gametest` checks the translation against hand calculation: danbrain speed curve `FUN_0008ce24`, the
game's acosf (table `0x3de210` from `FUN_0020f284`), and the pad stick filter `FUN_001ea594` (deadzone
0.236, square-to-circle on diagonals, angle 0 = up, pi/2 = right).
Pad button table 0x33f380 (8 actions x 2 layouts): 0 Triangle, 1 Square/Cross, 2 Cross/Square, 3 Circle, 4 R,
5 L, 6 Select, 7 Start. Danbrain posts messages to components instead of moving Dan itself (type 6 = move to
point at speed, type 1 = stop; `FUN_001c5c98`), see `FUN_0008a854`.

### 19.1 Running main() natively (platform layer)

- `medievil-gametest <prx> main` runs the game's `main` (0x20cf8) after `.ctors`. Guest memory reserves the whole
  4 GB (wild pointers read zeros); a crash or SIGTERM prints the last guest calls; `tools/hangbt.sh` attaches
  lldb and prints the host stack (generated functions are ordinary C++ frames).
- `src/Decomp/Platform.cpp`: thin native versions of PSP calls (files, memory, time, pad, UMD, sync objects).
  Threads are not started (logged); heavy subsystems (GE, audio, video) are to be replaced at the level of the
  game's own functions, not emulated. `PSP_TRACE=1` logs the calls.
- Disc files: `sceIoGetstat` returns a made-up sector in `st_private[0]`; the game then opens the whole file raw as
  `disc0:/sce_lbn0x<lbn>_size0x<size>` (gamedata.bin is read that way).
- Prototypes: `docs/names_functions/psp_prototypes.tsv` + `ApplyPrototypes.java`. 64-bit arguments sit in an
  aligned pair (a2:a3 for lseek, a1 skipped, whence in t0), modelled with explicit `_pad`/lo/hi parameters.
- Ghidra keeps auto-analysis prototypes (often `void f(void)`) at call sites even when the callee's decompile
  shows parameters/returns: `CommitSignatures.java` (run twice) commits the decompiler's view for every
  function. Without it arguments passed in registers get lost (e.g. the asset count `a2` for `FUN_00221cf4`).
- Ghidra-only `stack0xNNNN` (address taken, no variable): `CreateStackLocals.java` defines locals there.
- Transpiler heuristics still applied: extra caller arguments extend a callee's parameter list and pass through
  to its inner calls; a `void` function whose result is used returns its last call's value (v0).
