# W3 — Desert Storm, the first nights (January 1991)

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

**Status: BUILT AND FLOWN 2026-07-29** — `sim/missions/w3-*.fbm` + `sim/campaigns/w3-desert-storm.fbc`,
ten of ten runnable against a spec that called five blocked, both determinism criteria on the first
attempt. §State carries the numbers; the Spec below is left standing as written.

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

**BUILT AND FLOWN, 2026-07-29 — the sixth of the ten campaigns to exist as files, and the first whose
opponent is a SYSTEM rather than an aircraft.** Ten `.fbm` in `sim/missions/w3-*.fbm` plus
`sim/campaigns/w3-desert-storm.fbc`, run as a campaign, replayed step by step, and measured. **No file
under `sim/src/`, `sim/tools/` or `sim/assets/` was touched** (`git status --porcelain` lists eleven new
untracked files and **no modified one**), so the **195** pre-existing `sim/missions/*.fbm` are
byte-identical **by construction rather than by comparison**.

### The spec's own headline is superseded, and by measurement

This file said: *"Of the three things that actually went wrong on Package Q, FlightBox can measure ZERO
today."* Rule 7 says a blocked mission is re-checked against the **tree** rather than against a gap's
status line. Every blocker was re-checked one by one:

| The spec said | The tree says, 2026-07-29 |
|---|---|
| `C1` — nothing emits, launches or shoots | **CLOSED.** Nine positions; `sa2` `sa3` `zsu23` `p18` fly in this campaign. The ground-launch fix of 2026-07-29 gave their rounds a trajectory: W3's S-125 puts four V-601 inside 8.4 m of an F-16 |
| `C22`/`C23`/`C24` — no connected defence | **CLOSED.** `net` with `link wire`, `control`, `period`, `hold`, `wcs`, per-member `autonomy`. W3's whole sortie 01/02 pair is that mechanism |
| `C8` — no HARM, no Mk-84 | **BUILT** minus the rocket pod. `agm88` and `mk84` are the campaign's two weapons; the anti-radiation round hits a battery at **2.8 mm** from 20 km |
| `C26`/`C27` — no suppressed-vs-destroyed, no ARM cue | **CLOSED.** `objective suppress … emitting <s>`, `set emcon free <offS> [<onS>]`, `set attack_mode arm`, `set arm_class` — all four decide a W3 sortie |
| `C2` — no time of day | **CLOSED.** All ten declare a night clock |
| `C0` — no campaign layer | **CLOSED.** This campaign has a `.fbc` and a one-step chain |
| `C5` — no tanker | **STILL OPEN**, and sortie 06 measures what is underneath it |
| `C18` — no radio between units | **STILL OPEN.** Sortie 09 says so and measures the substitute instead |
| `C15` — no package coordination | **STILL OPEN**, and sortie 10 puts a price on it |
| `C7` — only two flyable modules | **BUILT-and-open**, and W3 flies **no catalogue row at all** — see below |

**So the spec's count of five runnable missions is now ten, and the headline is exactly inverted for
two of the three failure modes.**

### The three Package Q failure modes: which FlightBox can stage and which it cannot

| # | The anchor's failure | Can FlightBox stage it? | What W3 did |
|---|---|---|---|
| **2** | **the Wild Weasels left early on fuel**, leaving the strikers without suppression | **YES, fully.** Every ingredient exists: an emitting, shooting, magazine-limited battery; an anti-radiation round whose reach is measurable; a suppression verdict | sorties **03 / 04 / 05** + attribution run **A2**. It is the campaign's strongest result and it produced an answer the anchor does not contain |
| **1** | **tanker weather and early arrival**; four fighters aborted | **NO, and it is blocked twice over.** `C5` blocks the cause (no tanker, no boom, no external tank, no fuel-driven RTB). Underneath it, the EFFECT is blocked too: `FBPilot::CanPressOn` is the only line that reads the BINGO warning, and the state-machine branch that calls it is unreachable | sortie **06** + attribution run **A4**: the warning is ON for 5 200 telemetry rows and `eng_state` is byte-identical to a run without it |
| **3** | **the mission commander took 80 % of the calls**, an impossible workload | **NO.** `C18`: there is no voice net, so nothing with a call capacity exists to saturate | sortie **09** re-scopes it to the ONLY saturable command object in the tree — the flight's cooperative sort — and says in its own header that this is not an equivalence |

**One of three, and the campaign says which.**

### Why no catalogue row flies, in a campaign whose cast table asks for four

`f15c` (the anchor's escort), `mig23`/`mig25` (~45 of the 55 Iraqi fighters aloft) and `ef111` (the
jammer) all exist as rows since 2026-07-28, and W3 uses **none** of them:

| Row | Why refused |
|---|---|
| `f15c` | `ACCEPTED` — **as a flight model.** Its promotion gate measures eight aerodynamic anchors plus a roll plant and **nothing about weapons** (`../modules/air/module.md` A13). It can now shoot, but no gate has ever measured whether it shoots *like an F-15C* |
| `mig23` / `mig25` | **`ALPHA`.** An `ALPHA` row may not answer a campaign question at all, and W3's whole Red side would have been one |
| `ef111` | a mover with **no radar-jamming half** (`C13`). Its comms half is a `set` key on any airframe and needs no airframe |
| any gun-only row | `A15`: **no campaign may score a catalogue gun engagement.** W3 scores none |

**The substitution direction, stated once for the whole campaign:** MiG-29s stand in for MiG-23/MiG-25,
which makes **Red materially stronger than history**; F-16s stand in for F-15C, F-4G and EF-111, which
makes **Blue no weaker**; and a ZSU-23-4 stands in for the anchor's 100 mm AAA, which makes the AAA
layer **materially weaker** — at 5 000 m it cannot reach the ingress at all.

### The arena

Al-Tuwaitha nuclear research centre ≈ 33.20 N 44.52 E [T4, approximate]; Baghdad 33.31 N 44.36 E [T4].
Ingress west→east along 33.0–33.4 N. `--elev const`, 0 m datum (the Baghdad plain is ~34 m), **no
terrain masking (`C4`)**. Scale: 1° of longitude = **93 145 m**, 1° of latitude = 111 132 m [DERIVED].
The forward early-warning set stands 86 km west of Tuwaitha and 67 km outside the S-75's envelope,
which is the anchor's own geometry: border radars were killed because they were reachable.

**The net is a buried cable (`link wire`) and that is sourced doctrine, not convenience.** A
`link radio … mast 12` over the same 67 km would have a 28.5 km horizon [DERIVED,
`4.12·(√12+√12)`] — i.e. no link. Kari was hardwired and centralised, which is precisely why its radars
had to be destroyed rather than talked over.

**Night, once, for all ten:** 1991-01-17/19 at 00:00 Z = 03:00 local, sun ≈ **−48°** [DERIVED]. Nothing
in this tree emits light, so `sensors/FBVisualSystem` contributes nothing and **every W3 merge is
eyeless**. The campaign has therefore measured **radar and radio warfare, not night warfare**, and that
is a limit on the claim rather than on any mechanism. Package Q itself was a morning strike; the night
is the campaign's setting and W3's own §3 puts all ten sorties there.

### The ten sorties, their fingerprints and their answers

Campaign exit **3**, step exits `0 3 3 3 3 3 3 3 3 3`. Campaign fingerprint under `--elev const`:
`3490c4fab3f25f533ead565e393cc23d234067e827e5ea7ba733408988f1fa1a`. Wall clock for the whole campaign:
**53.2 s**.

| # | Mission | ctrl | exit | fingerprint | The answer to its one tactical question |
|---|---|---|---:|---|---|
| 1 | `w3-01-ew-radar` | — | 0 | `77df6f2fe18f1f2e` | **One bomb on the forward early-warning set is worth the whole strike.** Node killed t = 104.7 at `aimErrM` 43.3 m; the ring's three `emcon hold` positions had come up 8.0 s into the run on the cable's cue and go `AUTONOMOUS fallback=hold` at t = 116.0. **0 `site TRACK`, 0 `site LAUNCH`.** The second wave releases at t = 421.0 and is never fired at |
| 2 | `w3-02-net-intact` | 01 | 3 | `2b7bb7d497a4dde8` | **[ctrl 01, ONE token: `brief_master_arm arm → sim`.]** Node alive: **10 `site LAUNCH`**, four V-601 at 8.35 / 8.17 / 7.28 / 7.42 m against a 10 m fuze. **And the result is not a shoot-down**: `q2pen` survives and its `stores` system FAILS at t = 392.4, so it reaches its aim point at t = 448.3 with nothing to drop. **0 of 2 strikers reached release; 0 ground targets killed** |
| 3 | `w3-03-weasel-close` | — | 3 | `90bba24b8bf76fee` | **The Weasel that presses in kills the battery at 2.8 MILLIMETRES from 20 km** (`tofS` 52.08), t = 59.7, orphaning two V-601 fired 20 s earlier. `mission SUPPRESSED emittingS=59.7 allowanceS=120` — met, and met because the site died. 2 of 2 strikers released; `q3tgt` destroyed at 23.6 m, `q3hgr` (`target_hard`) INTACT at 21.5 m |
| 4 | `w3-04-weasels-leave` | 03 | 3 | `c0eff8b98797bab1` | **[ctrl 03, one fact: where the pair is.]** From 42 km both AGM-88 fall **10.5 and 7.3 km SHORT** — the round's reach is bracketed between 20 km (a hit) and 42 km (dirt). The battery lives, `SUPPRESSION_LOST` at t = 120.1 — **and it still fires all four rounds at the DEPARTING WEASELS** (brg 277–280°), so the strikers release unopposed and kill the same target at the same tick as the control |
| 5 | `w3-05-emcon` | 03 | 3 | `f9a9770250eaa961` | **[ctrl 03, one line: `set emcon free` → `free 30 200`.]** Off the air at t = 29.9 = **57.4 % of the round's 52.08 s flight**; the AGM-88 coasts and hits dirt **214 m short**. Site INTACT. It comes back at t = 200.0 and spends its whole magazine at 25.7–34.3 km on the **departing** Weasels (brg 268–270°), zero arrivals. `SUPPRESSION_LOST` at t = 290.2. Strikers unaffected |
| 6 | `w3-06-bingo` | — | 3 | `f5b0904ebe45f0b1` | **A fuel state is not a decision in this tree, and the reason is a preempted branch.** `warn_active` = 2 (BINGO) for all 5 200 rows; `eng_state` search→closing→attack→support→**defend**→search at t = 40.0 / 156.1 / 180.1 / 181.1 / **210.0** — **the identical seven transitions at the identical seven ticks** as A4, which deletes the bingo line. Total price of the brief: **7 of 184 columns** and zero metres |
| 7 | `w3-07-mig-cap` | — | 3 | `38a02bbbe00b3e99` | **With its controller, Red shoots FIRST — by 1.1 s.** First Red contact t = **54.9 at 52.76 nm**; five Red launch solutions from t = 195.4 (15.05 km); Blue's first at 196.5. What Blue has is the round: 2.47 m and a kill against Red's 12.94 m **inside a 13.8 m fuze and no kill**. Strike complete at t = 190.0, 5 s before Red's first shot |
| 8 | `w3-08-mig-alone` | 07 | 3 | `8cfb0b73f1ecdcbf` | **[ctrl 07, ONE deleted line.]** **0 Red radar contacts and 0 Red launch solutions**, against 50 and 5. Blue's own timeline does not move (first solution t = 196.3 against 196.5); its kill comes **104.0 s later** (t = 315.3 at 8.94 m) because it has to run the target down. The run lasts 97.6 s longer |
| 9 | `w3-09-saturation` | — | 3 | `0f206445e4f38ef1` | **The four-ship sort does not collapse — it CHURNS, and the churn sits on the lead.** `flt_switch` peaks at **57 / 37 / 19 / 6** for positions 1–4 (one re-sort every 5.8 s on the lead) and `flt_dup` reaches 1 on **all four**: every member held a duplicate the cost function flags and never resolves. 4 of 4 released, all four aim points destroyed, one F-16 lost to an R-27R at 0.93 m |
| 10 | `w3-10-package-q` | chain | 3 | `35eff34494823487` | **The package works at 24 aircraft, and the spec's condition is met on both halves: 8 of 8 strikers released, 15 of 16 recovered.** 66 actors, 308.5 s of sim in **11.7 s** of wall clock. Three findings sit inside it: `C15` priced (three of four AGM-88 into one battery, two of them after it died), the Weasel absorbing a second magazine (all six V-750 fired SOUTH at the SEAD flight), and **Red killing one of its own** |

### What a suppression element is worth — the campaign's strongest result

Four runs on one geometry, three of them among the ten and one an attribution run:

| run | the SEAD element | the battery | strikers reaching release | target |
|---|---|---|---:|---|
| **03** | presses to 20 km, shoots, hits | **DESTROYED** t = 59.7 | **2 of 2** | destroyed |
| **04** | shoots from 42 km, round falls 10 km short, turns home | alive — **magazine emptied at the WEASELS** | **2 of 2** | destroyed |
| **05** | presses to 20 km; the battery goes dark at 57.4 % of the flight | alive — **magazine emptied at the DEPARTING WEASELS** | **2 of 2** | destroyed |
| **A2** | **none** | alive — **magazine emptied at the STRIKERS**, four arrivals at 5.84–7.99 m | **1 of 2** | **INTACT** |

**The suppression element is worth one striker's release and the target — and it is worth that even
when its missile falls 10 km short**, because a FlightBox battery has no IFF interrogator and no threat
priority and splits a finite magazine over whatever is in reach. *Being shot at is most of what a
Weasel does here.* That is a mechanism result, not a doctrine one, and it is the sharpest thing this
campaign found.

### What emission discipline is worth to a position under a package

| | `03` radiates throughout | `05` dark from t = 29.9 to 200.0 |
|---|---|---|
| the anti-radiation round | arrives at **0.0028 m** | hits dirt **214 m short** |
| the position | **DESTROYED** at t = 59.7 | **INTACT** at the end of the run |
| `site LAUNCH` | 2 (both orphaned by its own death) | 4, all at 25.7–34.3 km at the departing Weasels |
| arrivals on an aircraft | 0 | 0 |
| `objective suppress … emitting 120` | **met** — because it died | **lost** at t = 290.2 |
| strikers reaching release | 2 of 2 | 2 of 2 |

**Emission discipline is worth the position and nothing else.** The crew survives by suppressing
itself for 170 s, and when it comes back it spends its magazine on the aircraft that are leaving. That
is `C26`'s *suppressed against destroyed* with a number on both sides, and it says the honest thing:
in this tree a battery that dodges a HARM has bought its own life and paid for it with the engagement.

**The 214 m is a MARGIN on one shutdown time on one geometry, and the campaign does not generalise it
into a percentage.** `../air-to-ground.md`'s bisected boundaries (85.0 % frontal, 88.1 % at 35°) are the
weapon's own numbers on the weapon's own proof geometry; here the shot is dead on the nose
(`brgDeg` −0.057), so the zero-effort miss at launch is ≈ 20 m and almost the whole 214 m is the coast.

### Rule 11 applied, and this time BOTH policies were measured in one round

`o1-02`, `o1-03`, `o5-03` and `o2-04` are the same deleted line (`set brief_gci`) in four theatres, and
they disagreed. O2 found the reason: the deciding line is `set n019_emission`. W3 is the fifth theatre
and it declared its policy **before** flying — `off`, the documented power-up state — and then measured
the other one:

| run | `n019_emission` | `brief_gci` | Red contacts | Red launch solutions | Blue's first shot | run ends |
|---|---|---|---:|---:|---|---|
| `w3-07` | `off` | **present** | 50 | 5 | t = 196.5 | 272.8 s |
| `w3-08` | `off` | **deleted** | **0** | **0** | t = 196.3 | 370.4 s |
| **A3** | **`illum`** | deleted | **50** | 7 | — | **272.8 s** |

**Under `illum` the deleted brief costs nothing measurable; under `off` it costs every contact and
every shot.** The four earlier campaigns were each right about their own file, and the comparable
quantity across all five is *"what the controller is worth GIVEN an emission policy"*. W3 is the first
of the five to fly both sides of that in one campaign instead of inheriting the answer.

### The carry: one callsign, and its value is a property of the net's topology

`carry units ground stores`, not narrowed. The chain is **01 → 10** and carries exactly one callsign,
`karinod`. Sorties 02–09 are pairwise disjoint from both ends and from each other in every unit they
can lose, aircraft *and* ground; sortie 01's own second aim point `kariref` exists in no other file, so
nothing this campaign destroys arrives later as an objective naming a deleted unit.

`campaign CARRY unit=karinod action=drop reason="destroyed in an earlier mission"` — one line, and this
is what it is worth, measured by running sortie 10 **twice**, as campaign step 10 and standalone:

| quantity | in campaign (node dead) | standalone (node alive) |
|---|---:|---:|
| `net CUE` | **16** | **25** |
| `site RADIATE` / `site TRACK` / `site LAUNCH` | 3 / 2 / 8 | 3 / 2 / 8 |
| first `site RADIATE` / `TRACK` / `CUE` / `LAUNCH` | 0.0 / 6.8 / 8.0 / 37.5 s | **identical** |
| strikers reaching release | 8 of 8 | 8 of 8 |
| aircraft lost / ground killed | 3 / 5 | 3 / 5 |
| run length | 308.5 s | 308.5 s |
| telemetry | 30 of 58 files byte-identical; the other 28 differ in **1 to 13 of 184–193 columns, every one of them RWR or datalink bookkeeping**. **No trajectory column moves** | |

**So killing a forward early-warning radar is worth the whole strike when it is the net's only node
(sorties 01/02) and 36 % of the cue traffic when it is not (sortie 10).** The value of that bomb is a
property of the net's TOPOLOGY, not of the bomb — which is the same shape as rule 11 one layer down,
and it is a direct qualification of O5's *"one Mk-84 on the field's P-18 costs its missile layer every
launch for two nights"*: O5's field had one node.

### Both determinism criteria, measured on the first attempt

Under `--elev const`, read out of `campaign-summary.txt` rather than assumed:

| # | Criterion | Result |
|---|---|---|
| **1** | 3 repetitions × `--threads 1/2/4` produce one campaign fingerprint | **9 runs, 1 fingerprint** `3490c4fab3f25f533ead565e393cc23d234067e827e5ea7ba733408988f1fa1a`, exit 3 in all nine |
| **2** | every step's per-mission fingerprint equals that mission run STANDALONE with step *k−1*'s state | **10/10 MATCH**, exit codes included, on the first attempt |
| **1 — re-run 2026-07-30** | the same criterion under the branch-order change of `b433950` ([`../pilot.md`](../pilot.md) §7.4a) | **9 runs, 1 fingerprint** `bfe4938ed90172291e151adbcd366200eb41efd94421c4bae8ecf08bf2749d8c`, `--elev const`. **The value above is kept with its date; this is the current one.** Step exits `0 3 3 3 3 3 3 3 3 3` — unchanged |
| **2 — re-run 2026-07-30** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Per-step fingerprints, 2026-07-30 | | `77df6f2fe18f1f2e 2b7bb7d497a4dde8 90bba24b8bf76fee c0eff8b98797bab1 f9a9770250eaa961 902cda7d0c701dcc 91adfacc39683304 2e3ca986d19458b2 5aece68a66b6b82c 2dfd506a232ec7e4` |

**And the replay was run after the FIRST mission**, on a throwaway one-step `.fbc`
(`sim/campaigns/w3-step1-check.fbc`, deleted afterwards): `01 … campaign fp=77df6f2fe18f1f2e standalone
fp=77df6f2fe18f1f2e MATCH`. **Annotating the ten files with their MEASURED blocks after the runs left
all ten per-mission fingerprints and the campaign fingerprint unchanged** — the check that a comment is
a comment.

### What this campaign found while building, none of it fixed here

Rule 9: *the defect sits in the seam you did not look at.* W3 found four, and none is in the air-defence
network the campaign was written about.

| # | Finding | The measurement that pinned it |
|---|---|---|
| **1** | **`FBPilot::CanPressOn` is unreachable, so the BINGO warning — the only fuel signal in the tree — can never decide anything.** The intercept state machine reads it at exactly one branch, `EngState_ == Defend && TimeS_ − IntThreatLastS_ >= DefendHoldS`; the chain ends in `else if (EngState_ != Abort)`, which fires on the first tick after `defendDue` goes false, and `IntThreatLastS_` is refreshed on every tick `mustDefend` holds — so the elapsed time is still one tick when the general branch takes the state away. The same branch also gates *"out of weapons → abort"* | `w3-06` against **A4**, one line apart: `warn_active` = 2 (`FBWarnBingo`) for **5 200 of 5 200** telemetry rows, and both escorts' `eng_state` columns are **byte-identical**, Defend held 28.9 s (181.1 → 210.0) and recovered into `search`. **7 of 184 columns** move in total, all of them `warn_active` plus command-bus counters |
| **2** | **A proximity fuze has no team test, and at package density that is an attrition channel.** The fuze is resolved by the runner against every published pose except the launcher's — the same boundary that makes a seeker blind to identity — but there is no fire-control inhibition anywhere above it | `w3-10`: `qamia1_r27r_25`, launched t = 275.0 with a solution at 32.98 N 44.63 E (a Blue escort), detonated **11.74 m from `qamib2`** — a MiG-29 of the *other Red flight* — at t = 293.2 and killed it. **1 of the 3 aircraft lost in the capstone was killed by its own side.** A 2v2 campaign cannot see this; 24 aircraft can |
| **3** | **`C15` has a price and it is now a number.** With no deconfliction and no lead tasking, a four-ship SEAD flight is four aircraft that happen to be aimed at the same thing | `w3-10`: three AGM-88 into `karisa3` at **0.0028 / 0.0052 / 1.10 m**, arriving at t = 60.2 / 61.0 / 65.7. **The site died on the first**; two thirds of the expenditure that arrived went into a corpse |
| **4** | **A battery has no threat priority, so the nearest firm track empties the magazine — whoever it is.** Measured four times in four sorties, in three different directions | `w3-04`: all four V-601 west at the departing Weasels. `w3-05`: all four at 25.7–34.3 km, also west, also departing. `w3-10`: all six V-750 SOUTH (brg 147–182°) at the SEAD flight while eight strikers ran in. `w3-02` vs **A1**: deleting a striker that never dropped anything and stayed 95 km from every fire unit changes the belt from **10 launches** to **8** and turns its wingman's outcome from *survived with a dead SMS* into *shot down at t = 358.9* |

**A fifth thing is a measurement rather than a defect, and it is the one a strike campaign most needs
to know:** in this tree **a striker is stopped by system damage far more often than by destruction.**
`w3-02`'s `q2pen` takes four V-601 inside 8.4 m, survives all four, and arrives over its target with
eleven systems failed including `stores`. Read as a loss table the sortie is 0–0; read as a package
result it is a total failure. Every W3 header therefore reports *strikers that reached release*.

### Where the built campaign departs from §3's table, and why

The Spec above is **left standing as written** and the departures are listed here, because each was
discovered by building:

| §3 says | Built as | Reason |
|---|---|---|
| mission 1 carries **2 × Mk-82** against a soft point target plus a bunker | **2 × Mk-84** against a `p18` that radiates and cues, plus the ring it cues | `C8` built the Mk-84 and `C1`/`C22` built the thing worth attacking. The spec's own question ("can a level laydown kill a soft point target reliably enough…") is answered on the way: `aimErrM` 43.3 m, and it killed |
| mission 2 is a **two-ship strike, no opposition**, 3 aim points | the **control run of mission 1**, one token apart | the campaign needed the node's value as a number, and §2's contract says the subject is the package. A no-opposition release-accuracy sortie measures the bomb, which `attack-ccrp.fbm` already does |
| mission 3 is a **fighter sweep 10 minutes ahead of a strike** | dropped; its slot went to the SEAD control | `C15`: there is no way to declare "10 minutes ahead", so the sweep's own question ("does it clear the corridor or drag the fight onto the strikers") is not askable. Sorties 07/09 measure the fighter layer instead |
| missions 4/5 measure **the size of the SEAD hole** | measure what a suppression element is **worth** | the hole closed. Rule 7 |
| mission 6 asks whether **the pilot's BINGO rule turns a fuel state into a decision** | asks the same question and answers **no**, with the branch named | the question survived the re-check; the answer did not |
| mission 9's Red is **6 MiG-29 in two flights** and Blue **8 F-16 in two flights** | exactly that | unchanged |
| mission 10 is **16 F-16 + 8 MiG-29 + SAM/AAA** | exactly that, plus a second early-warning node | the chain needs a node sortie 01 cannot reach, or the capstone's defence would simply be absent |

### Conservation, and the gates

`git status --porcelain` lists **eleven new untracked files and no modified one**: ten
`sim/missions/w3-*.fbm` and one `sim/campaigns/*.fbc`. Gates: `make core-lib gym native wasm`
warning-free; `verify-layers` *"301 files, 828 internal include(s), 12 layers — no upward include, 3
restricted header(s) respected, 6 registry reader(s) inside the perception boundary, 1 antenna-cue
poster(s), 288 file(s) in their layer's namespace (5 C-island file(s) exempt)"*; `verify-models` *"4
upstream-backed model path(s) match assets/MODEL-DELTAS.md (1 declared delta(s), 34 FlightBox-own)"*;
eight harnesses rc = 0.

## Gaps

**Re-checked against the tree on 2026-07-29 and struck through where the round measured them closed.
Ten of ten missions ran.**

| ID | What is missing | Blocks here |
|---|---|---|
| ~~`C1`~~ | **CLOSED and FLOWN.** `sa2` `sa3` `zsu23` `p18` emit, cue, track, gate an envelope and fire out of a finite magazine in eight of the ten sorties | — |
| ~~`C8`~~ | **BUILT and FLOWN** (minus the rocket pod). `agm88` and `mk84` are this campaign's two weapons | — |
| ~~`C22`~~/~~`C23`~~/~~`C24`~~ | **CLOSED.** The `net` block is the whole subject of sorties 01/02 and the capstone | — |
| ~~`C26`~~/~~`C27`~~ | **CLOSED.** `objective suppress`, `set emcon free <offS> [<onS>]`, `set attack_mode arm`, `set arm_class` each decide a sortie | — |
| ~~`C2`~~ | **CLOSED.** All ten declare a night clock | — |
| ~~`C0`~~ | **CLOSED.** `w3-desert-storm.fbc`, a one-step chain, both criteria passed | — |
| `C5` | **no tanker, no boom, no external tank, no fuel-driven RTB** | **Package Q's first failure mode, and it is blocked TWICE.** Below `C5` sits the finding of sortie 06: `FBPilot::CanPressOn` is the only line in the pilot that reads the BINGO warning and its branch is unreachable, so even a declared fuel number decides nothing. Fixing `C5` alone would not make failure mode 1 measurable |
| `C18` | **no radio between units** | **Package Q's third failure mode.** There is no channel with a capacity, so nothing can saturate. Sortie 09 re-scopes the question onto `pilot/FBFlightPicture` — the only saturable command object in the tree — and says in its own header that this is not an equivalence |
| `C15` | **no package coordination** — no time-on-target, no deconfliction, no lead tasking | the *definition* of a package, and now with a price: sortie 10 put three of a four-ship's four AGM-88 into one battery, two of them **after** it was destroyed. It also deletes the spec's own mission 3 (a sweep "10 minutes ahead" cannot be declared), whose slot went to the SEAD control |
| `C7` | **`ALPHA` rows and an unmeasured weapon half** | W3 flies **no catalogue row at all**. `f15c` is `ACCEPTED` **as a flight model** and no gate has measured its weapons (`A13`); `mig23`/`mig25` are `ALPHA` and may not answer a campaign question; `A15` forbids scoring any catalogue gun engagement. Substitutions are declared per file with their direction |
| `C13` | **no RADAR jamming** (the comms half is `C24` and closed) | the EF-111s' decisive half. `ef111` exists as a mover and has nothing to jam with |
| `C6` | **no live controller** | `set brief_gci` is static text with its own timestamps, which is enough for the 07/08 experiment and not enough for a controller that fails *during* an engagement |
| `C4` | **no terrain masking** | over the Iraqi plain a smaller lie than over the Bekaa, but it is why the Apache ingress became an F-16 laydown and why no sortie can hide behind anything |
| `C14` | **no cruise missiles, no ships, no moving ground units** | 100 TLAMs opened the war and none of them can be flown |
| `C17` | **an airfield has no state** | not used by W3: no sortie attacks a runway, because a `target_hard` whose death changes nothing is not a target |

### The honest headline, replaced by measurement

**The old one said: "Of the three things that actually went wrong on Package Q, FlightBox can measure
zero today." It is now ONE OF THREE, and the campaign names which and why:**

| failure mode | status | the measurement |
|---|---|---|
| **the Weasels left early** | **stageable in full** | four runs on one geometry; the suppression element is worth one striker's release and the target, and it is worth that even when its round falls 10 km short |
| **the tanker/fuel timing** | **not stageable, and blocked twice** | `C5` blocks the cause; `FBPilot::CanPressOn`'s unreachable branch blocks the effect. Measured: the BINGO bit set for 5 200 rows and a byte-identical `eng_state` column |
| **the saturated command net** | **not stageable at all** | `C18`. What was measured instead is the flight sort: `flt_switch` 57 / 37 / 19 / 6 across a four-ship and `flt_dup` = 1 on every member |

**And the thing this campaign found that was in no spec:** a proximity fuze has no team test, so at
24 aircraft a side's own missile becomes an attrition channel — one of the three aircraft lost in the
capstone was killed by a MiG of the other Red flight, at 11.74 m against a 13.8 m fuze.

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
