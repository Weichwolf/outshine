# W3 — Desert Storm, the first nights (January 1991)

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of the opening of Operation Desert Storm, 17–19 January 1991 | §Knowledge 1, every fact cited and tiered |
| **FlightBox sources** | what a `.fbm` can declare and what the modules can do | [`../missions/weapons.md`](../missions/weapons.md), [`../missions/sensors.md`](../missions/sensors.md), [`../missions/verdict.md`](../missions/verdict.md), [`../formation.md`](../formation.md), [`../modules/f16/module.md`](../modules/f16/module.md), [`../sensors.md`](../sensors.md) |

Confidence legend and gap IDs `C0…C24`: [`INDEX.md`](INDEX.md).

**Temporal honesty:** the F-16 flew from the first hours [T4], so the *type* fits. What is stretched
is the **date**: the campaign's centre of gravity is **Package Q, 19 January 1991**, not the night of
17 January, because Package Q is the best-documented large F-16 package against an integrated air
defence and it failed in ways that are precisely the ways a simulator can measure. The first-night
material (F-117, Apaches on the early-warning radars, the Tomahawks) supplies the **setting**; the
package structure and its failure modes supply the **missions**. Both halves are labelled below.

---

## Spec

### 1. The anchor, in two tables

**The setting — the opening 14 hours, 17 January 1991**

| Fact | Value | Tier |
|---|---|---|
| Start | **02:38 local**, eight AH-64 Apaches destroy two early-warning radar stations at treetop height | [T4] |
| Purpose of that first strike | blind the Iraqi IADS ("Kari") to what followed | [T4] |
| First 14 hours | **>1,300 combat sorties**, **100 Tomahawk** cruise missiles | [T4] |
| F-117 | **36 aircraft against 34 high-value targets** in defended Baghdad | [T4] |
| Iraqi IADS | "Kari" — a centralised, French-built integrated air-defence system | [T4] |
| Iraqi fighter force | ≈ **40 MiG-29s**; **six shot down by F-15Cs** in the opening days, none scoring an air-to-air kill; the cause is given as poor training, **no AEW support**, and flying "essentially alone, without the tactical architecture the design assumed" | [T4] |

**The archetype mission — Package Q, 19 January 1991**

| Fact | Value | Tier |
|---|---|---|
| Package | **78 aircraft**: 56 F-16, 14 F-15C, 6 F-4G Wild Weasel, 2 EF-111 | [T4] |
| Target | **Al-Tuwaitha nuclear research centre**, plus downtown Baghdad military installations (Iraqi AF HQ, refineries) | [T4] |
| Defences | thousands of SAMs and AAA; **SA-3** confirmed on at least one F-16; **100 mm** AAA; ≈55 Iraqi fighters aloft in the area (25 MiG-23, 20 MiG-25, 10 MiG-29) | [T4] |
| Losses | **2 F-16 shot down**, both pilots taken prisoner | [T4] |
| What went wrong — tankers | bad weather on the tanker tracks and tankers arriving early at the release point; **four fighters aborted**, others nearly stalled | [T4] |
| What went wrong — SEAD | **the Wild Weasels left early on fuel**, leaving the strikers without suppression | [T4] |
| What went wrong — command | the mission commander received "approximately 80 percent of the calls from all aircraft in the strike, an impossible workload" | [T4] |
| Consequence | large F-16 packages against Baghdad were cancelled; doctrine moved to **smaller formations** | [T4] |

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| The subject is **the package**, not the jet | every mission reports how many strikers reached release and how many came home. A mission whose result changes when one jet flies better is measuring the wrong thing |
| **The three Package Q failure modes are the campaign's three questions** | fuel/timing, suppression that leaves early, and a saturated command channel. Each gets its own mission, and each is measured, not narrated |
| The IADS is the opponent | not the fighters. Once `C1` is closed, a mission without a ground threat does not belong here |
| **Ground targets in every mission** | this is a strike campaign; even the sweeps are flown to open a corridor to something |
| Difficulty rises to 20+ units | mission 10 is the scale test for the actor list and the thread pool as much as for the pilot |
| The verdict is machine-read | `kill unit`/`kill team` on the target set + `survive` per striker |

### 3. The ten missions

| # | Mission | Task | Time | Wx | Blue | Red | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `w3-01-ew-radar` | single-ship strike on an early-warning site | **night** | calm | 1 F-16, 2 × Mk-82 | — | 1 `target_soft` (EW radar van) + 1 `target_hard` (bunker) | `kill unit ewradar` | The opening move of the whole war as one aircraft: can a level laydown kill a soft point target reliably enough that the rest of the plan may assume it? |
| 2 | `w3-02-pair-strike` | two-ship strike, no opposition | **night** | calm | 2 F-16 (flight) | — | 3 `target_soft` array | both `kill unit` | Two aircraft, three aim points: does the second jet's release inherit the first's error, or are they independent? |
| 3 | `w3-03-sweep` | fighter sweep ahead of a strike | **night** | calm | 2 F-16 (flight) | 2 MiG-29 (GCI-briefed) | 1 `target_hard` (the strike's target, unattacked) | `kill team hostile` + both `survive` | Does a sweep that is 10 minutes ahead of the strikers actually clear the corridor, or does it drag the fight back onto them? |
| 4 | `w3-04-sead-escort` | strike with a suppression element | **night** | calm | 2 F-16 strike + 2 F-16 "SEAD" (no HARM — `C8`) | 2 MiG-29 | 1 SAM site (**inert, `C1`**) + 1 `target_hard` | strike `kill unit` + all `survive` | With no anti-radiation weapon and no emitting SAM, **what is left of SEAD?** The mission exists to make the size of that hole explicit |
| 5 | `w3-05-weasels-leave` | the same package, suppression departs at a declared time | **night** | calm | as above, SEAD pair with a short `wp` plan home | 2 MiG-29 | as above | strike `kill unit` + strikers `survive` | **Package Q failure mode 2.** Does the strike's loss rate change when its cover leaves? Measurable only once `C1` gives the ground threat teeth |
| 6 | `w3-06-tanker-timing` | a long ingress at the edge of fuel | **night** | `wx fixture` | 4 F-16 | 2 MiG-29 | 2 `target_soft` + 1 `target_hard` | ≥3 `survive` + `kill unit` | **Package Q failure mode 1.** Without a tanker (`C5`), how much of the profile is reachable — and does the pilot's BINGO rule turn a fuel state into a decision? |
| 7 | `w3-07-mig-cap` | strike against a defending CAP with GCI | **night** | calm | 4 F-16 (two flights) | 4 MiG-29 (one flight, `set brief_gci`) | 1 `target_hard` + 2 `target_soft` | `kill unit` on the hard target + ≥3 `survive` | Against an opponent flown the way the type was designed to be flown (ground-vectored, late emission), does the F-16's earlier detection still convert into an earlier shot? |
| 8 | `w3-08-mig-alone` | the same fight with the GCI removed | **night** | calm | 4 F-16 | 4 MiG-29, **no `brief_gci`** | as above | as above | **The Iraqi MiG-29 question, isolated.** One line deleted from mission 7. How much of the historical 6-for-0 is the architecture and how much is the aircraft? |
| 9 | `w3-09-saturation` | many units, one command picture | **night** | `wx fixture` | 8 F-16 (two flights) | 6 MiG-29 (two flights) | 4 `target_soft` + 2 `target_hard` | `kill team hostile` on the ground set + ≥6 `survive` | **Package Q failure mode 3.** With more shooters than contacts and more contacts than shooters at different moments, does the sort still produce one target each — and where does `flt_switch` explode? |
| 10 | `w3-10-package-q` | the full package | **night** | `wx fixture` | 16 F-16 (four flights: 8 strike, 4 escort, 4 "SEAD") | 8 MiG-29 (two flights) + SAM/AAA once `C1` closes | 1 `target_hard` (research centre) + 6 `target_soft` (site + defences) | ≥6 of 8 strikers release AND ≥14 of 16 recover | **Does a 24-unit mission run deterministically, and does the package still work at that size?** Both halves are the result — the scale test and the tactical one |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| F-16C | flyable module | **yes** | strike, escort and the SEAD stand-in |
| MiG-29 | flyable module | **yes** | Iraqi air defence fighter; the GCI-on/GCI-off pair (missions 7/8) is the campaign's cleanest experiment |
| MiG-23 / MiG-25 class | flyable module | **no** (`C7`) | ≈45 of the 55 Iraqi fighters aloft in the anchor were these two types; substituting MiG-29s makes Red **stronger** than history, and the mission headers must say so |
| F-15C escort | flyable module | **no** (`C7`) | substituted by F-16s |
| F-4G Wild Weasel | flyable module + HARM | **no** (`C7`, `C8`) | the SEAD element has no weapon; missions 4/5 measure the hole |
| EF-111 jammer | flyable module + ECM | **no** (`C7`, `C13`) | not substitutable — jamming does not exist in the tree |
| KC-135 tanker | flyable module + boom | **no** (`C5`) | Package Q's first failure mode is a tanker failure |
| E-3 AWACS | air, support | **no** (`C6`) | the package's picture |
| SA-2 / SA-3 site | ground, emitting + shooting | **no** (`C1`) | the confirmed killer of at least one F-16 in the anchor |
| SA-6 / SA-8 mobile SAM | ground, emitting + shooting | **no** (`C1`) | |
| AAA (100 mm, ZSU-23-4) | ground, shooting | **no** (`C1`) | the anchor names 100 mm explicitly |
| Sector operations centre / GCI radar | ground, emitting | **no** (`C1`, `C6`) | "Kari" has no representation at all |
| `target_hard` / `target_soft` | ground | **yes** | research centre, bunkers, radar vans, refinery |
| Cruise missile (TLAM class) | air, one-way | **no** (roadmap R7) | 100 of them opened the war |

### 5. What must be true before mission 1 can fly

`w3-01`, `w3-02`, `w3-03`, `w3-07`, `w3-08` are buildable **today** — and `w3-07`/`w3-08` are the
pair worth building first, because they are one deleted line apart and they answer the campaign's
most interesting question with the modules that already exist. Everything with a "SEAD" or "tanker" in
its name is blocked.

---

## State

**Nothing built.**

Reused when they are: the GCI entry chain (`set brief_gci`, three typed entries over the command bus
with latency — [`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md) §2.2, measured
at 8.0 s from call to radiating radar in `mig29-intercept.fbm`), the flight sort and cover rule
([`../formation.md`](../formation.md)), the air-to-ground release path
([`../missions/weapons.md`](../missions/weapons.md)), and the thread pool's measured scaling (4 units
1.49–1.77× on 2–4 threads — [`../missions/runtime.md`](../missions/runtime.md)), which mission 10
will push well past anything yet run.

---

## Gaps

| ID | What is missing | Blocks here |
|---|---|---|
| `C1` | **no active surface-to-air threat** — nothing emits, nothing launches, nothing shoots | **the campaign's opponent.** Missions 4, 5, 8, 10 are hollow without it; 1, 2, 6, 9 lose their reason for the profile |
| `C8` | **no HARM, no LGB, no Mk-84, no cluster** | there is no such thing as a SEAD element in the tree — missions 4 and 5 can only measure the absence |
| `C13` | **no ECM/jamming of any kind** | the EF-111s have no representation; the anchor's electronic half is missing entirely |
| `C5` | **no tanker, no boom, no external tank** | Package Q's first failure mode |
| `C6` | **no live AWACS/GCI unit** | the command-saturation question (mission 9) has no command channel to saturate; `set brief_gci` is static text |
| `C7` | **only two flyable modules** | MiG-23/25, F-15, F-4G, EF-111, KC-135, E-3 all absent |
| `C2` | **no time of day** | every mission in this campaign is a night mission and none of them can say so |
| `C15` | **no package coordination** — no time-on-target, no deconfliction, no lead tasking | the *definition* of a package |
| `C18` | **no radio between units** | the anchor's third failure mode is a radio channel collapsing under 80 % of the calls; FlightBox has no such channel to collapse |
| `C22` | **no connected air defence** ([`../air-defence-network.md`](../air-defence-network.md)) | **"Kari" is the campaign's named opponent and there is nothing network-shaped to represent it.** Mission 1 (the strike on an early-warning site — the opening move of the whole war) measures nothing today, because an EW radar cues nobody and killing it changes no other unit's behaviour. Mission 9's saturation question needs a defence that can be saturated |
| `C23` | **no declared, judged belt geometry** | missions 4, 5 and 10 fly *through* a defence and can only report kills; where in the layer cake a striker was lost is not a quantity |
| `C24` | **no communications jamming** | the EF-111s' comms half becomes a `set` key on any airframe — the radar half stays with `C13` |
| `C0` | **no campaign layer** | attrition across ten nights is the campaign's arc |
| `C14` | **no cruise missiles, no ships** | 100 TLAMs opened the war |
| `C4` | **no terrain masking** | the Apaches' treetop ingress has no meaning |

### The honest headline

**Of the three things that actually went wrong on Package Q, FlightBox can measure zero today**:
tanker timing needs `C5`, the departing Weasels need `C1`+`C8`, the saturated command net needs
`C18`. What it *can* measure — and no other campaign in the set can — is the **MiG-29 with and
without its ground control** (missions 7/8), which is a one-line experiment on two existing modules
and speaks directly to the anchor's own explanation of a 6-for-0 result.

---

## Knowledge

### 1. The anchor with its sources

**The opening night.** [The Night That Rewrote Air Warfare: Desert Storm's Opening 14 Hours
(MiGFlug)](https://migflug.com/jetflights/desert-storm-opening-night-f117-apache-air-campaign-1991/)
[T4] — the 02:38 Apache strike on two early-warning radars, the >1,300 sorties and 100 Tomahawks in
14 hours, the 36 F-117s against 34 targets. Corroborated in outline by
[Gulf War: The Air War (Defense Media Network)](https://www.defensemedianetwork.com/stories/gulf-war-the-air-war/)
[T3] and the [Desert Storm timeline (GlobalSecurity)](https://www.globalsecurity.org/military/ops/desert_storm-timeline.htm)
[T3].

**Package Q.** [Package Q Strike (Wikipedia)](https://en.wikipedia.org/wiki/Package_Q_Strike) [T4] —
the 78-aircraft composition (56 F-16 / 14 F-15C / 6 F-4G / 2 EF-111), the Tuwaitha and downtown
Baghdad targets, the SA-3 and 100 mm AAA, the 55 Iraqi fighters by type, the two F-16 losses and both
pilots captured, the tanker weather and early arrival that made four fighters abort, the Weasels
departing on fuel, the 80 %-of-calls workload finding, and the doctrinal consequence. A contemporary
service account exists and is the T2/T3 upgrade path:
[Package Q (Air Force Magazine, January 2016, PDF)](https://www.airandspaceforces.com/PDF/MagazineArchive/Documents/2016/January%202016/0116packageq.pdf)
[T3] — **not retrieved on this pass; flagged in [`PROGRESS.md`](PROGRESS.md)**.

**The Iraqi MiG-29s.** [MiG-29 Fulcrum: specs, history, combat record (e3aviationassociation)](https://e3aviationassociation.com/aviation-articles/mig-29-fulcrum-specs-history-combat-record/)
[T4] — ≈40 aircraft, six lost to F-15Cs in the opening days, none scoring an air-to-air kill, and the
explanation used verbatim above: Soviet doctrine assumed tight ground control, and the Iraqis flew
"essentially alone, without the tactical architecture the design assumed". Corroborating engagement
detail: [USAF F-15C pilot on the only real turning fight of Desert Storm (Aviation Geek Club)](https://theaviationgeekclub.com/usaf-f-15c-pilot-explains-how-he-was-able-to-shoot-down-an-iraqi-mig-29-without-firing-a-single-shot-in-the-only-real-turning-fight-of-operation-desert-storm/)
[T4].

### 2. Where the sources are thin

| Thing | Status |
|---|---|
| The Kari IADS architecture (sectors, hand-off logic, what the loss of a radar actually cost it) | **not sourced beyond the name.** The campaign therefore models "the IADS" as a set of emitting ground units, not as a network, and says so |
| Iraqi MiG-29 sortie profiles, altitudes, emission policy | **not sourced.** Red's doctrine in missions 7/8 is the MiG-29 module's own GCI-led behaviour ([`../modules/mig29/module.md`](../modules/mig29/module.md)), which is a *design decision of ours* |
| Which F-16 blocks flew Package Q and with what load-out | **not sourced on this pass** |

### 3. Why missions 7 and 8 are the pair to build first

They differ by one line — the Red flight's `set brief_gci` — and the anchor supplies the hypothesis
in its own words: the aircraft was designed to be flown to the merge by somebody else, and the Iraqis
flew it alone. FlightBox already implements exactly that dependency as a **typed, latency-charged
command chain** rather than a modifier: without the GCI entries the N019 never gets its scan elevation
or its ZONE, and the jet is a fighter with a radar pointed at nothing. So the experiment is
cheap, it is decided by an existing mechanism, and its result is falsifiable in one number: the
detection time of the Red flight in mission 8 against mission 7.
