# Comanche: Maximum Overkill — sources, method, contradictions, and what was not found

> Every claim in [`campaign.md`](campaign.md), [`terrain.md`](terrain.md) and [`hud.md`](hud.md) is
> answerable from this file. Contradictions between sources are **kept, not smoothed**, per
> [`doc/mods.md`](../../../doc/mods.md) §3.

## Spec

### 1. Source ranking, and what each one actually delivered

| Rank | Class | Source | What it gave | What it could not give |
|---|---|---|---|---|
| 1 | **Manual PDF** | `MANUAL.PDF` on the Comanche CD, **139 pages**, NovaLogic `USER'S MANUAL` | the entire cockpit, HID, TAC pages, weapons, damage model, controls, aircraft and enemy specifications; the **campaign list** (10 named operations) | **no mission names, no mission list, no objective, no map, no scale, no place** — the manual describes campaigns in one paragraph each |
| 2 | **Game files** | the shipped data of the **1992 three-floppy release** | **everything the manual lacks**: 20 mission names, 20 briefings verbatim, every object with type / position / heading / goal flag, per-mission loadout and fuel, the four terrain maps cell for cell, the night palettes | behaviour; the meaning of six object fields and two parameter lines |
| 3 | **Game files (CD)** | `COMANCHECD.ISO`, 43 175 936 B | the manual PDF itself; the campaign inventory `0.MIS`–`9.MIS` that fixes the release accounting (§3) | the CD's `.MIS` use a **different, undecoded** obfuscation |
| 4 | Booklet | *Comanche Maximum Overkill CD — Installation Booklet*, 16 pp. | the CD's "100 missions grouped into 10 campaigns", the engine self-description, the key chart | nothing about the 1992 release |
| 5 | Let's Play with a visible mission list | — | **none found**, §5 | |
| 6 | Wiki | Wikipedia | release accounting for the mission disks (§3); the geographic anchor coordinates (§4) | nothing about missions |
| 7 | MobyGames | — | **HTTP 403** on both pages tried | |

**The ranking inverted in practice.** The manual is authoritative on the aircraft and worthless on the
campaign; the binaries are the only campaign source that exists.

### 2. Method — reproducible, step by step

#### 2.1 Obtaining the 1992 release

`https://archive.org/details/001758-ComancheMaximumOverkill` → `001758_comanche.7z`, 4 147 030 B,
three 1 474 560-byte floppy images. `archive.org/download/…` returned **HTTP 500**; the per-item node
from `https://archive.org/metadata/<id>` (`d1` + `dir`) served the file.

Each image is FAT12, 512 B/sector, 1 sector/cluster, 2 FATs of 9 sectors, 224 root entries. Root
directories hold `OVERKILL.000`, `.001`, `.002` plus `INSTALL.BAT`, whose content is the reassembly
recipe:

```
copy /b overkill.000+overkill.001+overkill.002 overkill.exe
```

Concatenated → 4 059 001 B LHA self-extractor → **92 files, 6.9 MB, all dated 29 Oct 1992**.

#### 2.2 De-obfuscating the campaign files — `1.MIS`

`0.MIS` (11 196 B) and `1.MIS` (13 200 B) contain no readable string. Both begin with byte `0x78` and
are almost entirely in the printable byte range, which rules out compression and points at a stream
cipher over text.

Key recovery, by known plaintext rather than by guessing:

1. Byte-frequency analysis per residue class for key lengths 1–64 gives its best English score at
   **period 4** and produces `wC|i~mxecb,Amteaya,Czi~ge``q…` — visibly the shape of
   `Operation Maximum Overkill` but off by a constant.
2. Assume the plaintext of `1.MIS` at offset 1 is exactly `Operation Maximum Overkill` and XOR:

```
cipher[1..26] = 49 62 74 71 67 66 78 6c 68 32 5c 62 7e 7b 7c 76 6b 32 5e 75 63 60 7a 6a 6a 7e
plain         = "O  p  e  r  a  t  i  o  n  ␠  M  a  x  i  m  u  m  ␠  O  v  e  r  k  i  l  l"
xor           = 06 12 11 03 06 12 11 03 06 12 11 03 06 12 11 03 06 12 11 03 06 12 11 03 06 12
```

3. The result is periodic with period 4 and no residual. Aligned to offset 0:

```
plain[i] = cipher[i] XOR K[i mod 4]        K = 03 06 12 11
```

4. **Verification, not assertion**: at offset 0 this yields `{`, at 27 `}`, at 28–29 `0D 0A`. Every
   record in both files then terminates in `\r\n`, every name is enclosed in `{}`, and the whole of
   both campaign files becomes clean ASCII. A wrong key cannot produce a self-consistent record
   structure across 24 396 bytes.

**Re-executed end to end when the missions were built** (`.7z` 4 147 030 B → three FAT12 images →
`overkill.exe` 4 059 001 B → 92 LHA members → `1.MIS` 13 200 B → the key above). Every number this
file and [`campaign.md`](campaign.md) state about `1.MIS` came back identical, the ten goal counts
26 / 17 / 5 / 14 / 3 / 4 / 11 / 19 / 16 / 12 included — so the reconstruction is reproducible from the
recipe alone, and nothing in `mods/comanche/src/` rests on a cached artefact.

The decoded record format:

```
{Mission Name}[lock marker]
briefing text, |r |g |o |y colour codes, ^ = line break
<
18 asset filenames, one per line
<
night flag
player start X,Y,heading
two equal numbers            (not decoded)
cannon,rockets,Hellfire,Stinger,artillery,wingman
fuel
nine values                  (not decoded)
eleven values                (identical in all 20 missions)
[*]type,a,b,c,X,Y,heading,d,e,f,delay      ← one line per world object
<
```

#### 2.3 Decoding the terrain and cockpit art

`Kyle DTA` container: 8-byte magic, `uint16` width−1 at offset 8, `uint16` height−1 at offset 10,
payload from **0x80**, PCX-style RLE (`b ≥ 0xC0` → run of `b & 0x3F`, next byte is the value),
**768-byte VGA palette appended at the end of the file**.

Self-check: all eight terrain files decode to **exactly 1 048 576 bytes** with exactly 769 bytes
unread (768 palette + 1 terminator). `CONSOL1S.DTA` declares 328 × 290 and decodes to exactly 95 120
bytes — and its width was **independently confirmed at 328** by minimising mean row-to-row difference
over candidate widths 280–420 before the header field was understood.

Palette placement was likewise verified rather than assumed: the last 768 bytes of `C1.DTA` are
byte-identical to the 48 bytes visible at offset 0x10, i.e. the header carries a copy of the palette's
head.

#### 2.4 Deriving what is not stated

| Quantity | Method | Section |
|---|---|---|
| goal flag = leading `*` | starred sets match the stated objective of all 20 briefings; manual's Mission Status Display counts goals and flashes their map markers | [`campaign.md`](campaign.md) §5 |
| loadout field order | three independent text-to-number checks; wingman field correlates 10/10 with the presence of a type-4 object | [`campaign.md`](campaign.md) §5.1 |
| unit types 1–6 | four single-type training missions whose briefings name the unit | [`campaign.md`](campaign.md) §6 |
| map orientation and heading zero | wingman spawned astern in 5 of 6 missions | [`campaign.md`](campaign.md) §7 |
| vertical scale | 500 ft ceiling `[MAN p.36]` against the measured peak height byte 121 | [`terrain.md`](terrain.md) §3 |
| horizontal scale | reinforcement delay against range, cross-checked against terrain slope | [`terrain.md`](terrain.md) §4 |

### 3. Release accounting — closed

Four independent sources agree on a total that no single one states:

| Release | Year | Campaigns | Evidence |
|---|---|---|---|
| **Comanche: Maximum Overkill** | 1992 | **2** — *Comanche Training*, *Operation Maximum Overkill* | **measured**: the floppy set contains `0.MIS` and `1.MIS` and no other campaign file |
| *Global Challenge* (Mission Disk 1) | 1993 | **3** | Wikipedia, *"three new campaigns"* |
| *Over the Edge* (Mission Disk 2) | 1993 | **4** | Wikipedia, *"four new campaigns"*, *"40 tough new missions"* |
| *Comanche CD* | 1994 | **+1** | Wikipedia, *"plus 10 bonus missions"* |
| **Total** | | **10** | **measured**: `CMOBASE` on the CD contains `0.MIS`–`9.MIS`, and the manual lists exactly 10 named campaigns `[MAN p.23–28]` |

`2 + 3 + 4 + 1 = 10 campaigns × 10 missions = 100`, which is exactly the Installation Booklet's
*"Multiple Missions 100 missions grouped into 10 campaigns."* The manual's phrase *"the least
challenging of the **four new** campaigns"* about Silver Dove `[MAN p.27]` fits Over the Edge's four and
excludes the CD's Zephyr.

**Consequence for this mod: the 1992 game is 20 missions, and the campaign under study is 10 of them.**
The eight campaigns from Overload onwards are later products and are outside this reconstruction.

Mapping of the CD's campaign files to the manual's list `[MAN p.23–28]`, by position:

| File | Manual entry | Belongs to |
|---|---|---|
| `0.MIS` | 1. Comanche Training Missions | **1992** |
| `1.MIS` | 2. **Operation Maximum Overkill** | **1992** |
| `2.MIS` `3.MIS` `4.MIS` | 3. Overload · 4. Restore Peace · 5. Clean Sweep | Global Challenge |
| `5.MIS` `6.MIS` `7.MIS` `8.MIS` | 6. Silver Dove · 7. Whirlwind · 8. Over the Edge · 9. Terminal Velocity | Over the Edge |
| `9.MIS` | 10. Zephyr | Comanche CD |

The mapping of files 2–9 is **positional inference**, not measurement — the CD's `.MIS` are obfuscated
differently and were not decoded. Files 0 and 1 *are* measured: they match the 1992 originals to within
12 and 24 bytes.

### 4. Full source list

**Game data**

1. **1992 three-floppy release** — `https://archive.org/details/001758-ComancheMaximumOverkill`
   (`001758_comanche.7z`, 4 147 030 B; three 1.44 MB images; extracted contents all dated 29 Oct 1992).
   **Primary source for the entire campaign.**
2. **Comanche CD (DOS)** — `https://archive.org/details/comanchecd` (`COMANCHECD.ISO`, 43 175 936 B).
   Source of the manual PDF and of the 10-campaign inventory.

**Documentation**

3. **NovaLogic Comanche `USER'S MANUAL`** — `MANUAL.PDF` in the ISO root, **139 pages**, PDF 1.1,
   Adobe PageMaker 6.0, created 23 Jul 1996. Copyright line reads **© 1992 NovaLogic, Inc.** `[p.5]`.
   Pages used: **2** (design intent, 500 ft) · **5** (copyright) · **11–14** (fiction) · **15–22**
   (flight, controls, night) · **23–28** (campaign list) · **29–34** (cockpit, views) · **35–37** (HID)
   · **38–46** (TAC monitors) · **47–53** (weapons) · **54–58** (gauges, damage) · **59–74** (menus,
   options) · **109–111** (RAH-66) · **112–114** (Ka-50) · **115–117** (Mi-24, Hughes 500MD) ·
   **118–124** (ground and naval).
   Printed page numbers coincide with PDF page indices; checked at p. 30.
   **Caveat, stated because it matters:** this is the **1996 electronic layout of the CD manual**, not
   the 1992 printed manual (80 pp. per dealer listings). Content that describes CD-only units
   (Mi-24, Hughes 500MD, Scud-B, BRDM-3, OSA II, Lebed) or CD-only campaigns cannot be assumed present
   in 1992 — and §6 shows those units' sprites are indeed absent from the 1992 file set.
4. *Comanche Maximum Overkill CD — Installation Booklet* —
   `https://archive.org/details/comanchemaximumoverkillcdinstallationbooklet`
   (OCR full text `…_djvu.txt`). Used for the engine self-description, the 100/10 count and the key
   chart.
5. *Comanche Maximum Overkill CD — Read This First* —
   `https://archive.org/details/comanchemaximumoverkillcdreadthisfirst`. Nothing used.

**Encyclopaedic**

6. `https://en.wikipedia.org/wiki/Comanche:_Maximum_Overkill` — 1992, MS-DOS, NovaLogic; Mac OS port
   1995; two mission disks; Comanche CD 1994; cancelled SNES Super FX port. **Does not state mission or
   campaign counts.**
7. `https://en.wikipedia.org/wiki/Comanche_(video_game_series)` — the mission-disk campaign counts used
   in §3.

**Geography** (anchors only; all four are choices, see [`terrain.md`](terrain.md) §6)

8. `https://en.wikipedia.org/wiki/Kīlauea` — 19.421097472 N, 155.286762433 W
9. `https://en.wikipedia.org/wiki/Kaʻū_Desert` — 19.40861 N, 155.29667 W *(map 3 anchor)*
10. `https://en.wikipedia.org/wiki/Moyobamba` — 6.033 S, 76.967 W *(map 1 anchor)*
11. `https://en.wikipedia.org/wiki/Canyonlands_National_Park` — 38.16691 N, 109.75966 W; Green River
    confluence 38.18917 N, 109.88528 W *(map 2 anchor)*
12. `https://en.wikipedia.org/wiki/Khash_Rod_District` — 31.7773 N, 62.9724 E *(map 4 anchor)*

**Not reachable**

13. `https://www.mobygames.com/game/5078/comanche-maximum-overkill/` and
    `…/12557/comanche-maximum-overkill-mission-disk-1/` — **HTTP 403** on both.

### 5. Let's Play with a visible mission sequence — not found

Searched for the mission names against the open web. **Zero hits.** No wiki, no walkthrough, no video
description, no forum post anywhere reachable names a single mission of *Operation Maximum Overkill*.

**That is a statement about the record, not a failure.** The names in [`campaign.md`](campaign.md) §2
appear to be published here for the first time outside the game's own binary, and they are therefore
uncorroborated by any second source. The mitigation is that they are *measured*, from a file whose
structure self-verifies (§2.2) — but a second witness would be better and there is none.

### 6. Contradictions, kept

| # | The contradiction | Both sides | Position taken |
|---|---|---|---|
| 1 | **"100 missions in 10 campaigns"** vs **20 missions in 2 campaigns** | Installation Booklet (CD) vs measured floppy file set | Not a contradiction once §3 is closed: the booklet describes the 1994 CD. Recorded because every secondary source repeats the 100 without saying which product it belongs to |
| 2 | **Manual describes units the 1992 game does not have** — Mi-24 Hind-E, Hughes 500MD, Scud-B, BRDM-3, OSA II, Lebed, M1A1 `[MAN p.115–124]` | manual vs the 1992 sprite set (`tank turr radr fuel hoke lh66`, six files) | **The files win.** The 1992 hostile inventory is Ka-50, T-80, SA-8, fuel tank. The manual is the CD's |
| 3 | **Map 1: "lush green hills of Peru"** vs **a Mesoamerican stepped pyramid** on the same map, in a mission titled *"Mayan Malay"* | game text vs game art, both 1992 | Neither corrected. Peru chosen as the anchor because it is the only place the game names; the alternative (Petén) is recorded beside it — [`terrain.md`](terrain.md) §6.1 |
| 4 | **Hellfire standoff "greater than 8 km"** `[MAN p.49]` vs a **10.24 km** world | manual vs derived scale | Unresolved, and it may be the scale that is wrong. Recorded at [`terrain.md`](terrain.md) §4.3 |
| 5 | **"Every Werewolf within 50 klicks"** vs a measured farthest Werewolf at **2.4 km** | briefing vs data | The briefing is hyperbole. It bounds nothing |
| 6 | **The manual's fiction is a 1999 Pakistan–India nuclear crisis** `[MAN p.11–14]` vs **the 1992 campaign's enemy is "the KGB"** `[MIS]` | manual prose vs mission text | Both stand. The fiction is by David R. Holmes and frames the CD; no mission of `1.MIS` mentions Pakistan, India or 1999 |
| 7 | **`Volcanic Nightmare` puts the wingman ahead of the player**, where the other five put him astern | data vs data | Left as measured; it is the one exception in the orientation derivation, [`campaign.md`](campaign.md) §7 |
| 8 | **`Tactical Run` starts the player at cell (0, 0)**, the map corner, 830 cells from its nearest goal | data | Left as measured. Whether the engine reads `0,0,0` as "unset" is unknown |
| 9 | **`Valley of Instant Death` loads the night music and night sky but the day object file and night flag 0** | data vs data | Left as measured. It is the only mission where the four night indicators disagree |

### 7. What was not found or not done

| Item | Status |
|---|---|
| Mission list in the **manual** | **demonstrably absent** — the campaign section `[MAN p.23–28]` gives one paragraph per campaign and names no mission |
| The **1992 printed manual** (80 pp.) | **not obtained.** Only the 1996 CD electronic manual. Any claim that a passage was in the 1992 book is unmade |
| Let's Play with a mission sequence | **not found**, §5 |
| MobyGames | **HTTP 403** |
| Object-record fields `a b c d e f` | **not decoded** |
| Parameter lines 3 and 6 (`24,24` and `5,3,5,3,3,3,6,3,30`) | **not decoded** |
| The `*1` / `*0` digit on each campaign's last mission | **not decoded** |
| The CD's `.MIS` obfuscation | **not broken.** Tested: the known plaintext `{Operation Maximum Overkill}\r\n` against the CD's `1.MIS` at every start offset 0–63 yields no period-4 key. The scheme is not the floppy's |
| `HUDS.RLE`, `CONS.RLE`, all `*.RLE` sprites | **not decoded.** Different container (`KRL1`) |
| `SPEAK0.RLE` / `SPEAK1.RLE` (211 kB of speech) | **not decoded.** Would give the radio call inventory |
| `DEMO0–3.SCR` (attract-mode scripts) | **not examined.** Likely a recorded flight path — i.e. exactly the "trace" artefact of [`doc/mods.md`](../../../doc/mods.md) §2.1, in a 1992 file |
| `COMANCHE.EXE` (61 154 B) | **not disassembled.** It is a loader; the 32-bit body is packed |
| Enemy AI behaviour | **nothing found in any source** |
| Frame/loop rate | **not established.** 60 Hz and 70 Hz both plausible; it moves the derived scale by 17 % |
| Whether the CD altered campaign 1 | **untested.** `1.MIS` on the CD is 24 bytes longer |
| Terrain rendered from the pilot's viewpoint | **not done.** All terrain description is from top-down decodes |

### 8. Hygiene

Everything downloaded for this run — floppy images, the 43 MB CD ISO, the extracted 1992 file set, the
manual PDF, decoded maps and images — lived in the session scratchpad and was **deleted on completion**.
**No original game file is committed to `mods/`.** This directory contains reconstruction only.

## State

Four documents. No `mod.json`, no `src/`, no `.fbm`, no code.

## Gaps

- **Single-witness problem.** The campaign rests entirely on one artefact — one dump of one release on
  one archive. The decode self-verifies, but a second dump has not been checked against it.
- **The 1992 printed manual is unread**, so every `[MAN]` citation is to a 1996 document about a
  superset product. Where the two could differ, this reconstruction cannot tell.
- **Nothing here has been checked by running the game.** No emulator run, no screenshot of the mission
  select screen, no observed playthrough. Every claim is static analysis plus manual prose.
- **Eight of ten campaigns of the CD are unreachable** behind the second obfuscation, so the 1992
  campaign cannot be compared with its successors.
