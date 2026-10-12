# MediEvil: Resurrection (PSP, UCES00052 EU) — нотатки реверсу

## 1. Образ диска
- ISO9660, том `MEDIEVIL`, 1.45 ГБ. Розпаковка: `python3 -I tools/iso_extract.py <iso> extracted`
  (bsdtar на macOS цей образ не читає). Повний список: `docs/iso_filelist.txt`.

## 2. Виконуваний код
| Файл | Що це |
|---|---|
| `SYSDIR/EBOOT.BIN` | `~PSP` зашифрований завантажувач `medboot` (18 КБ) |
| `SYSDIR/BOOT.BIN` | той самий `medboot` у відкритому ELF: вантажить PRX-модулі й `main.prx` |
| **`USRDIR/prx/main.prx`** | **уся гра**. Незашифрований ELF MIPS Allegrex (PRX, relocatable) |
| `USRDIR/prx/*.prx` | стандартні модулі Sony (atrac3+, mpeg, sascore, adhoc) — не портуються, замінюються |
| `USRDIR/UCES000{01,06,15}` | бонусні демо інших ігор (WipEout Pure, Ridge Racer?, …) — ігноруємо |

`main.prx`: `.text` 0x2C60C0 (~2.9 МБ коду, ~700k інструкцій), `.rodata` 0x77AC8, `.data` 0x12FD0,
`.bss` 0xEBE74. `.symtab` порожній — символів немає. Є `.ctors` (~640 статичних
конструкторів → C++), `.eh_frame`, `.gcc_except_table` → компілятор GCC (psp-gcc / SN).
Релокації `.rel.*` типу 0x700000A0 (PRX) — потрібні, щоб завантажити в Ghidra правильно.

Імпортовані бібліотеки (точки інтеграції для порту — HAL):
sceGe_user (графіка GE), sceDisplay, sceCtrl, sceAudio, sceSasCore, sceAtrac3plus,
sceMpeg (відео .pmf), IoFileMgrForUser, ThreadManForUser, sceUtility (сейви/діалоги),
sceRtc, scePower, sceUmdUser, sceNet* (adhoc мультиплеєр).

## 3. Архів ресурсів `USRDIR/gamedata.bin` (585 МБ)
Усі зсуви в секторах по 2048 байт.

**Заголовок:** `"TOCS"` u32 0x30, u32 81540, u32 1, далі 48 записів по 40 байт:
`char name[32]; u32 tocSize; u32 tocSector;` — одна група на рівень
(`global`, `01_danscrypt`, `02_graveyard`, … , тестові `ent_test`, `99_testbed`).

**TOC групи** (`"TOC0"`):
```
u32 magic 'TOC0'; u32 nBlocks; u32 nAssets; u32 0
Block  [nBlocks] 8B : u24 sector; u8 flags; u16 assetIndex; u16 nextCopy(0xFFFF=none)
Asset  [nAssets] 20B: u32 nameHash; u32 size; char type[4]; u16 kind(1=файл,2=група);
                      u16 block; u16 bstLeft; u16 bstRight
```
- `left/right` — бінарне дерево пошуку за `nameHash` (перевірено), ієрархії тут немає.
- Дані лежать з `Block[asset.block].sector`, **без стиснення**. Блоки з `assetIndex != i` — дублікати
  тих самих даних в інших місцях диска (для стрімінгу).
- Друга половина таблиці TOCS (індекси ≥ 28) повторює імена груп — ймовірно, копії для 2-го шару UMD
  або варіанти; не до кінця з'ясовано.

**Хеш імен** (`main.prx` `FUN_001be1e4`): `h = h*129 + (signed char)c`, старт 0, по імені файлу
з розширенням, без шляху. Тип = перші 4 символи розширення у верхньому регістрі (`FUN_00222310`).
`tools/namehash.py` відновлює імена зі словника рядків бінарника (129 непарне → суфікс можна
«відкусити» аналітично): зараз 1 263 / 7 788 унікальних хешів. Без імен поки INST/SUBZ/TCD/ENTS/TNV/VOL/LOC*.

Розпаковка: `python3 tools/gamedata_extract.py extracted/PSP_GAME/USRDIR/gamedata.bin assets --names docs/names.tsv`
→ `assets/<NN>_<група>/<TYPE>/<idx>_<ім'я>`, і `_index.csv` у кожній групі. 13 120 файлів, 253 МБ.

Магіки корисного навантаження: ANIM→`ACT4`, PSM→`PSM0` (текстура), INST→`BOX0`, SKEL→`SKE0`, MESH→`MOD0`,
SUBZ→`SBZ0`, ENTS→`ENT0`, LAND→`LND0`, VOL→`MAP0`, LOC/NLOC→`LON1`, RLOC→`LOR0`, TNV→`TNV%`, BNK→без магіка.

Перепис типів (21 634 записи):
SUBZ 60 МБ (підзони рівня), BNK 44 МБ (звукові банки), SKEL 41 МБ, PSM 39 МБ (ймовірно текстури/моделі
PSP), ANIM 23 МБ, TCD/TNV (колізія / навмеш?), MESH, FLIP (флібуки/UI), ENTS (сутності),
LAND/SNET, LOC/NLOC/RLOC (локалізація), SPL (сплайни), LIT/LTR/CCV (світло, колір).
Кожен тип ще треба розібрати окремо.

**Відео:** `USRDIR/movies/*.pmf` — стандартний PSMF (H.264 + ATRAC3+), конвертується ffmpeg.

## 4. Ghidra
- Ghidra 12.1.3 (brew) + розширення **ghidra-allegrex v21.4** (`~/Library/ghidra/ghidra_12.1.3_PUBLIC/Extensions`).
- Проєкт: `ghidra/MediEvil` → `main.prx`, лоадер «PSP Executable (ELF)», мова `Allegrex:LE:32`. Аналіз ~2 хв.
- Headless-скрипти в `tools/ghidra_scripts`:
  - `ExportOverview.java <outdir>` → `docs/ghidra/{functions,imports,strings,fourcc_refs}.tsv` (10 694 функції);
  - `Decompile.java <outfile> <depth> <addr|dataAddr>...` → C для функцій (або тих, що посилаються на дані).
  ```
  JAVA_HOME=/usr/local/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home \
  /usr/local/Cellar/ghidra/12.1.3/libexec/support/analyzeHeadless ghidra MediEvil -process main.prx \
    -noanalysis -readOnly -scriptPath tools/ghidra_scripts -postScript Decompile.java out.c 1 001be1e4
  ```
- Відомі функції: `001be1e4` hash129, `002723bc` strlen, `00272004` sprintf, `00272104` strcat,
  `0022223c` basename, `00222380/00222310` розширення→FourCC, `00222740` завантаження ассета за іменем
  (для ANIM пробує ще `<name>.comp`), `0020aba8` обробник чанків локалізації.
- TODO: імпорти sce* поки без імен (`FUN_002c6xxx` у `.sceStub.text.*`) — розставити NID.

## 5. План
1. ✅ Розпаковка ISO, ідентифікація виконуваного коду та архіву.
2. ✅ Екстрактор `gamedata.bin`, хеш імен знайдено (частково відновлені імена).
3. ✅ Ghidra + ghidra-allegrex, `main.prx` проаналізовано. ⏳ розмітити NID імпортів.
4. Знайти в коді парсери кожного типу ассетів (шукати FourCC-константи) → описати формати.
5. Вибір стратегії порту:
   - **(а) статична рекомпіляція** MIPS→C (як N64Recomp / psxrecomp) + HLE шар для sce* API та GE→OpenGL/Vulkan;
   - **(б) повний реверс до C++** функція за функцією (decomp-проєкт, роки роботи);
   - **(в) новий рушій** на розібраних форматах ресурсів (найшвидше дає видимий результат, але логіку гри писати заново).
   Рекомендація: почати з (а) — найреалістичніший шлях до гри, що запускається нативно.
