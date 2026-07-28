# W4 — Allied Force 1999 (mountains, cloud, an air defence that will not die)

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of Operation Allied Force, 24 March – 10 June 1999 | §Knowledge 1, every fact cited and tiered |
| **FlightBox sources** | what a `.fbm` can declare and what the modules can do | [`../missions/weather.md`](../missions/weather.md), [`../missions/weapons.md`](../missions/weapons.md), [`../missions/sensors.md`](../missions/sensors.md), [`../world/weather.md`](../world/weather.md), [`../world/terrain.md`](../world/terrain.md), [`../render/clouds.md`](../render/clouds.md), [`../modules/f16/module.md`](../modules/f16/module.md) |

Confidence legend and gap IDs `C0…C21`: [`INDEX.md`](INDEX.md).

**Temporal honesty: none needed.** The F-16C flew this campaign in both roles that matter here — the
Block 40 strike jet and the **Block 50 F-16CJ** SEAD jet with the AGM-88 [T4]. It is the only
campaign in the western half whose *weather* is as well documented as its order of battle, which is
why it is the campaign that consumes FlightBox's weather hook.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| Dates | **24 March – 10 June 1999**, planned for 72 hours, ran **78 days** | [T1]/[T4] |
| SEAD force | **50 × F-16CJ Block 50** carrying **AGM-88 HARM** against emitting Yugoslav radars | [T4] |
| Altitude floor | **15,000 ft or higher**, forced by heavy MANPADS and AAA | [T3] |
| Weather | "the stormy spring weather just kicked our butts for the first 45 days" — Lt Gen Michael C. Short, CFACC; many crews returned with their bombs | [T3] |
| Yugoslav SAM inventory | **SA-6 (2K12 Kub)**, **SA-2 (S-75)**, **SA-3 (S-125)**, plus MANPADS and AAA; largely Cold-War vintage, limited mobility and ECM | [T4] |
| Yugoslav SAM tactics | **dispersed, radars mostly not emitting**; concealment under weather and mountainous terrain; **extensive use of decoys** | [T3] |
| NATO losses (aircraft) | **1 × F-117** (27 March), **1 × F-16C Block 40** (2 May) | [T4] |
| Yugoslav MiG-29s | the 127th Fighter Aviation Squadron "Knights" at **Batajnica**; five scrambled the night of 24/25 March, several shot down by F-15Cs on 24/25 and 26 March; the surviving aircraft had **outdated avionics, degraded radar performance and a limited missile stock** | [T4] |
| Anchor region | Aviano AB ≈ 46.03 N 12.60 E; Batajnica ≈ 44.93 N 20.26 E; Kosovo engagement zone ≈ 42.2–43.2 N 20.2–21.8 E (**approximate, verify against DEM**) | [T4] |

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| The subject is **finding things**, not killing them | an air defence that does not emit and a target under cloud are the same problem twice. Every mission reports what was *detected*, not only what was hit |
| **The weather is a participant** | every mission carries a `wx` line, and at least four use `wx fixture` with a real GFS blob ([`../missions/weather.md`](../missions/weather.md)). A campaign flown in `wx calm` is not this campaign |
| The altitude floor is mission data | the 15,000 ft floor is declared in every striker's `wp` altitudes and is the thing the missions push against |
| Terrain is cover, not scenery | flown with `--elev tiles` over the real Balkans; the moment `C4` closes, these missions are its acceptance test |
| **Ground targets in every mission** | the campaign is a ground campaign; the fighters are an interruption |
| The verdict is machine-read | `kill unit`/`kill team` on the ground set, `survive` per striker |

### 3. The ten missions

| # | Mission | Task | Time | Wx | Blue | Red | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `w4-01-floor` | strike from above the floor | day | calm | 1 F-16, 2 × Mk-82 | — | 1 `target_soft` | `kill unit` | What does a release from ≥15,000 ft do to the delivery error the tree measured at low level (22.2 m on `attack-ccrp.fbm`)? The number that decides whether the whole campaign is flyable with unguided stores (`C8`) |
| 2 | `w4-02-overcast` | the same strike under a real cloud deck | day | `wx fixture` | 1 F-16 | — | 1 `target_soft` | `kill unit` | Does the deck change anything measurable? **Expected answer today: no** (`C16` — cloud affects only the IRST). The mission exists so that the answer is on record before the cloud rebuild lands |
| 3 | `w4-03-valley` | strike in a mountain valley | day | `wx wind` | 2 F-16 (flight) | — | 1 `target_hard` in terrain | both `kill unit` | Can the path law hold an attack heading down a valley, and how much of the miss is the wind (measured: +12.8 m at 25 kt) versus the terrain-relative geometry? |
| 4 | `w4-04-emitter-hunt` | hunt a radar that emits and stops | day | calm | 2 F-16 | 1 emitting ground radar (**`C1`: does not exist**) | 1 `target_soft` (the radar) | `kill unit radar` | **The campaign's central question.** An emitter that goes quiet is a memory problem, not a sensor problem — exactly the shape of `FBBfmTrack::Datum`. Unbuildable today; specified so the shape is on record |
| 5 | `w4-05-decoy` | the same hunt with decoys present | day | calm | 2 F-16 | 1 real + 3 decoy emitters (**`C1`**) | 4 `target_soft`, one of them the real one | `kill unit` on the real one, penalty for the others | Can a pilot that only has bearings distinguish a decoy from a battery? A test of the RWR's own honesty: **it measures received power, never range** ([`../sensors.md`](../sensors.md)) |
| 6 | `w4-06-cap-intercept` | intercept a scrambling defender | day | `wx fixture` | 2 F-16 (flight) | 2 MiG-29 from Batajnica, degraded (`set n019_mode off` on one) | 1 `target_hard` (the airfield) | `kill team hostile` + both `survive` | The 1999 Fulcrum flew with broken systems. What does an opponent with **no radar at all** still do to a flight — and does the F-16 pilot notice that it never gets illuminated? |
| 7 | `w4-07-package` | strike package with a suppression element | day | `wx fixture` | 4 F-16 (2 strike, 2 "SEAD") | 2 MiG-29 | 2 SAM sites (**`C1`**) + 1 `target_hard` (bridge) | strike `kill unit bridge` + all `survive` | Is a suppression element that cannot suppress worth the airframes? The mission is the honest form of "we have no HARM" |
| 8 | `w4-08-weather-abort` | the mission that must be brought home | day | `wx fixture` (heavy) | 2 F-16 with stores | — | 1 `target_soft` under the worst cell | either `kill unit` **or** `waypoints` home with stores | **The 45-day finding as a decision:** can the pilot decide not to release? Today it cannot — the attack phase pickles on the cue and nothing tells it the target is obscured (`C16`) |
| 9 | `w4-09-kez` | armour in the engagement zone | day | `wx wind` | 4 F-16 (two flights) | 2 MiG-29 | 6 `target_soft` in a dispersed column (**static — `C14`**) | ≥4 of 6 killed + all `survive` | Against dispersed, small, moving targets from 15,000 ft, how many passes does a four-ship need — and how much of the answer is the store rather than the pilot? |
| 10 | `w4-10-allied-force` | the full night package | **night** | `wx fixture` | 8 F-16 (four flights: 4 strike, 2 escort, 2 SEAD) | 4 MiG-29 + SAM/AAA (**`C1`**) | 1 `target_hard` (bridge) + 4 `target_soft` (IADS nodes) + 2 decoys | ≥3 of 4 IADS nodes + the bridge, ≥7 of 8 recover | With every one of the campaign's four antagonists present at once — cloud, mountains, an emitter that hides, and a fighter that only sometimes works — **does the package still find its targets?** |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| F-16C strike | flyable module | **yes** | Block 40 stand-in |
| F-16CJ SEAD | flyable module + HARM | **module yes, weapon no** (`C8`) | the campaign's defining aircraft cannot carry its defining weapon |
| MiG-29 | flyable module | **yes** | the 127th's aircraft, with the documented degradation expressible today as `set n019_mode off` / `set n019_emission off` |
| SA-6 (2K12) battery | ground, emitting + shooting + **mobile** | **no** (`C1`, `C14`) | the war's most effective Yugoslav system |
| SA-2 / SA-3 site | ground, emitting + shooting | **no** (`C1`) | one of them killed the F-117 |
| MANPADS / AAA | ground, shooting, short range | **no** (`C1`) | **the reason the 15,000 ft floor exists** — without it the floor is an arbitrary altitude |
| Radar decoy | ground, emitting, not lethal | **no** (`C1`) | the anchor names decoys as a decisive Yugoslav measure |
| Bridge | ground, hard | **yes** (`target_hard`) | |
| Dispersed armour | ground, moving | **static only** (`C14`) | |
| E-3 / EA-6B / tanker | air, support | **no** (`C5`, `C6`, `C13`) | |

### 5. What must be true before mission 1 can fly

`w4-01`, `w4-02`, `w4-03`, `w4-06` are buildable **today**. `w4-01` and `w4-02` should be built
first and as a **pair**: they differ by one `wx` line, and together they put a number on how much of
"weather" FlightBox currently simulates for a strike (the honest expected answer: the wind, and
nothing else).

---

## State

**Nothing built.**

What exists and is directly consumed: the `wx` line and its three providers, the GFS fixture
(`sim/assets/wx-2026-07-27T00Z.wxb`, verified against an independent ecCodes decoder), the measured
wind effects on the release (`+12.79 m` at 25 kt crosswind, target still destroyed; `+45.16 m` at
100 kt, target **intact**), the `orbited` waypoint rule that exists *because* of wind, the tile
elevation provider over real terrain, and the MiG-29's switchable sensors.

What exists but is inert for this campaign: the cloud data in the weather provider reaches
`FBWorld::SetWeather` and the IRST's masking test — nothing else consumes it.

---

## Gaps

| ID | What is missing | Blocks here |
|---|---|---|
| `C1` | **no active surface-to-air threat, no emitter, no decoy** | missions 4, 5, 7, 10 entirely; the 15,000 ft floor loses its cause in all ten |
| `C8` | **no HARM** (and no LGB, no Mk-84, no cluster) | the F-16CJ cannot be an F-16CJ |
| `C16` | **cloud affects only the IRST** — not radar, not weapon delivery, not a visual pickup; the cloud rebuild is unbuilt ([`../render/clouds.md`](../render/clouds.md)) | mission 2 and mission 8 measure an absence |
| `C4` | **no terrain masking** — the hook (`const FBWorld*`) reaches the sensor slots, the computation does not exist | "shielded by mountainous terrain" is the anchor's own phrase for the thing that is missing |
| `C14` | **no moving ground units** | the KEZ armour hunt is a hunt for parked vehicles |
| `C2` | **no time of day** | mission 10 is a night package |
| `C5` | **no tanker** | Aviano-to-Kosovo is a tanked profile |
| `C7` | **no other modules** | F-117, EA-6B, E-3, KC-135 absent |
| `C15` | **no package coordination** | four flights on one target set with no timing |
| `C0` | **no campaign layer** | 78 days of attrition against a defence that hides is *the* campaign-layer subject |
| `C13` | **no jamming** | |

### The honest headline

Allied Force is the campaign whose **subject FlightBox is closest to being able to model and furthest
from having modelled**. The weather hook is real, measured and already changes outcomes; the terrain
is real; the elevation provider is real. What is missing is the other half of every one of those
sentences: cloud that a sensor or a weapon notices, terrain that blocks a radar, and a ground threat
that gives the altitude floor a reason to exist. Two of those three (`C4`, `C16`) are already named
as next steps elsewhere in the tree — [`../roadmap.md`](../roadmap.md) R5 and the "terrain masking
next" line in R6.

---

## Knowledge

### 1. The anchor with its sources

- **Dates, duration, scale.** [Kosovo Air Campaign, March–June 1999 (NATO topic page)](https://www.nato.int/cps/en/natohq/topics_49602.htm)
  [T1]; [1999 — Operation Allied Force (Air Force Historical Support Division)](https://www.afhistory.af.mil/FAQs/Fact-Sheets/Article/458957/1999-operation-allied-force/)
  [T1]; [Kosovo — Operation Allied Force (Naval History and Heritage Command)](https://www.history.navy.mil/browse-by-topic/wars-conflicts-and-operations/bosnia-kosovo/allied-force.html)
  [T1]. The "planned 72 hours, ran 78 days" framing is carried by the Osprey campaign study
  [Operation Allied Force 1999 (Osprey, Laslie)](https://www.ospreypublishing.com/us/operation-allied-force-1999-9781472860323/)
  [T3].
- **The 15,000 ft floor, the weather quote, the SEAD force, the Serb concealment and decoy tactics.**
  [Operation Allied Force — how airpower won the war for Kosovo (Air & Space Forces Magazine)](https://www.airandspaceforces.com/article/operation-allied-force-how-airpower-won-the-war-for-kosovo/)
  [T3] and [Operation Allied Force: Lessons for the Future (RAND research brief)](https://www.rand.org/pubs/research_briefs/RB75.html)
  [T3]. The "50 F-16CJ Block 50 with AGM-88" figure and the SAM inventory are from
  [NATO bombing of Yugoslavia (Wikipedia)](https://en.wikipedia.org/wiki/NATO_bombing_of_Yugoslavia)
  [T4] and [The Serbian Air Force against the 1999 NATO Allied Force operation (Defence reDefined)](https://defenceredefined.com.cy/the-obscure-files-of-an-anniversary-or-david-vs-goliath-the-serbian-air-force-against-the-1999-nato-allied-force-operation/)
  [T4].
- **The F-16 loss of 2 May.** [Holloman commander recalls being shot down in Serbia (f-16.net)](https://www.f-16.net/f-16-news-article2167.html)
  [T4].
- **The MiG-29 side.** See [`o5-airfield-defence.md`](o5-airfield-defence.md) §Knowledge 1, which owns
  the Batajnica material in full; it is summarised here rather than duplicated.

### 2. Where the sources are thin, and it is stated

| Thing | Status |
|---|---|
| The percentage of sorties cancelled or aborted for weather | **not established.** The Short quotation is qualitative ("the first 45 days"); no percentage was found and none is invented. Mission 8's difficulty is therefore `[SET]` |
| SA-6 "shoot and scoot" cycle times, emitter discipline intervals | **not sourced.** The campaign describes the behaviour ("emit, engage, move, stay quiet") without numbers, and mission 4/5 must declare its own `[SET]` timings once `C1` exists |
| Which specific bridges / IADS nodes were struck when | **not sourced on this pass**; the ground sets above are archetypes, not a target list |

### 3. Why missions 1 and 2 are the pair to build first

They are one `wx` line apart, they need nothing that does not exist, and they produce a number the
whole cloud rebuild will be measured against: **the delivery error from above the floor, with and
without a real cloud deck between the aircraft and the target.** Today the two runs should differ
only by the wind that the fixture happens to carry — and if they differ by more, that is a finding
about the weather provider, not about the weather.
