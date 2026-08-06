# Comanche: Maximum Overkill (1992) — Campaign One: "Operation Maximum Overkill"

> **Sources:** the shipped game data of the **1992 three-floppy release** (campaign file `1.MIS`,
> de-obfuscated in this run — method in [`sources.md`](sources.md) §2) and the **NovaLogic
> `USER'S MANUAL`** (`MANUAL.PDF`, 139 pages, on the Comanche CD).
> Form per [`doc/mods.md`](../../../doc/mods.md) §3. A mod has **no `test/`**: the missions are the test.

Provenance tag on every fact:

| Tag | Means |
|---|---|
| `[MIS]` | **measured here** from the decrypted `1.MIS` of the 1992 floppy release. Strongest source: it is what the game actually loads |
| `[MIS0]` | measured here from `0.MIS` (the *training* campaign). Used only as cross-evidence for object-type identification |
| `[MAN p.N]` | NovaLogic Comanche `USER'S MANUAL`, page N. Printed page numbers coincide with PDF page indices (checked at p. 30) |
| `[DERIV]` | computed here; the formula is given at the point of use |
| `[DTA]` | measured here from the game's art/terrain files |

**Nothing below comes from a wiki, a review or MobyGames.** No public source found in this run names
a single Comanche mission — see [`sources.md`](sources.md) §5.

## Spec

### 1. Frame

| Item | Value | Provenance |
|---|---|---|
| Game | **Comanche: Maximum Overkill**, NovaLogic, 1992, MS-DOS | `[MAN p.5]` (`© 1992 NovaLogic, Inc.`) |
| Engine | **Voxel Space** (pat. pending), 386 native mode, 320×200×256 VGA | CD Installation Booklet (§4 of [`sources.md`](sources.md)) |
| Campaigns in the **1992 floppy release** | **exactly two** — `0.MIS` *Comanche Training*, `1.MIS` *Operation Maximum Overkill* | `[MIS]` file inventory of the floppy set |
| Missions per campaign | **10** | `[MIS]` |
| Campaign under study | **`1.MIS` — `{Operation Maximum Overkill}`** (the first *combat* campaign; menu entry 2, after the training set) | `[MIS]` verbatim first record of the file |
| Own aircraft | Boeing-Sikorsky **RAH-66 Comanche**, 2-seat, fly-by-wire, "management by exception" | `[MAN p.15, 109–111]` |
| Wingman | a second RAH-66, present in **6 of 10** missions | `[MIS]` §5 |
| Enemy | **"the KGB" / "renegade KGB agents" / "the new KGB"**, flying Ka-50 Werewolves and fielding T-80 and SA-8 | `[MIS]` briefing text |
| Enemy hardware named in briefings | Werewolf (Ka-50 Hokum), T-80, Gecko (SA-8) | `[MIS]` |
| Named place in this campaign | **none.** No mission of `1.MIS` names a country, city or river | `[MIS]` full-text check |

**The 1992 release contains 20 missions, not 100.** The "100 missions in 10 campaigns" of the box copy
is the **1994 CD compilation** (base + two mission disks + one CD-exclusive campaign). Measured: the
floppy set holds `0.MIS` and `1.MIS` only; the CD holds `0.MIS`–`9.MIS`.
See [`sources.md`](sources.md) §3 for the campaign-to-release attribution.

### 2. The ten missions

Names and briefings are verbatim `[MIS]`. Map, time of day, loadout and goals are measured `[MIS]`.

| # | Name | Map | Night | Goals | Opposition present | Own loadout `Hf/St/Ar/Wg` | Fuel |
|---|---|---|---|---|---|---|---|
| 1 | **Werewolves on Patrol** | 2 | yes | **26 of 27** | 14 Werewolf · 4 T-80 · 8 Gecko | 4 / 18 / 0 / 14 | 10 000 |
| 2 | **The Last Sacrifice** | 1 | no | **17 of 26** | 8 Werewolf · 12 T-80 · 5 fuel tank | 6 / 10 / 5 / 6 | 20 000 |
| 3 | **Tactical Run** | 2 | no | **5 of 17** | 4 Werewolf · 6 T-80 · 2 Gecko · 5 fuel tank | 6 / 10 / 0 / 0 | **6 000** |
| 4 | **Rivers Run Deep** | 2 | yes | **14 of 20** | 7 Werewolf · 7 T-80 · 5 Gecko | 8 / 8 / 6 / 8 | 8 000 |
| 5 | **Night of Death** | 3 | yes | **3 of 29** | 9 Werewolf · 7 T-80 · 11 Gecko · 1 fuel tank | 8 / 8 / 2 / 12 | 12 000 |
| 6 | **Thirsty Werewolves** | 4 | no | **4 of 29** | 14 Werewolf · 11 T-80 · 4 fuel tank | 8 / 8 / 4 / 0 | 10 800 |
| 7 | **Spiritual Reclamation** | 1 | yes | **11 of 17** | 5 Werewolf · 9 T-80 · 3 Gecko | 6 / 10 / **8** / 0 | **6 000** |
| 8 | **Volcanic Nightmare** | 3 | no | **19 of 20** | 7 Werewolf · 12 T-80 | 8 / 8 / 8 / 10 | 8 000 |
| 9 | **Valley of Instant Death** | 4 | no | **16 of 22** | 6 Werewolf · 4 T-80 · 8 Gecko · 4 fuel tank | **14 / 2** / 5 / 0 | 10 000 |
| 10 | **Wolfpack** | 4 | yes | **12 of 29** | 8 Werewolf · 12 T-80 · 8 Gecko | 8 / 8 / 0 / 12 | 12 000 |

`Hf` Hellfire · `St` Stinger · `Ar` artillery calls · `Wg` wingman Hellfire hand-offs. Cannon is
**500 rounds** and rockets **62** in every one of the twenty missions of the 1992 release — those two
numbers never vary `[MIS]`, `[MIS0]`.

**Five of ten missions are night** `[MIS]`. Night is a first-class engine mode, not decoration: the
whole cockpit and both TAC monitors switch to green-on-black image-intensifier rendering
`[MAN p.22]`, and the mission loads a separate `n`-prefixed object/palette file
([`terrain.md`](terrain.md) §2).

### 3. Briefings, verbatim `[MIS]`

Markup in the source text: `|r` red, `|g` green (the default), `|o` orange, `|y` yellow, `^` line break.

```
{Werewolves on Patrol}
If you're going to see the world, it might as well be from behind enemy lines. Fortunately it's night
and the enemy hasn't spotted you yet. Pick'em off one by one, and let your wingman have some fun.

{The Last Sacrifice}
Guarding an ancient sacrificial temple, renegade KGB agents have one last sacrifice in mind, |rYOU!|g
Your objective is to eliminate all ground support in the quadrant. Your wingman has a full load of
Hellfires, while you have a rack of Stingers.^|oWARNING:|g Werewolves have been reported in the area.

{Tactical Run}
The KGB is back, and is entrenched in the desert with a large fuel reserve. Fight your way through
enemy lines, and destroy the fuel tanks. A strategic victory like this could put an end to the reign
the new KGB.

{Rivers Run Deep}*
Enemy T-80s and Geckos have infiltrated your area. You and your wingman are being sent to take them
out. Recent attempts have been thwarted by the squadron of Werewolves giving air support. We recommend
that you take them out first.

{Night of Death}*
The beauty of the night (with visiononics) can be overwhelming. Especially when the air is filled with
missiles. We have found several key missile sites that we would like to have removed. Hopefully you and
your wingman will be able to take them out. Should you not make it back, we'll have to divvy up your
possessions.

{Thirsty Werewolves}*
Your objective is to eliminate the enemy's fuel depots before their Werewolves can be refueled for
action. Getting them on the ground will extend your life dramatically. Unfortunately, they are heavily
guarded by missile launching Geckos and roaming Werewolves.^|rGood Luck, you'll need it!|g

{Spiritual Reclamation}*
The KGB has returned to reclaim their temple. In a sneak attack at night, they have sent a powerful
arsenal of Geckos, T-80s and Werewolves all armed with guided missiles aimed at |rYOU!|g Not completely
prepared for the attack, you fly into battle alone with a minimal complement of weapons. Don't worry
too much, a few good men in the artillery ranks are behind you 100%.

{Volcanic Nightmare}*
The perfect secret base. The rising heat and cloud cover has hidden this encampment from our satellites
for years. If not for a minor improvement in IR detectors, we may never have known about it. Your
mission is to let the enemy know that we know where they are by killing them all!

{Valley of Instant Death}*
Thanks for volunteering, no one else would dare go on this mission. The enemy has entrenched themselves
in a small valley, and are very prepared for an attack. By the way, we accentally told all the news
stations where you would be and sold the enemy a few truck loads of guided missles.

{Wolfpack}*1
Every Werewolf within 50 klicks is on your tail. You have one Stinger for each Werewolf, so^|rDon't
Miss!|g If you survive the onslaught of the Wolves, the rest of the mission is a piece of cake.
```

`accentally`, `missles` and `the reign the new KGB` are the game's own typos, reproduced.

### 4. Unlock structure `[MIS]`

The `*` suffix on a mission name is the lock marker. Missions 1–3 carry none; 4–10 carry `*`;
mission 10 carries `*1`.

| Reading | Evidence |
|---|---|
| **1–3 selectable from the start, 4–10 gated** | the marker is present exactly on 4–10 `[MIS]`; the manual says *"You will not be able to access some missions until you have completed earlier ones"* `[MAN p.24]` |
| the trailing digit of `*1` / `*0` is a **second** parameter | `0.MIS` uses `*0` on its last mission, `1.MIS` uses `*1` on its last `[MIS]`, `[MIS0]`. **What the digit selects is not determined** |

The manual adds one hard consequence for the reading rules below:
**"If your helicopter is destroyed during any campaign, you will have to re-start the campaign from the
first available mission of that campaign."** `[MAN p.59]`

### 5. Object records — what a mission places `[MIS]`

Every mission is a plain-text block. After the briefing come an asset list, seven parameter lines, and
one line per world object:

```
[*] type , a , b , c , X , Y , heading , d , e , f , delay
```

| Field | Meaning | How established |
|---|---|---|
| leading `*` | **this object is a mission goal** | the manual's Mission Status Display counts *"the number of assigned goals you need to destroy"* and marks goals with **flashing** map borders `[MAN p.40, 42]`; the starred sets match every briefing's stated objective in all 20 missions `[MIS]`, `[MIS0]` |
| `type` | unit type, 1–6 (table below) | cross-checked against briefings, §6 |
| `X`, `Y` | map cell, 0–1023; **+X east, +Y south** | `[DERIV]` §7 |
| `heading` | 0–255 over the full circle; **0 = north, 64 = east, 128 = south, 192 = west** | `[DERIV]` §7 |
| `delay` | spawn delay; 0 = present at mission start | `[DERIV]` §7 |
| `a b c d e f` | **not decoded.** `(a,b,c)` is constant per type; `d e f` vary | — |

Parameter lines, in file order:

| Line | Content | How established |
|---|---|---|
| 1 | night flag, `0` or `1` | `1` in exactly the missions whose object/palette file carries the `n` prefix, 20/20 `[MIS]`, `[MIS0]` |
| 2 | player start `X , Y , heading` | in all six wingman missions the wingman spawns 41–58 cells from this point and directly astern of this heading `[DERIV]` §7 |
| 3 | `24,24` — two equal numbers, **meaning unknown**. Constant in all of campaign one; `15,15` and `20,20` occur in two training missions | `[MIS]`, `[MIS0]` |
| 4 | **loadout: cannon, rockets, Hellfire, Stinger, artillery, wingman-Hellfire** | §5.1 |
| 5 | fuel | only quantity that the briefings call "low" where it is low (3 and 7 at 6 000, both *"minimal"* / no re-supply) `[MIS]` |
| 6 | `5,3,5,3,3,3,6,3,30` — nine values, **not decoded**. Differs only in the last value (35 in missions 4 and 7); two training missions use `4,2,4,3,2,4,4,2,25` | `[MIS]`, `[MIS0]` |
| 7 | `0,0,0,1,0,0,0,0,0,0,0` — eleven values, **identical in all 20 missions** | `[MIS]`, `[MIS0]` |

#### 5.1 The loadout line is closed — six fields, six weapon keys

The manual's Weapon Select Display has exactly six entries, in this order: **cannon (Z) · rockets (X) ·
Hellfire (C) · Stinger (V) · artillery (B) · wingman (N)** `[MAN p.47, 53]`. Line 4 has six fields in
the same order. Three independent checks, all from mission text against the numbers `[MIS]`:

| Check | Mission | Text | Field | Value |
|---|---|---|---|---|
| Stinger = field 4 | **Wolfpack** | *"You have one Stinger for each Werewolf"* | 4 | **8** — and the mission places exactly **8** Werewolves |
| Artillery = field 5 | **Spiritual Reclamation** | *"a few good men in the artillery ranks are behind you 100%"* | 5 | **8**; every mission whose text is silent on artillery has **0** |
| Wingman = field 6 | all ten | field 6 > 0 ⟺ a type-4 object exists | 6 | **10 of 10**, no exception |

The wingman correlation is the strongest single result in this file: *The Last Sacrifice* says
*"Your wingman has a full load of Hellfires"* and carries `6`; *Spiritual Reclamation* says *"you fly
into battle alone"* and carries `0` **and no type-4 object**.

### 6. Unit types `[MIS]`, `[MIS0]`

| type | Unit | How established |
|---|---|---|
| **1** | **Ka-50 "Hokum" / Werewolf** | training *Flying Werewolves*: *"destroy all the Hokums (Werewolves)"* — goals are 6× type 1 + 2× type 6 `[MIS0]` |
| **2** | **T-80 main battle tank** | training *DUI Enforcement*: *"take out two entire tank platoons"* — 8× type 2, nothing else `[MIS0]` |
| **3** | **fuel / oil tank** | training *Oil Tank Holiday*: *"destroy all the oil tanks"* — 20× type 3, nothing else `[MIS0]`; campaign *Tactical Run* *"destroy the fuel tanks"* — the 5 goals are type 3 `[MIS]` |
| **4** | **friendly RAH-66 wingman** | one per mission at most, always ~50 cells astern of the player start, present exactly when the text mentions a wingman `[MIS]` |
| **5** | **SA-8 Gecko mobile SAM** | *Rivers Run Deep* *"Enemy T-80s and Geckos"* → only types 2 and 5 present; *Night of Death* *"several key missile sites"* → the 3 goals are type 5 `[MIS]` |
| **6** | **Ka-50 "Hokum" / Werewolf**, second variant | same evidence as type 1; **what distinguishes 6 from 1 is not determined** — see `## Gaps` |

Sprite files loaded by every mission of this campaign, in file order `[MIS]`:
`lh66` (Comanche) · `spin` (rotor disc) · `tank` · `turr` (turret) · `radr` (radar) · `fuel` · `hoke`
(Hokum). **Six world objects, six object types** — but which of `turr` / `radr` draws the Gecko is not
established.

**That is the whole hostile inventory of the 1992 release: one helicopter, one tank, one SAM, one
static target.** No Hind, no Scud, no OSA II patrol boat, no BRDM, no Hughes 500MD, no M1A1 — those
appear in the manual `[MAN p.115–124]` because the manual ships with the **CD**, and their sprites
(`hind` `scud` `osa2` `lebe` `jeep` `trck` `m1ab`) exist only in the CD file set, not in the 1992 one.
See [`sources.md`](sources.md) §3.

### 7. Orientation, delays and the coordinate frame `[DERIV]`

Neither the manual nor the data states which way the map faces. Derived from the wingman spawns:
a wingman spawned **astern** implies heading 0 → −Y, 64 → +X, 128 → +Y, 192 → −X.

| Mission | Player | Heading | Wingman | Offset | Astern under 0=−Y? |
|---|---|---|---|---|---|
| Werewolves on Patrol | 493, 291 | 128 | 493, 250 | 0, −41 | ✔ |
| The Last Sacrifice | 300, 800 | 0 | 275, 850 | −25, +50 | ✔ |
| Rivers Run Deep | 430, 700 | 128 | 430, 650 | 0, −50 | ✔ |
| Night of Death | 450, 650 | 0 | 430, 670 | −20, +20 | ✔ |
| Volcanic Nightmare | 600, 800 | 0 | 521, 742 | −79, −58 | **✘ — 58 cells ahead** |
| Wolfpack | 825, 350 | 64 | 775, 350 | −50, 0 | ✔ |

**Five of six.** The frame adopted throughout this mod is therefore: **image column = X = east, image
row = Y = south, heading clockwise from north in 256 steps.** *Volcanic Nightmare* is left as measured,
not smoothed — it puts the wingman ahead and to the left.

`Tactical Run` is the one mission whose start line is `0,0,0`, i.e. the map corner, 830 cells from its
nearest goal. Recorded as measured; whether the engine reads it as "no start given" is unknown.

**Delays** carry two distinct magnitudes and both are consistent with **ticks**, not seconds:

| Mission | Delays | Reading |
|---|---|---|
| Werewolves on Patrol | 60, 70, 80 … 190 | a 14-ship formation staggered over ~2 s |
| Thirsty Werewolves | 3 600 · 5 400 · 6 300 · 7 200 | four reinforcement waves, one per fuel depot, in **increasing distance from the player start** (486 → 700 → 774 cells for the first three) |
| Night of Death | 2 000 · 3 000 · 4 000 · 4 500 · 4 600 | two clusters, likewise farther = later |

The *Thirsty Werewolves* correlation (delay rises with range) is what makes the tick reading usable as
a **scale anchor** — the derivation is in [`terrain.md`](terrain.md) §4.

### 8. Reading rule per mission

The `.fbm` header in this tree states how its exit code is read (`CLAUDE.md`). Each rule below is now
the binding header of the file of the same number in `../src/missions/`; what the run then MEASURED
against it is in [`substitutions.md`](substitutions.md) §6. The **goal set is data, not opinion**: it is
the starred object list of §5, and the engine's own Mission Status Display counts down exactly that set
`[MAN p.42]`.

| # | Reading rule |
|---|---|
| **1** | SUCCESS only when all 26 starred objects are destroyed and the Comanche is intact; the single unstarred object is the wingman, so a run that wins with a dead wingman is still SUCCESS and must be distinguishable in telemetry from one that does not. |
| **2** | SUCCESS only when all 17 ground goals die; the 8 Werewolves are **not** goals, so a run that kills every Werewolf and no tank is a fail — if the harness reports it as progress, the harness is wrong. |
| **3** | SUCCESS only when all 5 fuel tanks are destroyed **on 6 000 fuel**; running dry short of the target is a mission loss, but reaching the target with fuel to spare at a lower throttle setting than the model allows is an engine defect. |
| **4** | SUCCESS only when the 14 goals die; the briefing orders the Werewolves killed **first**, so telemetry must carry the kill order or the tactical claim of this mission cannot be checked at all. |
| **5** | SUCCESS only when the 3 starred Geckos die — 26 of 29 objects are irrelevant to the verdict and must not enter it; a run that clears the map and misses one missile site is a fail. |
| **6** | SUCCESS only when the 4 fuel depots die; the reinforcement waves at ticks 3 600–7 200 mean a slow run meets an enemy a fast run never sees, so the same rule produces two different fights and both must be reproducible. |
| **7** | SUCCESS only when the 11 goals die flying **alone, on 6 000 fuel, with 8 artillery calls**; this is the campaign's only mission where artillery is load-bearing, so a run that never calls artillery and still wins means the artillery model is untested, not that it works. |
| **8** | SUCCESS only when all 19 starred objects die; the mission is a pure clear-the-board and is therefore the cheapest regression probe in the campaign — if anything in weapons or damage drifts, this one moves first. |
| **9** | SUCCESS only when the 16 ground goals die with **14 Hellfires for 16 targets**; the arithmetic is deliberately short, so a win is only a win if the shortfall was covered by cannon or rockets, and telemetry must say which. |
| **10** | SUCCESS only when the 12 ground goals die; the 8 Werewolves are not goals but the loadout carries exactly 8 Stingers, so the run that spends Stingers on anything else and still wins has found a hole in the Stinger model. |

### 9. Own platform `[MAN p.109–111]`

| Quantity | Value |
|---|---|
| Engines | 2 × Allison-Garrett **T800-LHT-801**, 925 shp each; transmission max 2 054 shp |
| Main rotor | bearingless, **5 blades**, composite, Ø 11.90 m (39.04 ft) |
| Anti-torque | **FANTAIL**, 8 blades, Ø 1.37 m (4.50 ft), blade length 11.43 cm, chord 17.0 cm |
| Dash speed | **above 328 km/h** — the HID section gives **177 kt ≈ 200 mph** as the simulated top speed `[MAN p.37]` |
| Vertical rate of climb | 360 m/min (1 182 fpm) |
| Load factor | **+3 g** |
| Length fuselage / rotors turning | 13.22 m / 14.28 m |
| Weights | self-deploy 7 790 kg · primary-mission gross 4 587 kg |
| Internal fuel | 984 l (260 gal) |
| Gun | turreted **20 mm Gatling**, **500 rounds**, **1 500 rounds/min** — *"ammunition will last less than 1 minute"* `[MAN p.48]` |
| Rockets | **70 mm**, fired in spreads of 1 or 2, flechette warhead, lethal to 2 km, unguided `[MAN p.49]` |
| Hellfire | **AGM-114**, laser-guided, standoff **> 8 km**, lock must be held to impact `[MAN p.49]` |
| Stinger | **AIM-92**, IR, fire-and-forget, **1–2 km** `[MAN p.50]` |
| Artillery | 155 mm and MLRS, called by TAS coordinates over the C2 net, quantity per mission `[MAN p.50]` |
| Sensors | focal-plane-array FLIR · low-light-level II TV · helmet-mounted sight · laser range-finder/designator · Aided Target Detection System `[MAN p.110]` |
| Simulated ceiling | **≈ 500 ft above sea level** — a design decision, stated as such `[MAN p.2, 36]` |

**Simulated ceiling is a design statement, not a limitation to be repaired.** *"On today's battlefield,
if you are flying above 150 ft., you are flying too high"* `[MAN p.2]`.

### 10. Hostile specifications `[MAN]`

Only the three that occur in this campaign. The rest of the manual's enemy chapter belongs to the CD.

| Unit | Page | Values |
|---|---|---|
| **Ka-50 Kamov "Hokum" / Werewolf** | 112–114 | 2 × Klimov TV3-117K, 2 220 shp, transmission 5 500 shp · coaxial contra-rotating, 2 × 3 blades, Ø 14.5 m, **no tail rotor** · dash **above 350 km/h (189 kt)** · climb 600 m/min · **+3 g** · turreted **30 mm**, 500 rounds, 1 000 rds/min, selectable AP/HE · Vikhr or Spiral AGM, **SA-19 AAM** · FLIR, helmet sight, laser designator · reduced RCS, IR suppression |
| **T-80 main battle tank** | 119 | crew 3 · 42 t · 985 hp gas turbine · **75 km/h** · 125 mm smooth bore, 42 rounds · 7.62 mm and 12.7 mm MG · **AT "Songster" anti-helicopter missile** — *the enemy tank shoots back at aircraft* |
| **SA-8 Gecko** | 121 | pulse-Doppler radar guidance, own radar array · 88 lb contact/proximity warhead · range **1 to 9.4 miles** · altitude **32 to 42 000 ft** · **Mach 2** · two-missile salvo on different frequencies against ECM |

The Werewolf out-dashes the Comanche (189 kt against 177 kt) and out-guns it (30 mm against 20 mm).
That is the campaign's central asymmetry and it is in the manual, not inferred.

## State

**All ten missions are built and run**: `mod.json` (`depends f16`, no catalogue and no DEM of its own)
plus `src/missions/c01m01…c01m10-*.fbm`, one file per record of `1.MIS`, every object of §5 placed at
its measured cell through §7's frame. `./build/fb-gym --mod ../mods/comanche --mission <name>`; the
verdicts, the goal counts and what each of them measures are in
[`substitutions.md`](substitutions.md) §6, the reading rule of §8 is the header of each file.

**There is still no rotorcraft.** The player and the wingman fly `module f16`, the Werewolf is an
`ah64` mover that fires nothing, and the four axes of that substitution are measured in
[`substitutions.md`](substitutions.md) §2.1 — which is what this round set out to produce.

**19 of 127 goals fall.** Not because of the airframe: `set task attack` is ONE briefed pass at ONE
active waypoint and hands back to `Route`, so the game's own cast (player + wingman when field 6 is
non-zero) can strike at most two aim points against goal sets of 3 to 26.

## Gaps

**What the engine cannot declare today** — this is the deliverable of the round, per
[`doc/mods.md`](../../../doc/mods.md) §2. Every row that a run in this round could put a number on now
carries one; the rest stayed exactly as written before the missions existed.

| What Comanche needs | Why it is not declarable |
|---|---|
| **A rotorcraft at all** | the body format ([`doc/body-format.md`](../../../doc/body-format.md)) spans it in principle; nothing implements it. **No rotor, no collective, no cyclic, no tail-rotor authority, no ground effect** — and ground effect is not cosmetic here, the manual builds NoE flight on it `[MAN p.18]` |
| A flight envelope of **0–500 ft and 0–177 kt** | **measured, and it is the airframe that refuses.** The pass flown at the original's own speeds — 180 / 200 / 220 kt — CRASHES on `FBPilot`'s attack egress (120° turn + 500 m climb bleeds the F-16 to 88 kt CAS at 543 m); 250 and 275 kt crash on mission 2's geometry; 300 kt survives. The height half is the opposite: at 150 m the CCRP delivery misses by 8.01 m against 26.60 m from 900 m |
| **Hover as a commanded state** (`*` key = auto-hover at NoE height) | no hover, no auto-hover, no altitude-hold-over-terrain law |
| **Yaw decoupled from turn** (fantail rotation in place, `Ins`/`Del`) | fixed-wing control mapping has no equivalent |
| **Terrain seen and used at 10–50 m** — terrain masking is *"the essence of modern helicopter warfare"* `[MAN p.2]` | terrain, buildings and foliage at that scale — and, newly named: **a mod carries exactly one `"dem"`** and this campaign is four disjoint theatres, so its ground could not be baked even if the heightfields loaded ([`terrain.md`](terrain.md) §State) |
| A **wingman as a weapon system** — `N` designates a target and *his* Hellfire flies `[MAN p.51]` | no unit may command another unit's release. The wingman here flies its own briefed pass instead, which is a different tactic with the same airframe |
| **Off-map fire support** (155 mm / MLRS called by TAS coordinates, N per mission) | no artillery, no C2 net, no delayed-arrival ordnance. Mission 7 is built on 8 calls and flies without them |
| **A helicopter enemy with a 30 mm gun and AAMs** | **measured, and it is not merely unwritten.** A hand-written `ka50` catalogue row (mover, T2, `gsh301`, 500 rounds, 4 stations) behaves BYTE-IDENTICALLY to the unarmed `ah64`: T2's only combat phase is `Bfm`, `set task bfm` is refused at spawn on a row with no measured roll plant, and every mover has none. Across the ten missions: **82 Werewolves, 0 weapon events** |
| **A rocket pod** — 62 unguided 70 mm per mission, in all twenty missions of the release `[MIS]` | no rocket store row exists; `hydra70` / `s8` are specified in [`doc/air-to-ground.md`](../../../doc/air-to-ground.md) §3.5 and not built |
| **A second pass** — the campaign's goal sets run 3 to 26 objects for one aircraft and a wingman | `set task attack` is ONE briefed pass at the ACTIVE waypoint; it egresses and hands back to `Route`, and nothing in the format or in `FBPilot` re-enters it. **This, not the airframe, is why 19 of 127 goals fall** |
| **A run-in the striker flies itself onto** | measured: the attack phase anchors its leg at the phase's first tick and never corrects a lateral offset — spawned on the game's own start heading the strikers released 249.33 m (mission 1) and 8 382.30 m (mission 3) across track |
| **A schedule** — mission 1's 14-ship stagger, mission 6's four waves at ticks 3 600–7 200 | `.fbm` is not a schedule; all 29 objects stand at t = 0, so mission 6's *"a slow run meets an enemy a fast run never sees"* has no object |
| **A tank that shoots at helicopters** (T-80 "Songster") | ground units are declared in the body format, absent in code |
| **Six discrete damage areas** (tail rotor / engine / TAS / weapon mount / cannon / TAC display) with named consequences `[MAN p.57–58]` | `FBSystemHealth` is monotone and typed, but the *areas* and their flight-behaviour effects are aircraft ones |
| **Automatic** chaff and flare on detection, with manual override | `set cmds_mode auto` covers the automatic half on the substitute airframe; the game states no magazine, so the count is the F-16's |
| Briefing prose per mission | `.fbm` carries a reading rule for a machine, not prose for a player |

**What is measured but not understood** — every one of these is a hole in the reconstruction, not in the
game:

- **Object-record fields `a b c d e f` are not decoded.** They are constant per type for `(a,b,c)` and
  vary within a type for `d e f`; unit identity is recoverable without them but behaviour is not.
- **Type 1 versus type 6 is unresolved.** Both are Hokums by four independent briefings. They alternate
  regularly inside formations (`6,1,6,1,…` in *Wolfpack*; every fourth ship in *Werewolves on Patrol*),
  which suggests leader/wingman or two AI profiles — **suggests, not shows**.
- **Parameter line 6** (`5,3,5,3,3,3,6,3,30`) is not decoded. It varies exactly where difficulty
  plausibly varies, which is a guess and is recorded as one.
- **Parameter line 3** (`24,24`) is not decoded.
- **The delay unit is inferred, not read.** The tick reading rests on the range-versus-delay
  correlation in §7 and on 60 Hz or 70 Hz being the loop rate; neither is confirmed by any source.
- **Enemy behaviour is entirely undocumented.** Nothing read here says how a Werewolf patrol is
  triggered, whether Geckos have a detection radius, or what "roaming" means. Every reading rule in §8
  that mentions a fast run against a slow run depends on this.
- **The `*1` / `*0` digit on the last mission of each campaign is unexplained.**
- **The CD's `1.MIS` uses a different, undecoded obfuscation.** Whether the CD altered this campaign is
  therefore unknown; the file is 24 bytes longer than the floppy's.
- **No independent confirmation of the mission order exists.** The order 1–10 is the record order in
  `1.MIS`. That is strong — it is what the selection screen enumerates — but it is a file order, not an
  observed playthrough, and no Let's Play with a visible mission list was found ([`sources.md`](sources.md) §5).
