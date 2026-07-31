# Journal — the chronicle of the rounds

**What this file is:** one line per finished round, in the order they happened — commit, what it
built, what it measured. It is history, not a plan and not a contract.

- **What each area must do** → the `## Spec` section of its topic file ([`INDEX.md`](INDEX.md)).
- **What is built right now** → the `## State` section of that same file.
- **What is missing, and what was tried and rejected** → its `## Gaps` section.
- **What comes next, in order** → [`roadmap.md`](roadmap.md).

Every round adds a line here; nothing here is ever rewritten to look better. Rejected approaches are
kept — in the Gaps of the file they belong to, with their measurements.

State of the entries below: commit `793e1fe` + the model-root/delta round (2026-07-27).

## Maturity per area

| Area | State | Doc |
|---|---|---|
| FDM adapter | **finished** — instanceable, IC-sealed, damage and stores channels | [fdm.md](fdm.md) |
| Core / avionics bus | **finished** — typed blocks with three-state validity, command bus with acknowledgement | [core.md](core.md) |
| Mission orchestrator | **finished** — four steps, no mission knowledge in the code | [missions/runtime.md](missions/runtime.md) |
| Multi-unit | **finished** — cast as mission data, thread per unit in the gym, deterministic | [missions/runtime.md](missions/runtime.md) |
| Formation | **built** — roles as mission data, station keeping, target sort, cover rule; rejoin and lead-level tactics missing | [formation.md](formation.md) |
| Sensors | **built** — datalink, radar, RWR, IRST, countermeasures. Without terrain masking. | [sensors.md](sensors.md) |
| Weapons | **built** — AIM-120, Mk-82, M61A1, ground targets, damage model | [weapons.md](weapons.md) |
| Pilot AI | **in progress** — takeoff/route/landing, BFM, BVR intercept, air-to-ground all fly; refinement ongoing | [pilot.md](pilot.md) |
| Renderer | **built** — stage split complete. Units and weapons still invisible. | [render/renderer.md](render/renderer.md) |
| HUD | **built** — generic default HUD + full F-16 symbology, coverage AA | [modules/f16/module.md](modules/f16/module.md) |
| Cockpit displays | **not started** — the values are on the bus, the presentation is missing | [clients/clients.md](clients/clients.md) |
| HOTAS | **not started** — deliberately last, it is only a mapping | [clients/clients.md](clients/clients.md) |

## Chronology

### 2026-07-29 — Koordinatenrahmen: dreimal derselbe Fehler, also wurde der TYP gebaut und nicht die vierte Stelle geflickt

Drei Runden hintereinander fanden je einen Defekt derselben Art — ein **Weltwinkel** in einem
**körperfesten** Befehl. Statt den nächsten zu flicken, wurde der ganze Baum abgelaufen: **41
Winkelübergaben** inventarisiert, jede mit Erzeuger-Rahmen, Verbraucher-Rahmen und Urteil
([`sensors.md`](sensors.md) §10 — die korrekten stehen mit drin, eine Liste nur aus Fehlern sagt nichts
über Abdeckung). **Vier waren falsch, alle vier in derselben Richtung, alle vier behoben.** Drei waren
einzeln bekannt (O5, Katalog-Runde, W5), der vierte — die gebriefte GCI-Höhe der Katalogzelle — fiel
beim Ablaufen an.

**Die drei Zahlen.** `o5-02-scramble`: die MiG-Rotte steigt mit +5,65…+6,01° Nick, der Angriff steht bei
−3,41…−3,74° körperfest, die N019-Suchleiste ist ±6,0°; kommandiert wurde **+2,891°** (Unterkante
−3,109°, der Angriff 0,3–0,6° darunter) — **null Radarkontakte in 700 s**. Jetzt **−2,754°**: erster
Kontakt **t = 48,0 s auf 25,70 sm**, vier R-27R, **beide Angreifer abgeschossen**, Exit **3 → 0**. Der
Live-Netz-Cue der Katalogzelle: **+0,025° → −3,358°** auf `air-awacs-cue` (eine steigende MiG-25). Und
der **Spawn-Tick**: `FBMissionBoot` veröffentlichte einen Zustand mit NUR der Position, also lief für den
ganzen Tick 0 jede Körpertransformation im Baum gegen die Identität — Empfänger wie Strahler. Auf der
committeten `pair-2v2-f16.fbm` meldete viper1 eine Feuerleitung bei **brgDeg = −180,0** mit Signal
0,9998, wo die geflogene Geometrie −0,0 ist.

**Strukturell gesichert statt gezählt.** Der Rahmen ist jetzt ein **Typ**: `core/FBBodyAngle` ist über
genau drei benannte Konstruktoren erreichbar (`FromTrueBearing`, `FromWorldElevation`, `Measured` — die
EINE Hintertür, benannt, damit ein unverdienter Gebrauch an der Aufrufstelle sichtbar ist). Es gibt keine
Syntax für „nimm einfach dieses double". `FBCommandBus::PostAntennaAz/El` nehmen nur diesen Typ und sind
die **einzige** Stelle im Baum, die `FBCommandTarget::RadarSlewAz/El` in einem Post nennen darf;
`make -C sim verify-layers` druckt **`1 antenna-cue poster(s)`** und fällt beim zweiten — dieselbe Form
wie die Zahl der Registry-Leser daneben. Die Bodenstellung ging durch dieselbe Umrechnung, obwohl sie
schon richtig war (eine Lafette veröffentlicht Roll = Nick = 0): arithmetisch ein No-Op, byte-identisch,
und die Richtigkeit hängt jetzt an der Transformation statt am Zufall.

**Der Preis, offen ausgewiesen.** Der Spawn-Tick-Fix bewegt **190 von 193** Missionen, weil er die ersten
0,01 s der Regelung jeder Zelle mit Zellen versorgt, die ihre echte Lage UND ihre echte Geschwindigkeit
kennen. Die reinen Rahmen-Fixes allein bewegen **35** — genau die Missionen mit `brief_gci` oder Netz-Cue.
Acht Exit-Codes wandern, jeder einzeln belegt und jeder eine Messerschneide am Zünderradius: `net-belt-high`
(V-750-Zündung 11,55 → 7,82 m gegen 12 m Zünder ⇒ Flugsteuerung *degraded* → *failed*), `bvr-duel-decided`
(2,36 → 9,67 m gegen 10 m), `cm-beam-only` (7,83 → 9,94 m), `duel-doctrine-mig` (9,35 → 13,22 m gegen
13,8 m), `o4-04`/`o4-06` (Trades statt Entscheidungen), `o4-09` (die Nacht-Messung schrumpft von sechs
Spalten auf eine, weil ihr Tageskontroll-Kampf jetzt bei 29,5 s statt 76,9 s endet), `o5-02` (siehe oben).
Die halbe Variante — nur die Lage statt des ganzen Zustands — wurde gemessen und verworfen: sie bewegt
158 Missionen und lässt das Artefakt dahinter stehen (ein Jet mit 5° Nick und 0 kt).

**Tore.** `core-lib gym native wasm` warnungsfrei, sieben Harnesses rc = 0, `--threads 1/2/4` identisch
über alle 193, `verify-models` grün (1 deklariertes Delta), `verify-layers` 301 Dateien / 6 Registry-Leser
/ **1 Antennen-Poster**. Alle fünf gebauten Kampagnen bestehen beide Determinismus-Kriterien erneut —
9 Läufe je ein Fingerabdruck, 10/10 Schritte einzeln nachgespielt; die neuen Werte stehen in den
`## State`-Abschnitten neben den alten mit Datum.

### 2026-07-28 — `C7` built: eighteen catalogue rows, ten generated decks, and the gate they do not pass

`modules/air/` exists: ONE class with eighteen `core/FBAircraft.h` rows, ten JSBSim decks GENERATED by
`tools/gen_air_decks.py` from eight published anchors apiece, eight kinematic movers, five pilot tiers,
seven new rounds, six new guns, and one new sensor slot (`sensors/FBNetLinkSystem`, the controller feed
that needed a block of its own because a fighter's Datalink block already carries Link-16 PPLI).
The perception boundary did not widen: **6 registry readers**, unchanged. All 133 existing missions are
byte-identical at `--threads` 1/2/4.

**The round's own gate says the round is not finished, and that is the entry.** `make -C sim test-air`
measures each deck's eight anchors against the bands `flight-model-recipe.md` §7.1 derived from the
MiG-29 deck's own misses: **0 of 10 rows `ACCEPTED`, 10 of 10 `ALPHA`.** A2 (Vmax at sea level) is
inside ±5 % on all eight rows that publish one (−0.3 %…−1.4 %) — the closed-form inversion reproduces
itself exactly where it was inverted. A1 (Vmax at altitude) misses on nine of ten, always low, −9.5 %
to −58.1 %; `mig25`'s worst-in-set result was predicted by R2 in advance. Mass closure is inside ±1 %
on ten of ten.

**Three steps of the recipe did not survive contact with the data, all recorded as R11–R13.** A4 can
invert a subsonic `CD0` for NO row — the published climb rates imply negative drag on two rows and 0.0038
on the F-5E against its own published 0.0200 — so it became a probe and the subsonic level is taken from
the catalogue's one published `CD0`, the same generalisation §4.1 already makes for `e`. A deck without
a throttle channel cannot light its afterburner (F-5E: M 1.14 instead of M 1.63). A deck whose mains sit
0.06 L behind the CG cannot rotate at all (take-off run 4 643 m against a published 610).

**The attribution instrument is built and it answers.** `tools/fb_tournament.py --attribution <row>`
prints `band_deck` against `band_doctrine` and, below the 0.25 rule, prints the two bands INSTEAD of a
result. `mig21` 2.4/38.5 = 0.061 · `mig23` 4.5/32.3 = 0.140 · `f15c` 1.9/39.5 = 0.048 · `su27`
3.7/1231.7 = 0.003 · `mig25` 0.5/0.0 = ∞, not admissible. The control cell disagrees with `band_deck`
on `mig21` and is a no-op on `f15c`; §Spec 11 calls that a defect of the instrument and it is booked as
one.

### 2026-07-28 — clouds: the proof set becomes reproducible, and the grain is measured rather than assumed (this round)

The R5 rebuild was built and merged with three things open. All three were taken to a number; two of
them ended somewhere other than where the gap text expected, and that is recorded as such.

**The proof set is reproducible now, and the recipe is the finding.** The stored PNGs could not be
reproduced from the committed source (99.88 % of pixels differed — tuning drift, re-measured on the
merge). The cause of the *irreproducibility* was never the clouds: the screenshot venue streams tiles
while it renders, so a short run frames a half-built quadtree. Holding the camera still for 180 frames
and writing only the last one puts the streamer at `pending=0`, and the converged tile set is a pure
function of the camera — **12 frames, two independent runs, byte-identical sha256, in both SVS and
EVS**. The consequence is that the fly-through had to become a LADDER through one column rather than a
flown mission: a moving camera never converges, so a flown sequence cannot be hashed. Named in Gaps.

**The march grain fell 13 %, not the 50 % the gap hoped for, and the honest reason is in the way.**
Removing the erosion term entirely dropped the grain from 0.0263 to 0.0149, so the erosion — 1.6 km
wide, 300 m tall, against a 260 m step — is what a 6–12 node march undersamples. Two changes went in:
a **composite trapezoid** in place of the jittered rectangle rule, and a **band-limit of the erosion
against the march's own step** (`ErodeFlat`, a new parameter of the SHARED density function, so
`--cloudcheck` still covers it — AGREE at 1.90·10⁻⁵). Grain 0.0329 → 0.0285 (3×3 high-pass), cost
+5 %. The bigger win is one the gap did not ask for: the rectangle rule rendered a thick deck **3.7 %
too dark**, and the trapezoid renders it to +0.8 % of the converged reference. Five alternatives were
built, measured and rejected — no jitter (−58 % grain, but 72 px contour bands), half-amplitude jitter,
a front-loaded geometric step ladder (+39 %, worse: it is right for a steep crossing and wrong for the
grazing far field), a stratified 4×4 ordered jitter (−1 %: stratification fixes the block mean, the
artefact is per-pixel), and an amplitude fade of the erosion, which measured BEST and was rejected
because the improvement was a 3.5 % brighter deck in the metric's denominator.

**One ceiling, three étages: a weight, not a choice.** The predecessor clamped the reported ceiling
into the band of the lowest broken deck and carried two discontinuities — the base stuck at the band
edge while the ceiling walked on (4.2 km of assertion the data contradicts, measured), and the choice
of deck flipped whenever a cover crossed 0.5. It is now an ownership weight with a 2 000 m handover:
étages hand the ceiling over continuously, a deck below a tenth of the sky cannot own one, and the
outermost edges saturate because there is nothing to hand to. The Payerne proof corridor is unchanged
to the metre. The price is named and measured: during a handover the deck slides at up to 325 m/km
against the data's own 135 m/km.

**And the question the owner actually asked — do you see them when you fly?** Yes, on this run.
Probing the committed fixture point by point: over the Swiss box (90 points) 60 % of points carry
≥ 25 % cover in some étage and a ceiling is reported at 71 % of them; Payerne itself is 75.7 % low
cloud with a 2 991 m ceiling. The distribution is strongly bimodal — that is GFS' own layer
diagnostics, not a defect here — and one run is one atmosphere, so a mission that needs a guaranteed
sky still has to say `wx fixture`.

One defect was found that predates the round and is NOT fixed: **the underside of an optically thick
deck receives no light**. The slant optical depth to the sun is ≈ 57, all three multi-scatter octaves
evaluate to zero, and the base ends up the ambient floor times the sky — dark blue where a real
overcast is bright grey. It is an ambient/multi-scatter model change, not a march change, so it is
named with its numbers instead of patched in the middle of a proof round; the cirrus frame shows the
same code producing Beer damping and a silver rim as soon as the deck is thin.

Gates: all targets clean, `verify-layers`/`verify-models` green, nine harnesses rc=0, `fb-gym` still
GPU-symbol-free, WASM builds, `--cloudcheck` AGREE, and 14 telemetry CSVs over six missions
byte-identical to `HEAD` — the clouds do not touch the physics, and that is now measured rather than
argued.

### 2026-07-28 — out of single fighters, an air force: the FLIGHT

A "flight" was an appearance. `fl` in the datalink's contact filter meant "the first unit of that
faction in mission order", two jets of one side flew two private wars, and nothing in the tree could
tell whether they had prosecuted the same target while a third went unengaged. This round makes it a
mechanism, in five pieces, and every one of them is a no-op without the declaration that turns it on.

**Roles are mission data.** `flight <name> <position>` in a `.fbm` unit block, beside `team` and for
the same reason: it is both mission data and world identity (`core/FBFlight.h`, `FBUnit`), so the
cooperative datalink reads it off the registry as it reads the team. Position 1 is the lead, a flight
without one is a parse error, and `fl` now selects it.

**The wingman holds a station on a moving point**, in two channels that never talk to each other:
across and vertical through the path law that already exists (`SetDirectLeg` onto the LEAD's course
line through the station), along through the throttle at `sqrt(2·a·|e|)` with the airframe's own
measured brake authority. That separation is the law — a `Direct` at the station is pursuit of a point
moving at combat speed, which is the regime that produced the merge roll problem. Measured
(`pair-formation.fbm`): **45.2 m** median station error on a straight leg, 1,937 m peak through a 90°
turn, no standing offset.

**The sort is three levels of information**, applied in order of worth: what a mate SAYS (a target
POINT, correlated against one's own echoes — never an identity, because this radar does not know whom
it sees), what the flight can WORK OUT (a greedy minimum-cost matching on time-to-a-shot, identical on
every member), and what was AGREED before takeoff (`set brief_sort`, the only sort an aircraft without
a cooperative terminal has). Measured: the cooperative pair holds different targets in **93 %** of the
ticks both were assigned, and `dup && free` — a target engaged twice while another was free — is
**0 over every unit of every formation mission**.

**Cover is one rule whose price is the weapon.** A member does not fire a round that would bind it
while a mate is already bound; a bound shooter flies at its own antenna and a flight with nobody free
cannot answer a launch. Measured: an AIM-120 binds **0.3 s**, an R-27R **17.3 s** — a factor of 58 —
and `pair-cover.fbm` measures **7.8 s** of real deferral with the flight never left uncovered. The
MiG cannot apply the rule at all, because "I am bound" has no channel on its aircraft; that is the
round's sharpest finding and it is the doctrine contrast arriving at its consequence.

**The asymmetry as one number** (`four-4v4-asym.fbm`, same run, same geometry): distinct targets per
engaged member, cooperative **0.962** against briefed contract **0.750**.

Five missions (`pair-formation`, `pair-2v2-f16`, `pair-2v2-asym`, `pair-cover`, `four-4v4-asym`), one
analysis tool (`fb_flight_report.py`), a `--flight N` mode for the tournament with `dl=`/`sort=` on a
variant line, 14 appended `flt_*` columns. **All 79 stock missions byte-identical** on every column
they ever had and line for line in `events.log`; one fingerprint per new mission over `--threads
1/2/4` × 3. Rejected with their measurements: symmetric yielding (a 1 Hz oscillator, 60 consecutive
swaps), age-compensating the contact range (switches 33 → 87), a blink hold (no effect), and capping
the along-track correction at one spread (a wingman stuck 40 km out for 230 s). Full contract,
derivations and gaps: [`formation.md`](formation.md).

### 2026-07-28 — the F-16's roll law gets an END: the merge becomes two-sided

The previous round moved the merge's blocker onto the F-16: with the MiG acquiring and flying aggressive
locked pursuit, the F-16 departed `duel-merge` at t=18.0 (`LOC`). **Diagnosis first.** The regime is not
"a reversal": it is a head-on pass at **898 kt of closure** whose line of sight sweeps at up to
**543 °/s** — 34 × the airframe's corner turn rate — against `gun-bfm`'s 17.7 °/s and `bfm-basic`'s
6.2 °/s. There the commanded lift direction rotates WITH the aircraft, the roll error never closes, and
the law rolled **290° in 3.1 s** on a steering error of 10–20°. Neither existing guard sees it: the
conversion guard has exactly this premise but tests the target's angular OFFSET (its zone floors at 90°,
the merge never exceeded 76.6°) and only freezes the turn SENSE, which permits an unbounded roll in that
sense; the rate cap did bind and held 103–109 °/s against its declared 90.

**The fix is the missing half of the same closed form, not a fourth hook.** §5.7.2 derived the cap as
"180° in `kBfmTurnTimeS`" and applied it as a PEAK — next to a judge whose rule is a SUSTAINED quantity
(|ω| > 60 °/s for 3 s), so 90 °/s is 1.5 × the threshold and survivable only while no geometry holds it.
The law always takes the short way, so no correction can require more than 180° of roll in one direction;
the same sentence therefore also bounds the roll flown per window. The cap is scaled by the share of a
reversal still open (`cap · (1 − |∫p dt over kBfmTurnTimeS| / 180)`), which is exact at an empty window
and has the fixed point `p = cap/2` — 45 °/s for the F-16 — because `cap · T = 180`. That margin against
the judge is thin on purpose: measured, it is what the softer variants do not have.

**Measured.** `duel-merge`: longest |ω| > 60 stretch **2.9 → 0.8 s**, roll per 2 s window
**195.8 → 110.8°**, run **18.0 → 232.3 s**, no F-16 departure; viper `lock_s` 16.1 → 28.2, fulcrum
14.2 → **79.4** and the campaign's first WVR employment (12 GSh-301 rounds, t=195.2, miss). 16-approach
sweep (now a committed tool, `sim/tools/fb_bfm_sweep.py`, instead of a scratch script every round has
to guess back): departures **6 → 0**, kills **5/16 → 7/16**, peak roll rate **182.2 → 103.4 °/s** (2.02 × → 1.15 ×
the cap), seconds above the cap **63.2 → 5.4**. Rejected with their numbers: the bang-bang form of the
same constraint (sweep departures 3, `gun-bfm` loses its kill) and giving the MiG back the derived 90 °/s
peak (restores `mig29-bfm`'s control position, costs a MiG departure at t=103.6).

**Re-baseline:** 79 missions, **73 byte-identical**, 6 moved, ONE verdict change — `gun-dry` 1 → 3, whose
twelve rounds all still arrive but into the nose zone at 25.7 kJ/m² instead of the centre at 153. Two
costs are declared rather than explained away: `bfm-blind` and `mig29-bfm` lose their control position
(`bfm_ctrl_s` 48.4/88.2 → 0.0), causally, because a sustained conversion roll is now half the peak. A
matching chaos measurement bounds how much of the sweep is signal at all: perturbing `gun-bfm`'s spawn in
0.8 m steps over ±3 m flips it between KILL (t=77.9…197.1) and no kill in 2 of 8 samples. Determinism
1/2/4 threads × 3 on all six moved missions plus `payerne-pair`/`-four`; nine harnesses, `verify-models`,
`verify-layers`, `nm`, native frame proof out of the merge, WASM rebuilt and hash-verified against the
baseline build. What the merge STILL does not test is the R-73/GSh-301 thesis, and the reason is now
honest: 190 of its 232 s have the F-16 blind, 143 the MiG, 133.6 both — after the first pass neither ACM
box re-acquires, both jets sink and the MiG loses that race. New gaps: `pilot.md` 2.8 (the lift-vector
law is singular for a downward demand above 1 g — the mechanism that TRIGGERED the roll) and 2.9
(close-combat re-acquisition + the BFM floor).

### 2026-07-28 — MiG-29 value round: the merge acquisition and the second dispenser

Two coupled gaps, both closed. **(A) The N019 acquires in a turning merge.** Its documented close-combat
modes (CC/VS/BORE) are azimuth PENCILS (±1.75°/±1.5°/±1.25° — the vertical reading DCS-FM p.12 forces),
and a pencil cannot hold a manoeuvring nose-on merge target: `duel-merge` fulcrum lock_s was **0**. A
BROAD forward auto-lock volume was added, `FBMig29Radar::kAcm*` — ±37° azimuth [T4 §7.1, read as azimuth
against the vertical reading CC takes; the two sources genuinely conflict], a [SET] ±30° nose-centred
vertical band (measured threshold: ±25° never firms, ±30° acquires, wider buys nothing), Doppler-EXEMPT
like CC (a co-speed merge is the low-closure case), frame **0.75 s** [DERIVED from T4's "1-2 s lock" over
the generic two-look firming, the same construction as the RAD 3.0 s frame]. The pilot SELECTS it in the
fight phase: a new generic hook `FBPilot::BfmRadarModeOrdinal` + `BfmSelectRadarMode`, the F-16 leaving it
−1 (byte-identical, its `acm_hud` is mission-set). Measured: `duel-merge` `n019 MODE acm` t=0.5,
`RADAR_LOCK` t=3.8 (3.32 nm), **lock_s 14.2**; `mig29-bfm` improves 203→296 lock_s / 5.3→88 ctrl_s.

**(B) The BVP-30-26 dispensers.** `modules/mig29/FBMig29Cmds`, a `sensors/FBCountermeasureSystem`
override: 60 cartridges [DOC], a [SET] 30/30 split (a named source gap), 5/5 BINGO and the three geometry
programmes on the generic slot machine ([SET] values, schema from the source — the F-16 ALE-47 pattern).
Wired in (cycled RWR→CMDS at 10 Hz, `CmDispense`/`CmConsent`/`CmdsMode` routed, `cmds_*`/`brief_flare_s`
keys, gated on the `Countermeasures` health id the damage layout already zoned). Its flares seduce the
AIM-9 through the SAME deterministic model that seduces the R-73 (`sensors/FBIrstSystem::SelectFlare`):
`mig29-defend.fbm` measures `FLARE_SEDUCED tgtIntensity=0.16` (head-on/dry) and the round expiring 16.0 m
wide, against an astern control detonating 0.04 m out. The defensive asymmetry (D5) is now two-sided.

**The finding the merge exposed.** With the MiG now flying aggressive LOCKED pursuit, the F-16's own
UNCAPPED BFM roll law (defaults 90 °/s, `BfmSearchRollCap` 1.0) goes into a full-deflection roll-reversal
PIO at the close-in high-closure reversal and DEPARTS at t=18 (`duel-merge` exit 3→2, result=LOC). It is
CAUSAL — with the MiG's ACM disabled the F-16 does not depart and the run is exit 3 again. Per the
campaign rule a loss to an AI defect is not a result, so this is NOT "the MiG wins the merge"; the
R-73/GSh-301 thesis stays untested and the merge blocker moved a third time (departure → acquisition →
the F-16's roll law). The F-16 cap is F-16-scoped and would touch its byte-identity, so it is deferred.

**Gates.** All **57 F-16-only** stock missions byte-identical (before/after `telemetry*.csv` SHA-256);
`test-corner` unchanged (380 / 16.18 / 5.44); MiG non-combat missions byte-identical; the BVR duels
(`duel-offset`/`duel-emcon`) move only in `cmd_*` bookkeeping (the intercept CmDispense is no longer
rejected `NotImplemented`, flight state and outcome identical). Determinism 1/2/4 threads ×3 = one
fingerprint on `mig29-defend`, `duel-merge`, `mig29-bfm`. `verify-models`, `verify-layers`, WASM and
native all green; proof frame `notReadyDraws=0 violation=0`.

### Foundation (24–25 Jul)

| Commit | Section |
|---|---|
| `59f08c8` | module architecture runtime-polymorphic, nine system slots with NoOp defaults |
| `c9206eb`…`2099cb0` | renderer stage split in four slices — at the end zero inline shaders in `FBRenderer.cpp` |
| `4cb92e8` | HUD stopgap → generic default HUD in the displays slot |
| `2f3c277`, `8997eec`, `6f160af` | HUD font: coverage AA instead of alpha test, split into generic font system / MAX7456 hook, 16×16 glyphs from B612 Mono, the same AA technique for all strokes |
| `6802a6d`, `d31b1a9` | F-16 main HUD with the real combiner aperture, legibility for 720p |

### Pilot AI and the control loop (26 Jul)

| Commit | Section |
|---|---|
| `681c5f8` | pilot-AI framework: `FBPilot`, units, airframe controls |
| `65d334c` | mission runner + telemetry — **the control loop itself**, the prerequisite for everything that follows |
| `e49d335` | phase 1: takeoff flies |
| `e4d7c26` | telemetry/log architecture: declarative sources, central bus, `FBLog` |
| `705c90a` | lib/client split: core lib, `fb-gym`, elevation hook, baked Swiss DEM |
| `28e74e5` | `FBFlightMonitor` — incorruptible physics K.O., model-derived |
| `92fe8a4` | mission orchestrator down to four steps, declarative spawn, `FBMissionMonitor` |
| `8cd3a74` | phase 3: landing — `payerne-full` flies fully autonomously |
| `bf4ee62` | **hardening**: silent wrong values, aborts, client divergence — see "Defect classes found" |

### Multi-unit (26 Jul)

| Stage | Commit | What it built |
|---|---|---|
| 1 | `c1bc9de` | FDM instanceable — `FBFdm` as an object, no global instance |
| 2 | `c08a168` | the actor is ONE object (`units/FBSimUnit`) |
| 3 | `2c03704` | the formation is mission data — two jets fly |
| 4 | `6d7ed5a` | thread per unit in the gym, lockstep barrier, bit-identical |
| 5 | `9190e7c` | datalink — units see each other through a system |
| 6 | `4049a7b` | FCR radar with ACM modes, anonymous contacts, IFF |
| 7 | `b375bef` | BFM manoeuvre AI — flies on radar contacts alone |
| 8 | `071ea2b` | avionics data model: output blocks with validity + command bus |

### Knowledge base (26 Jul)

`2dd1142`, `e22f228`, `c4e96e7` — the official ED documentation distilled into `doc/modules/f16/`.
`weapons.md` and `defence-rwr-cm.md` from SHALLOW to FULL; `controls-commands.md` new, as the template
for the command blocks.

### Weapons, damage, tactics (27 Jul)

| Commit | Section |
|---|---|
| `b62c769` | weapons foundation: the weapon is a unit of its own with its own FDM |
| `5c68fc5` | AIM-120 with seeker, guidance and datalink support |
| `439f53a` | RWR and countermeasures — who notices being seen |
| `1ecd433` | intercept AI: BVR tactics — guide, shoot, support, defend |
| `6d84647` | damage model: hits become system failures, failures become invalidity |
| `82df2e2` | combat objectives and evolutionary tournaments |
| `a1a8fbf` | M61A1 cannon: derived ballistics, EEGS funnel, kinetic damage |
| `1eeff72` | air-to-ground: ground targets without an FDM, CCIP/CCRP from one integration |

### Refinement of the AI (27 Jul, ongoing)

| Commit | Section |
|---|---|
| `cac7b62` | pilot memory: the datum instead of the last measurement point; gun tracking with a rate term; roll-rate controller |
| `9673e00` | guidance holds a track where a track is declared — cross-track error and waypoint capture |
| *(this round)* | the close-combat law survives a raw airframe — the MiG-29 flies BFM without departing (`duels.md` D1) |

### The MiG's close-combat law (D1) — surviving a deckless airframe

**What it built.** The BFM control law is written for the F-16, whose JSBSim deck holds α and roll rate
under any stick. The MiG-29's deck has no FLCS, so the law departed it in 22.8 s from a merge. Four
measured, airframe-scoped screws close it, F-16 byte-identical: (1) the Manual-path pitch-deflection cap
`PitchStickMax` and (2) an α limiter allowed to push to recover (both in `systems/FBFlightControl`), plus
the pilot hooks (3) `BfmSearchRollCap` (0.20 — the search is a scan, not a combat roll) and (4)
`BfmRollRateMaxDegS` (60 vs the F-16's 90 — the twitchy K=201 roll overshoots the 10 Hz cap into a PIO).
Each was diagnosed by telemetry before it was turned, each exposed the next (α tumble → mush → search
limit cycle → pursuit PIO). **What it measured.** `mig29-bfm.fbm` (new): full BFM run, no KO, α ≤ 27°,
acquires and locks a trail defender (lock_s 203 / ctrl_s 5.3), deterministic over threads 1/2/4 × 3.
`duel-merge`: exit 2 → 3, the MiG survives (aoaMax 24.6°); the F-16 dominates the angles (lock_s 298 /
ctrl_s 78) but neither converts — a draw, the remaining blocker being gap 4h's ACQUISITION half (RAD
cannot hold the target in a turning merge), not the flying. 13 F-16 BFM/gun/BVR/attack missions
byte-identical; every other MiG mission's exit code unchanged (`mig29-full` touchdown 143.4 → 143.7 kt).
`doc/pilot.md` §5.10, `doc/duels.md` D1, `doc/modules/mig29/module.md` gap 4h.

### One model root and the delta rule (27 Jul)

**What it built.** All flown JSBSim models now live under `sim/assets/aircraft` — `f16` (incl.
`Systems/` and the two referenced engine XMLs, which moved into the model directory as `f16/engine/`:
JSBSim's own per-aircraft layout, which its loaders search first), `mk82`, and the `aim120` that was
already there. `FBModelRoots` has ONE root, `FBModule::FdmModelVendored()` and `FBStoreSpec::Vendored`
have been dropped without replacement, `FBFdm`'s engine/Systems probing (`stat` + parent truncation)
has become two unconditional paths, and the WASM build embeds one root instead of five individual
paths.

**Why.** Principle 1 has moved from "never patched" to the **delta rule**: the pinned submodule is the
base, the copy flies, and every deviation is a named, evidenced entry in `sim/assets/MODEL-DELTAS.md` —
a better mission result is explicitly not evidence. The gate is `make -C sim verify-models`
(`sim/tools/verify_models.py`): canonical unified diff per file, character by character against the
diff block of the entry. Deliberately no `patch`/`git apply` — an application with fuzz could swallow a
deviation.

**Measured.**

| Check | Result |
|---|---|
| Regression, 50 missions | **121/121 telemetry files byte-identical**, all 50 exit codes equal, `events.log` identical except for output path and wall clock |
| `verify-models` | green (4 upstream-covered paths, 0 deltas, 1 FlightBox-own model) |
| Negative test | one changed byte in `f16.xml` → rc=1 with the missing block; likewise a declared but absent delta, a non-matching diff, and an undeclared model directory |
| Harnesses | all seven rc=0; corner speed unchanged at 380 KCAS / 16.2214 °/s |
| Determinism | 5 missions × `--threads 1/2/4` × 2 repetitions = one signature each |
| WASM | builds; JSBSim loads `f16` from the embedded `/fb/aircraft` and trims (`trimConverged=1`) |
| Frame | `gpu_native --mission payerne-takeoff --interval 20` → 28 PNGs, terrain + HUD |

## Defect classes found

What the control loop brought to light that an inspection would not have found. The list is both a
warning and a test pattern.

| Class | Concrete case |
|---|---|
| **Silent wrong values** | `ApplySetup` returned 0.0 for unparsable text and reported success. An HTML error page from the `/elev` endpoint was cached as a sea-level elevation — a whole 216 s mission flew over sea level and reported SUCCESS. |
| **Missing divergence check** | 16 injected NaN cases all ran through with `tripped=0`. |
| **Unguarded calls** | unchecked JSBSim calls → `std::terminate`, exit 134. |
| **Missing header dependencies** | The Makefile had no `-MMD -MP`: stale objects, phantom measurements. Proven by a deliberate header change that altered a telemetry hash and was then reverted. |
| **Architecture leak** | `FBFlightMonitor` knew about runways. Physics K.O. and mission verdict were separated. |
| **Module specifics in generic code** | F-16 references in `FBFlightMonitor`; limits are now derived entirely from the model. |
| **Non-determinism through ordering** | Log line position depended on the scheduler. Solved via merge order instead of locks. |
| **Two copies of the same data** | `sim/web/missions/*.fbm` was a hand-kept copy in the old format — the WASM app stayed black. Now a build copy. |
| **Aliasing through tick rates** | The seeker looked at 20 Hz at poses published at 10 Hz: 446 m/s measured instead of 654 m/s. Solved via a dwell window instead of two single measurements. |
| **Zombie state** | A detonated missile kept radiating for 74 s after its detonation. `Retire()` now clears the signature. |
| **Wrong controlled variable** | A pure P controller against a ramp (the gun solution against a turning opponent) parks at ramp rate × time constant. A point controller against a track has a steady-state cross-track offset. Both are a matter of controller type, not tuning. |
| **Stale documentation in a data file** | Two mission headers still documented "ends in a timeout" after both runs had become kills. The header carries the reading rule and must be maintained with it. |

### Documentation: the spec-driven restructuring (27.07.)

**What it built.** `doc/` moved from "one file per subsystem plus a central TODO" to a
spec-driven shape: every topic file now carries `## Spec` / `## State` / `## Gaps` / `## Knowledge`,
grouped into `sim/`, `aircraft/`, `render/`, `clients/`. New: `vision.md` (the direction),
`roadmap.md` (R1–R10, thin, pointing at the Spec each stage must satisfy), `aircraft/mig29.md` and
`render/units-visual.md` (both spec-only, nothing built), `aircraft/stores.md`,
`clients/clients.md`. `PROGRESS.md` became this file; `TODO.md` dissolved into the Gaps sections of
the files it belonged to, plus `roadmap.md`/`vision.md`. `render/rendering.md` split into
`renderer.md` + `hud.md` + `clouds.md` (whose Spec is the owner-approved rebuild, including the
cirrus layer) + `units-visual.md`.

**Rule change.** The maintenance obligation is now spec-first: change the Spec, build until State
meets it, then update State/Gaps and add a line here (`conventions.md`). There is no second list of
open work any more.

**Not done at the time.** Existing bodies stayed German (each file said so); the translation wave plus
the schema alignment of `doc/modules/f16/` and `doc/modules/mig29/` was roadmap R10. `world-and-terrain.md` stayed at its
old path until the `/wx` round lands, then splits into `world/terrain.md` + `world/weather.md`.

### 2026-07-27 — /wx: worldwide weather on the tile server (`24ac1fc`)

New `/wx` endpoint on fb-tiles: NOAA GFS 0.25°, decoded by an own 330-line GRIB2 reader (wgrib2 is
not packaged in Debian trixie; ecCodes serves as the test oracle — max error 0.5 quantisation steps
over all 20 fields × 259,920 points), delivered as ONE packed 8.3 MB blob per run ("one run is one
atmosphere" — split blobs could straddle a cycle boundary). Byte-identical deterministic builds
across two compilers; the fixture in `tiles/testdata/` doubles as the gym dataset. Poisoned-cache
lesson applied: NOMADS failure writes nothing, ever.

### 2026-07-27 — weather in the simulation (`43b82b5`)

`core/FBWeatherProvider` (calm / constant-wind instrument / FBWX blob from file or memory),
`FBFdm::SetWindNedMs` → `FGWinds` (derivation in the header; only the owner writes, only on change),
`wx` mission declaration (mission always wins; defaults gym/native calm, **browser live**).
Measured: crosswind drift 3.3078° vs 3.2765° derived (0.95 %); uncorrected CCRP in 25 kt crosswind
shifts 12.8 m — far below wind×TOF (127 m) because a 227 kg bomb barely couples laterally in a 10 s
fall; GFS fixture wind recovered from the flown trajectory to 0.12 m/s. All 50 pre-existing missions
byte-identical. Found and open: guidance cannot close a steerpoint inside its drift-widened turning
circle at 18 m/s crosswind (permanent 59° orbit); the 10 m wind anchors at 10 m ASL, not AGL.

### 2026-07-27 — R10: English throughout, schema everywhere (this commit)

The four-part wave: (a) the seven big `sim/` bodies translated (~8,300 lines, zero content loss,
anchors fixed); (b) the rest of `doc/` plus legacy markers on the twelve old cloud
studies; (c) `doc/modules/f16/` on the Spec/State/Gaps/Knowledge schema — producing the first **coverage
map** FlightBox-vs-real-jet (near-full: command bus, HUD symbology, RWR/CMDS; nothing: startup,
displays, HOTAS, refueling; and the surfaced fact that the model flies an F100-PW-229 while the
doc describes the F110); (d) `doc/modules/mig29/` on the schema plus the citation reconciliation — where
the task premise ("uniformly PDF pages") proved wrong: the files were internally mixed, so all 131
DCS-FM citations were scored individually against the extracted PDF text (88 converted, 43 already
printed, re-grep proof 125 printed / 0 PDF). Provenance tags (`[MESS]`/`[ABL]`/`[MODELL]`) stay
German deliberately — they appear identically in code and three doc trees; renaming is only sane as
a coordinated sweep. Remaining German: `world-and-terrain.md` (splits into `world/` in phase 3 of
the mirror refactor) and the pre-refactor `sim/src` paths inside the seven translated files (also
phase 3).

### 2026-07-28 — the first model delta, and the landing that follows from it (this round)

**D1 — the flaperon mixer** (`sim/assets/MODEL-DELTAS.md`, the delta rule's first live entry, and its
first practical test: the emitted block collided with the verifier's own HTML-comment stripping, so
`tools/verify_models.py` now protects the inside of a ```diff fence — otherwise a delta that touches an
XML comment would be undeclarable). `f16.xml`'s flaperon summer carried the flap command
DIFFERENTIALLY and the roll command SYMMETRICALLY, so `fcs/tef-control` cancelled out of
`fcs/flaperon-mix-rad` and twice the aileron command took its place. The correct mixing is derived from
the model's own consumer structure — the mixer's only two consumers, `CLDflaps` and `CDDflaps`, are
symmetric per-radian force coefficients, while the rolling moment travels through `fcs/aileron-pos-rad`
— and the evidence is a physical impossibility the model produced: **+6,420 lbf of forward "drag"** on
a right roll at 350 KCAS. Measured before → after: `flaperon-mix-rad` under a pure roll step
−1.28 → **0.0000**; Nz peak in the roll-in −1.54 g (right) / +3.46 g (left) → **+0.97 / +0.97**; flaps
fully out 0.0002 → **0.349 rad** = the 20° the Flaps channel commands, ΔCL **0.122**, ΔCD **0.028**;
roll rate at 400 KCAS +187.8/−132.3 → **+156.4/−156.6 °/s**, direction asymmetry across 250–600 KCAS
from **55.5 → ≤ 0.2 °/s**.

**Hook cascade, each one re-measured rather than assumed:** corner SPEED unchanged at 380 KCAS, the g
at it 5.6 → **5.4** (`BfmCornerG`), best rate 16.22 → 16.37 °/s (peak moves to 400); 11°-AoA trim speed
165 → **154 KCAS** (`ApproachSpeedKt`). Unmoved and reported as such: `BfmBrakeMs2` (2.531 → 2.527 m/s²
— the flaps only deploy below 250 KCAS, that hook is measured at 325–400) and the ~0.2° cruise
asymmetry (median |φ| on settled route legs 0.186° → 0.185° over 60,900 samples — it is the roll PID's
steady-state residue, not the mixer's; the hypothesis that D1 would fix it is **falsified**).

**The long landing roll.** The deceleration budget named the cause and it was not the model's µ:
JSBSim brakes on `static_friction` (0.8, upper end of dry-runway values), and the measured brake
deceleration is 3.3–3.8 m/s², working correctly. The loss sat between the two-point attitude and the
brake gate. In the aerobrake the wings carry the whole aircraft (wheel normal load **0 lbf** at 12°),
so no brake can bite and the 5,295 lbf of aero drag is the entire budget; the moment the nose falls,
drag collapses to 1,477 lbf. The pilot gated the brakes on `AerobrakeSpeedKt` (100 kt) while the
elevator actually loses the attitude at ~106 KCAS — a **361 m / 6.7 s coast at 0.45 m/s²** in between.
The gate now hangs on the fact instead of the speed: `FBAirframeControls::GetNoseWheelOnGround()`
(the forwardmost bogey's WOW, selected by geometry, `FBFdm::GetNoseGearOnGround`), latched for the
roll-out, exactly as `procedures-landing.md` sequences it. Landing roll at Payerne RWY23:
**1,597 → 785 m** (`payerne-landing`, −51 %) and **1,341 → 928 m** (`payerne-full`, −31 %). Attributed:
D1 plus the new approach speed does 1,597 → 1,039 m and 1,341 → 909 m (the flaps finally give the
two-point attitude real drag), the gate does 1,039 → 785 m on `payerne-landing` and is NEUTRAL on
`payerne-full` (909 → 928 m) — there the nose happens to fall at 99.6 KCAS, so old gate and new gate
fire at the same instant. That neutrality is the point: the gate does not brake EARLIER, it brakes when
the aerobrake is over, whenever that is.

**The approach speed is the honest one, and it costs distance.** With the pre-D1 165 kt the same build
rolls 642 m / 578 m and greases the touchdown (126.7 kt, 0.29 m/s sink) — but it flies final at 9.2° AoA
and floats 38 kt before touching. At the measured 154 kt it flies final at 11.0° AoA and touches at
12.8° AoA, both exactly as `procedures-landing.md` prescribes, at 142.9 kt and 2.96 m/s of sink (peak
gear load 2.05 W against the monitor's 3.0 knockout). The extra 143 m is the price of a procedurally
correct approach instead of a float. What this exposed and did NOT fix: the flare law targets a pitch
ATTITUDE 1.7° above the approach attitude and therefore barely arrests the sink — it had been masked by
11 kt of excess approach speed for as long as the flaps did not work.

**Re-baseline:** 53 missions, 48 verdicts unchanged, five changed and all five explained rather than
papered over — `attack-ccip`/`attack-ccrp`/`wx-ccrp-wind` (the release vertical velocity flips sign
because a roll-in no longer produces a lift step, and the FCC's own table-vs-aero prediction error of
53–64 m stopped cancelling the aim error instead of adding to it: aim error 28 → 80 m), `gun-bfm` and
`bvr-duel-decided` (the BFM/launch geometry rides on the roll behaviour that changed). No mission file
was edited to make any of them green. Determinism 1/2/4 threads identical on five multi-unit missions;
eight harnesses, `verify-models` (green WITH exactly one declared delta, and its negative directions
re-checked), `verify-layers`, WASM + smoke (the corrected gain is in `gpu.wasm`, the old one is not)
all pass.

### 2026-07-28 — the re-tune against the corrected physics (this round)

D1 left five missions on TIMEOUT with a suspended reading rule. All five are back — and none of them by
a number chosen to make them green: each of the three faults it exposed was a real defect that the old,
broken roll authority had been paying for.

**`gun-bfm` — the closure schedule was capped on the wrong measurement.** Attribution first, by running
the CURRENT code against the PRE-D1 model in a scratch tree: over a 16-approach sweep (8 geometries ×
straight/turning defender) the pre-D1 model scores **4/8 straight + 8/8 turning**, post-D1 **0/8 + 8/8**
— the whole regression sits against the STRAIGHT defender, and it is one event: the first stern
conversion now tips the other way and becomes a fly-through that costs the ACM box its contact. Under it
sat the real fault. `BfmBrakeMs2` bounds the closure schedule's cap `a/k`, but it had been measured as
the airframe's LEVEL-FLIGHT deceleration (2.4 m/s², 238 samples) — a different quantity, because a
closure carries the pursuit geometry as well as the drag. Measured on the thing itself (one-second
windows in the conversion, idle + full speedbrake + valid track, N=4,595): **median 1.86, p20 1.16, p90
5.76 m/s²**. A braking LIMIT takes the pessimistic end of its own distribution, so the hook is 1.2 and
the cap 140 → 70 kt. `gun-bfm`: the pursuer used to arrive at 0.5 nm with 105–120 kt against a schedule
asking for 27 and fly through at 0.11 nm; it now tracks at t=59.5 and KILLS at t=66.7 on 70 rounds.
Sweep after: **3/8 + 8/8 = 11/16** against 12/16 pre-D1 and 8/16 post-D1, with mean tracking error
41.1° → 25.5° (straight) and 7.2° → 4.6° (turning). The last kill does not come back and it is named as
such, not papered over.

**`bvr-duel-decided` — the round, not the shot.** The launch geometry is unchanged (24.8 km, the same
beaming defender to within 2° of heading and 30 m of altitude); shooting closer was measured and does
nothing (`pilot_shot_rtr` 1.0 → 0.5 gives 6.25 / 5.35 / 8.11 / 7.52 / 7.20 / 4.03 m — noise, no trend).
What D1 exposed is an instability in the AIM-120's terminal acceleration loop: past ~10 g of demand the
fins ran onto their stops, the integrator wound into a reversal, and the round's own alpha rang (mean
tick-to-tick |Δα| in the terminal phase 0.70°). Two structural fixes, no damage-model change:
**conditional integration** (a fin on its stop cannot answer more integral) and **`kLoopI` 2.0 → 1.5**,
the largest gain on the stable side of the measured boundary (|Δα| 0.698 at 1.75 → 0.139 at 1.50 — an
edge, not a trend). Miss 6.25/7.09 → **2.36 m, one shot, exit 0**. Everything else the round flies got
better with it: `intercept-lostlock` 4.12 → 0.755 m, `damage-amraam` 1.90 → 1.49 m, `cm-beam-only` from
no detonation at all to a 7.83 m hit — which is what its own 2×2 table claims (beam alone leaves the
seeker nothing to be confused by; only chaff AND beam still defeat the shot, and that leg still does).
Two verdicts follow: `cm-beam-only` 0 → 1 and `intercept-lostlock` 0 → 1, both explained in their heads.

**The three attacks — two errors that used to cancel, now separated.** D1's report said the fire
control's own table-vs-aero error (53–64 m) had stopped cancelling the aim error. Measured, the aim error
had a cause and it was not the computer: the pilot set `AtkReleased_` when the pickle was POSTED, so the
escape turn began during the actuation latency and the store left the rail at **32° of bank** and
−0.6 m/s. He now flies the run-in until his own SMS counter says the store has LEFT (roll −0.16°,
vertical +0.01 m/s at separation), and he leads the cue by his own DECISION TICK as well as the bus
latency — between reading a number and pressing lies one slot, worth 21 m at 211 m/s. Result, per
`stores DELIVERY`: `predErrM` 63.8 → **43.6 m** (inside the ~45 m the target requires, and NOT corrected
— it is the declared property), `aimLongM` 78.8 → **40.9 m**, i.e. the release-moment error is now ~0 and
what remains IS the computer's error. `attack-ccip`/`attack-ccrp`/`wx-ccrp-wind` exit 0.

**Rejected, with their measurements** (now in `pilot-ai.md`'s Gaps): integral action on the BFM throttle
— the textbook fix for a P-only loop, and it turns the straight-defender sweep 0/8 → 8/8 while turning
the other one 8/8 → 0/8, because exact speed matching leaves the pursuer at the defender's own 248 KCAS
and his rounds miss by 7–8 m instead of 1.6–4 (the miss is ½·V·ω·TOF², so it is the shooter's speed);
and a turn-rate speed floor meant to replace that accidental energy bias, which does not bind (the
"max-rate" defender actually turns at 5.4 °/s) and binds everywhere the moment the aim error is added.

**Regression, all 53:** 7 verdicts changed — the five targets plus the two missile neighbours above; the
other 46 keep theirs. 27 missions differ byte-wise, in exactly three families: the BFM-phase ones (the
closure cap), the attack/store ones (the release timing) and every AIM-120 one (the terminal loop).
Determinism: 9 runs each (1/2/4 threads × 3) on the five targets → one fingerprint each. Eight harnesses,
`verify-models` (green, still exactly one declared delta), `verify-layers`, `nm` (0 GPU symbols in
`fb-gym`), native + WASM green, and the WASM A/B is decisive: `gpu.wasm` built from this tree carries one
more `1.2` double than one built with the old hook, and the two binaries differ. Proof frame:
`gpu_native --mission attack-ccip.fbm --interval 20` → SUCCESS, bunker DESTROYED, eight PNGs.

**Left stale on purpose:** `doc/weapons.md` §10.2's gain table still prints
`kLoopI = 2.0` and does not mention conditional integration — that file was outside this round's write
permission.

### 2026-07-28 — R5 clouds merged (`9ca2c0e`), MiG-29 stage 1 merged (`b411b2b`)

**Clouds:** the rebuild per the approved Spec — ONE bounded-volumetric stage, one separable density
function evaluated in C++ AND WGSL (constants printed from the C++ side, max |Δ| 1.87e-5 over 12,288
samples), the six FBCloud* stages and the tonemap's second pipeline deleted. Cost measured worst-case:
8.8 ms full-res vs ~23 ms of the old quarter-res+temporal chain — 2.6× cheaper at 4× the marched
pixels. Weather-driven via FBWorld::Weather() (no weather ⇒ no cloud pass, 6/7 passes). Five proof
sets incl. seamless fly-through and cirrus fibres along the real 250 hPa wind (3.8° residual).
Merged on its own branch against `ab40bac` by a dedicated agent (deletions win over namespace edits;
`--wx` is the screenshot venue's weather, mission venue reads the .fbm — combining both is now an
argv error). Known gaps: stored proof PNGs are stale vs the committed source (predecessor tuning
drift — re-capture wanted), march grain 0.04–0.08 by design, one ceiling clamped into three decks.

**MiG-29 stage 1:** the model exists — `sim/assets/aircraft/mig29/` (FlightBox-own, GPL-2.0-or-later,
every table tagged INV/GEO/ANALOGY/SET) plus `make test-mig29` measuring 23 anchors. 10 hit or in
band (Vmax SL +0.2 %, rotation/liftoff/ROC in band), 4 missed with diagnosis instead of anchor-fitting:
Ps SL −24.8 % is the borrowed thrust analogy (needs aug factor 1.16, F100 surface gives 1.02 — NOT
drag-closable without destroying Vmax SL), ceiling +8.7 % same family, takeoff run +29.8 % is the
spec's own §12.3 doubt. Roll rate 241 °/s declared a model property — no anchor exists at any tier.
Two JSBSim findings for the house: FGTrim drives `pitch-trim-cmd-norm` (a pitch channel without that
summer cannot trim at all), and linear table interpolation overstates a quadratic drag rise ~4.5× at
the first breakpoint. F-16 untouched (corner numbers byte-identical).

### 2026-07-28 — Phase 3 of the mirror rebuild: `doc/` becomes `sim/src/`

The documentation is now a **1:1 mirror of the source tree**. `doc/flightbox/` is gone; the seven meta
files sit at the root beside `core.md` / `fdm.md` / `systems.md` / `sensors.md` / `weapons.md` /
`pilot.md`, and the four subdirectories `missions/`, `modules/`, `render/`, `world/`, `clients/` carry
the same names as their source directories. Every move was a `git mv`, so the history follows.

**The two splits, both with translation** (the last German prose in `doc/`):

- `mission-format.md` → `missions/` — nine files (`INDEX`, `syntax`, `verdict`, `sensors`, `avionics`,
  `weapons`, `combat`, `weather`, `output`) plus `units-and-missions.md` → `missions/runtime.md`. Each
  new file carries the Spec/State/Gaps/Knowledge frame; the leading rules (exit codes, "a mission
  file's header comment is a binding reading rule") live in `missions/INDEX.md`.
- `world-and-terrain.md` → `world/terrain.md` (§1–§8) + `world/weather.md` (§9). The two points the
  roadmap had parked for exactly this split — DEM cache per worker instance, imagery mode not
  declarable in `.fbm` / TLS not wired — got their home in `world/terrain.md`'s Gaps. **The Parked
  table is now empty.**

The two reference bases moved under their module (`doc/f16/` → `modules/f16/`, `doc/mig29/` →
`modules/mig29/`), each now sitting beside the `module.md` that implements it; the cloud studies became
`render/clouds-legacy/`. **One skill instead of three:** `f16-systems` and `mig29-systems` are deleted,
their routing tables absorbed into `.claude/skills/flightbox/SKILL.md` as "The module reference bases".

**Path sweep:** 209 relative links inside `doc/` re-resolved against their new locations, plus 475
plain-text mentions across `doc/`, `CLAUDE.md`, the `.fbm` headers, `sim/tools/`, `sim/assets/`,
`tiles/` and ~150 comment banners in `sim/src/**`. Comment banners only — the three CLI usage strings
that name the format were deliberately left, because touching a string literal would move the
`strip_comments` hash. That hash is unchanged (`8d85837e…`, 233 files), the link check over every `.md`
is clean, `core-lib`/`gym` build, and three mission samples run byte-identically.

### 2026-07-28 — two value gaps: the wind orbit and the roll-limiter fixed point (this round)

**A — a steerpoint the guidance cannot close** (`doc/systems.md` §7.5.1). A capture circle is a GROUND
test of fixed radius; the circle the aircraft can fly lives in the AIR MASS, and a fix WITHOUT a leg is
flown by the bearing law, which controls the nose and not the ground track. New instrument
`missions/wx-orbit.fbm` (the GFS fixture's 9,000 m wind as the closed form `wx wind 338 39`): closest
approach **614 m** against a 500 m circle, then a permanent limit cycle — 1,793…4,851 m, −59.1° bank,
99.2 s per lap; the same file in calm captures the same fix with **4 m** to spare. Answer: a THIRD
sequencing ground, `orbited` — two failed approaches (closest approach, opened by more than the capture
radius, closed again, opened again) — bound to the SUCCESSOR as `passed` is bound to the predecessor, so
the deliberate terminal orbits of `bfm-basic`/`gun-turning`/`bvr-duel` are out of scope by construction
(re-measured: bandit `activeWp` 0 for the whole run, zero `WP_REACHED`). Threshold 2 is measured, not
chosen: at 1 the attack missions sequence their target fix out of the egress at t=87.9 s. Both
authorities state it independently, both fired at t=311.6 s; `wx-orbit` SUCCESS at t=485.4 s. All 53
pre-existing missions byte-identical.

**B — the roll limiter had no fixed point** (`doc/pilot.md` §5.7). `cmd_prev·cap/rate` is not a limiter:
linearised against the identified plant it is `z² − 2az + a = 0`, i.e. an oscillator with **|z| = √a**,
and it held **1.52 ×** its own declared cap over the 16-approach sweep (pooled autocorrelation of the
rate while active: first recurrence 0.70 s). Replaced by a memoryless ONE-STEP PLANT INVERSION off an
ARX(1) identification (15,325 samples below the cap, open loop: a = 0.734 / τ = 0.323 s, K = 78.7
°/s per stick) — 1.23 × at the same cap, and stretches ≥ 4 s above 0.8 × cap 11 → 0. The cap itself
became a closed form: the largest error this law can command is 180°, flown in the time constant the
roll serves → **90 °/s**, with `kBfmReverseS` falling out identically `kBfmTurnTimeS`. Re-measured over
six cap values × 16 approaches, 90 is also the measured optimum (12/16 against 8/16, and the only value
with no departure in the eight committed BFM missions); the control with the limiter removed scores
7/16 at 132 °/s peak. New instrument `missions/bfm-pointblank.fbm` (0.8 nm head-on, the swinging
stimulus): 1.37 × → **0.89 ×** the cap, 9.2 s → **0.0 s** above it. Costs, all declared in their
headers: `gun-dry` 3 → 1 (all twelve rounds now arrive), `gun-bfm` kill 66.7 → 84.2 s, `bfm-blind`'s
blind interval 41 → 199 s (chaotic across every cap tested), one departure in a non-committed sweep
geometry. Exactly five missions move, all BFM; nothing else in the tree changes by a byte.

### 2026-07-28 — MiG-29 stage 2a+3: the module flies end-to-end (merge of `b3da424`)

`sim/src/modules/mig29/` (module, pilot numbers, damage zones, registry name `mig29`) plus four
missions; `mig29-full` flies takeoff, route and landing autonomously to a stop on the Payerne
threshold (exit 0, 730.6 s; rotation 130.1 kt, touchdown 143.4 kt at 11.66° AoA and 3.59 m/s).
`mig29-pair` proves two DIFFERENT modules in one formation. The FBW preset is its own for a
structural reason: behind the g output the F-16 has an FLCS, here the output IS the deflection.
Three measured failures stand in the preset comment and determine it (saturating yaw → LOC t=28 s;
double-integrator limit cycle, 20 s period; no α limiter → α 90°, LOC t=122 s). The SOS limiter is
thereby built where `flight-model-spec.md` §7.3 placed it, behind one preset number. `test-mig29`
gained the two measurements the module cites: 136.8 kt at the documented 11° touchdown α, and corner
420 kt / 24.18 °/s / 7.83 g. F-16 byte-identical across all 53 stock missions.


### 2026-07-28 — MiG-29 stage 4: the asymmetric duel as a measurement campaign (this round)

**What the round was for.** Everything since stage 1 existed so that two DIFFERENT aircraft could meet.
[`pilot.md`](pilot.md) gap 2.3 had recorded for three rounds that the symmetric F-16 duel is a
stalemate by construction, and `modules/mig29/module.md` had said in as many words that the MiG exists
to turn the coin toss into a choice. This round is the measurement that says whether it did.

**What it built.** Eight missions (`sim/missions/duel-*.fbm`), an analysis tool
(`sim/tools/fb_duel_report.py`), a `module=` key on the tournament so a variant file can pit an F-16
doctrine against a MiG doctrine, and a new topic file [`duels.md`](duels.md) — which is a family of
MISSIONS rather than a directory of source, and the first entry in `INDEX.md` that is not a mirror of
`sim/src/`.

**The answer, and it was not the expected one.** Neither side structurally dominates; the launch
DOCTRINE does. With both pilots on the shipped rule (shoot at Rtr) five of five BVR geometries draw —
head-on, 50° offset, 6,000 m to either side, EMCON — because the two Rtrs sit within half a mile of
each other (AIM-120 9.78 nm, R-27R 10.25) and every round then arrives outside its warhead's lethal
radius. Change the rule on one side and the same geometry decides, and what each side needs is
different: **the MiG needs only the early launch** (`duel-doctrine-mig`, exit 0 — R-27R away at
14.41 nm, 25.8 s of unbroken illumination, 9.35 m detonation, the F-16 defensive 1.5 s before its own
trigger and never firing), **the F-16 needs the early launch AND 6,000 m** (`duel-doctrine-f16`,
exit 0 — its early launch alone is 10.7 s ahead and still draws at 4.79 m; from 6,000 m higher the
identical decision arrives 1.77 m out and kills).

**Three AI defects, all found by measuring, all fixed.** The GCI entry chain advanced on the POST
rather than on the acknowledgement, so the one entry that makes the N019 exist could be lost to a
single g-loaded tick (measured: 400 s of a duel flown blind). The intercept antenna was centred on a
COASTED look while the jet's own attitude moved, freezing a ±6° bar after one look through a 6,000 m
descent. And `FBMig29Pilot::InterceptSpeedKt` was a unit error — a CAS derivation fed to a TAS command
— that had the MiG cruising to every BVR merge at 217 KCAS / M 0.54, 40 % below its own departure
speed. **That last one is the round's second finding:** with it in place the F-16 won four of the five
BVR geometries outright. Correcting it turned all four into draws, i.e. most of the F-16's apparent
BVR dominance was a MiG tuning error rather than a weapon-system difference.

**Measured.** 66 of 69 stock missions byte-identical, all 69 exit codes unchanged; the three that
moved are `bvr-duel` and `bvr-duel-decided` (one to two extra antenna slews — `cmd_*` counters, plus
2.9 s in which one jet's RWR carries an extra SEARCH-class contact behind it that nothing acts on) and
`mig29-intercept` (same exit code and verdict, everything earlier and tighter: kill 87.7 → 78.1 s,
miss 1.13 → 0.34 m). No flight-state column and no verdict moved anywhere. All eight duels one fingerprint over
`--threads 1/2/4` × 3. The mixed tournament decides 12 of 30 runs where the symmetric one decides
**0 of 30**, and the early launch is worth an entire outcome band on the MiG (−393.7 → +585.0) against
nothing on the F-16 (601.8 → 603.3) — the same asymmetry the named missions found, reproduced by a
fitness written before the campaign existed. Open, and now with numbers: the MiG's close-combat law
DEPARTS the airframe in 22.8 s from a nose-on merge (`duel-merge`, kept as a reproducer), and an
AIM-120's terminal miss runs 1.37 → 7.66 m as closure runs 744 → 1053 m/s, which against a 1/r² damage
model is the difference between a kill and a jet that flies on.


### 2026-07-28 — MiG-29 stage 2c: the weapons and the signature

**What the round was for.** The MiG-29 had sensors and no weapons; the F-16 had no infrared round at
all; flares had been dispensed and counted since the countermeasure round with nothing to work on; and
`RADAR_DESIGNATE` was unreachable because the intercept pilot correctly disengages from a target it
cannot shoot. All four are the same missing piece, and it is the SEEKER.

**The one architectural idea.** A guided round is still ONE module and N catalogue entries; what makes
an AIM-120, an AIM-9 and an R-27R three different weapons is `FBSeekerKind`, and each kind names a
derivation of a SENSOR SLOT THAT ALREADY EXISTS. The infrared seeker is an `sensors/FBIrstSystem`, so
it inherits the aspect law, the afterburner term, the cloud deck and the anonymity, and the perception
boundary does not grow by a file (`verify_layers`'s `RESTRICTED` list is unchanged — the scan lives in
the base). The semi-active seeker is an `sensors/FBRadarSystem` that never transmits. Two seeker kinds,
no new architecture, and the tactical differences fall out of the sensors' own limits.

**The measurements that decided things.**

- **Flares now work, deterministically.** One inequality between two received irradiances in one unit
  (a clean airframe seen dead astern = 1.0), so the ASPECT does the whole job: head-on and dry an
  aircraft radiates 0.16 and a cartridge beats it six times over; astern in afterburner it radiates
  2.25 and cannot be deceived. Both branches measured on BOTH airframes at exactly `tgtIntensity=0.16`
  — the same number from the same code — and the decoyed rounds miss by 22.8 m (R-73, 3.5 m fuze) and
  25.96 m (AIM-9, 6.0 m fuze).
- **The semi-active penalty, as a number.** 28.56 s of unbroken illumination for one R-27R shot against
  the AIM-120's 5-15 s; break the lock in flight and the round misses by 27.04 m where an AIM-120 with
  the same loss still hits by 0.755 m.
- **The RCS calibration is the identity for the F-16.** `σ^¼` scaling with the F-16's own 1.2 m² as the
  reference, so all 55 F-16 missions came out byte-identical on every column and every event, and the
  asymmetry (1.351× / 0.740×) exists only across types.
- **30 mm is a different weapon in the same currency.** A kill on 67 of 150 rounds at 294 m of round
  path; the FULL drum at 571 m wipes the target's avionics without downing it. The documented
  200-790 m effective band emerging from the dispersion model rather than from a range limit.

**Three defects the measurements found**, each fixed where it belonged rather than where it showed:
the MiG's gun never learned its own unit id and therefore shot ITSELF down at the muzzle (the runner's
shooter exclusion compares `LauncherId`); `FBFlightControl` returned before its alpha limiter in
`Manual`, so every hand-stick phase on an airframe whose deck has no limiter was unbounded — invisible
until BFM became the first phase that really pulls; and the BFM roll-rate cap inverts a PLANT, so with
another aircraft's constants it is an oscillator rather than a cap (identified for this airframe:
a = 0.819, K = 201 °/s against the F-16's 0.734 / 78.7).

**One long-standing gap closed by measuring instead of arguing.** The MiG's corner formula read −16 %
against the harness. Neither hypothesis survived: the altitude loss inside the window is worth +1.7 %
and the convexity of `√(n²−1)` +0.4 %. The harness was measuring the rate of the body's EULER HEADING
while the formula predicts the turn rate of the VELOCITY VECTOR, and at 22.7° of incidence in an
85°-banked pull those differ by 18 %. Measured directly, the formula is right to **1.4 %** — better
than the F-16's own −2 %. The correction went to the harness's reporting, not to the formula.

**What is honestly not finished.** The MiG-29 has no dispensers at all (no source states the
BVP-30-26's programme parameters), so the flare asymmetry currently runs entirely one way. And its BFM
is unfinished: the N019's close-combat modes are pencils in azimuth and its wide mode does not
auto-lock, so a manoeuvring MiG cannot acquire — 0 contact ticks in 134 s, measured.
`FBPilot::BfmDesignate` gives the pilot the thumb he needs (a no-op on an auto-locking set), but the
cold-search law still rolls the jet before the first two looks land. `mig29-gun` therefore measures the
WEAPON from a stable position with a briefed burst, and says so in its header.

### 2026-07-28 — MiG-29 stage 2b: the sensors and the guidance

Three real sensor derivations, **one new generic slot**, and GCI as mission data.

**`sensors/FBIrstSystem` is the fourth sensor slot and the fifth file allowed to read
`units/FBUnitRegistry`.** The boundary was never a COUNT — it is "only simulated sensors, each paying a
stated price" — and the widening is recorded where it is enforced: `tools/verify_layers.py`'s
`RESTRICTED` table FAILED on the new include until the file was added to it by name. An IRST pays in
range (25 km at best against the radar's 50), in identity (no interrogator, and `core/FBIrstContact` has
no field one could be put in) and in weather, and gives back the one thing no other sensor here does:
it costs the observer nothing to look.

**Two generic constants became hooks, both defaulting to the previous behaviour exactly.**
`DopplerNotchMs(rangeM)` + `NotchRejectsDetection()` (until now the notch was ONLY chaff's channel — a
target in the filter stayed visible; a set whose source QUANTIFIES the threshold now rejects) and
`CoastS(volume)` (the N019's source names a duration, not a frame count). The RWR grew four:
`Blanked`, `ReportBearingDeg`, `ClassifyMode`, `PriorityRank`. `FBUnitSignature` gained
`Afterburner`, read off JSBSim's own `FGTurbine::GetAugmentation` rather than off a throttle position.

**Measured against the documented numbers** (four new rigs, all TIMEOUT by construction): detection
latency **6.0 s** (the derivation runs the other way — the documented "up to six seconds" over the
generic two-look firming IS the 3.0 s frame time); the Doppler envelope rejecting at **7.94 m/s vs
41.67** beyond 8 nm and **4.34 vs 16.668** inside 5.4 nm; **`coastS=6`** inertial tracking; the SPO-15's
forward hemisphere going dark in the SAME tick ILLUM is acknowledged, with the emitter's `fcr_on`
unchanged, and its bearings reported as channel centres (−10.0° where the F-16 reports 0.045°); the IRST
aspect law separating a tail-on detection at **19 562 m** from a 103°-aspect one at **15 222 m**; the
6 km laser stepping `irst_lock_nm` from **−1** to 3.199 nm; a target above a GFS deck never detected
(`irst_masked`, the first tactical weather effect on a sensor here); and the GCI chain taking **8.0 s**
from the controller's call to a radiating radar, with the opposing RWR lighting up 0.1 s later.

**Two defects found by building the rigs, both fixed and both measured.** (1) A set powered up mid-run
replayed its whole silent period through the catch-up guard and reported a firm track in the tick the
switch moved (t=27.9 instead of one frame later) — `ResyncScan()`, opt-in, so the F-16 is untouched.
(2) Timing the SPO-15's documented 125-250 ms illumination event classified EVERY search emitter as
tracking, because the emission model publishes a searching beam as continuous (`mig29-pair`, t=0.3:
the F-16's CRM sweep reported as TRACK). The event half of that rule now waits for a pulsed emission
model; the channel half — the actual device defect — is what the override contributes.

`set task intercept` is unlocked for this module, and the honest outcome is that the intercept
DISENGAGES on first contact: `pilot/FBPilot`'s own rule is "a target on the scope and nothing on the
rails → Abort", and this jet has no weapon yet. F-16 byte-identical across all **56** stock missions on
every column they ever had; the four MiG missions move exactly once, because the N019's power-up
emission position is OFF and this aircraft now starts silent by doctrine.

### 2026-07-28 — C2: the mission clock, and the ephemeris moved down a layer (this round)

One mission-wide `time 1999-03-24T22:00:00Z` line, Zulu only, 1901…2099, converted by a
days-from-civil calendar in `core/FBCivilTime.h` rather than by `timegm` (which would read the host's
zone and make the same file mean a different sky in a container); absent means **no clock at all**, which
is why all **84** pre-round missions stay byte-identical — 259/259 telemetry files bit-for-bit, 84/84
`events.log` identical modulo `wallS`/`speedup`/path, at `--threads` 1, 2 and 4 — and a client `--utc`
that contradicts a declared `time` is a **boot error** (`missions/FBClockBoot.h`), not a precedence.
The price of the round was structural, not the parser: `render/FBEphemeris.h` became
`core/FBEphemeris.h` (`FBSunPos`/`FBMoonPos`, `double` seconds) so `sensors/` can reach the sun for
`C3`, proven pixel-exact by identical `--utc 922312800` PNGs in SVS and EVS; and `fb-gym`, which had
no ephemeris at all, now writes `FBEnvironmentBlock` through `FBSimUnit::UpdateSolar` →
`FBModule::SetSolar`. `missions/clock-night-payerne.fbm` proves arrival twice — `mission CLOCK
utc=1999-03-24T22:00:00Z sunElDeg=-37.0489` in the log, `blk_env`=1 for all 2 167 rows — and proves
the clock is a stamp and not an input: against `payerne-airstart.fbm`, same spawn and route, the two
telemetry files differ in **exactly that one column**.

### 2026-07-28 — C3: the eye — the sixth registry reader, and what measuring it corrected (this round)

`sensors/FBVisualSystem` is built to the contract of `sensors.md` §9 and is the SIXTH file allowed to
read `FBUnitRegistry`, declared in the gate before a line was written. One inequality — presented extent
over range against a contrast-scaled 12-arcmin threshold — fed by a chain of laws that were all already
in the tree: the presented dimensions come from `FBDamageLayout` (which gained the plan extent, so the
gun and the eye read ONE table), the haze from Koschmieder + the ISA scale height, the daylight from
`FBDaylightFactor` moved out of the renderer into `core/FBEphemeris.h` (its fourth consumer, not a
second dusk), the glare from Stiles–Holladay collapsed into one readable half-angle, and the cloud from
a MARCH through `core/FBCloudDensity` — the price §6.5 declined for the IRST. **Measured, not asserted:**
head-on 2 493 m against side-on 3 784 m (the ratio IS the two presented dimensions); zero contacts at
night against nine by day on byte-identical geometry; 1 206 m against 2 373 m looking into a 6.6° sun
(the reach ratio 1.97 equals the contrast ratio 1.94 — exactly inverse-linear, as specified); a crossing
line of sight through a deck at optical depth 22.5, never seen, with `vis_masked` marking exactly the
window in which the same air without the deck would have shown it. The anonymity claim `w5-03`/`o2-08`
hang on is now a measurement: two runs differing only in the target's `team` token produce
**byte-identical telemetry and identical `vis` lines**, both naming `mig29`.

**Three places the contract was wrong, corrected in `sensors.md` rather than quietly satisfied.** §9.4's
worked ranges used dimensions the layout does not declare and left out its own haze term. §9.6c bought
the march on "a 40 % deck is mostly hole" — measured over the whole committed fixture the opposite
holds: at 40–50 % cover every ray is optically closed while a LID calls it clear, so the march differs
from a lid only in the 20–50 % band and there it is STRICTER. And §9.9's promise that `events.log` stays
byte-identical "because no mission declares a visual scenario" is false: 38 of 93 missions have aircraft
inside a few km of each other. The requirement BEHIND it held exactly — 285/285 telemetry files carry
every pre-existing column identical position-for-position with nine appended, 93/93 `events.log` have no
pre-existing line changed, and no trajectory moved, because nothing consumes the block yet (deliberate,
the D3 precedent). Determinism `--threads 1/2/4` byte-identical; the march costs −1.1 % on an
8-aircraft mission, i.e. below the run-to-run noise.

### 2026-07-28 — C12: the target vocabulary — four objective kinds, and a cover rule with an exit code

`identify unit X range <m> hold <s>`, `protect unit|team`, `no_fire` and `deny release unit|team` are
built. The whole price is what the spec said it would be — one monotone bit (`ReleasedWeapon`) and one
float (`RangeM`) on `FBUnitObservation`, both filled by the OWNER from registers it holds itself: the
bit at the one place the runner drains `Stores().TakeRelease()`/`Guns().TakeBurst()`, the range from the
published poses the CPA already runs on, and only when some unit declares an `identify`. One correction
against the estimate: `no_fire` asks about the DECLARING unit, and the monitor has no identity with
which to find itself in the roster, so the same bit also rides on `FBMissionMonitorSample` beside
`CombatIneffective` — a field more than counted, in the struct that already carries the twin. `identify`
measures the GEOMETRY and not the sensor event, at the stated price and for the stated reason; the IFF
half is beside the verdict in the log (`radar IFF_REPLY … reply=none`), never inside it.
**`FBObjectiveCovers` returns false for all four**, and that is measured rather than asserted:
`missions/objective-covers-none.fbm` exits 1 with `decisive=1` on the shot-down striker, and with the
predicate patched to cover, the same file exits 0 — one line of source, two verdicts. Honest limit,
measured too: inside a single mission a wrong `protect` cover is invisible, because the protector's own
FAIL is decisive either way; that half is held by the exhaustive `switch` (`-Werror=switch`). Eight new
missions, seven of them a pair ONE number apart, cover fulfilment and violation of every kind that can
be violated: 0/3 for the identification box, 1 for the broken weapons hold, 0/1 for `protect`, 0/3 for
`deny release` — the last pair being the one thing `kill` cannot say, since the kill succeeds in both.
Conservation held at full strength against the pre-round binary: **260/260 telemetry files and 85/85
`events.log` of the 85 pre-round missions byte-identical** (modulo `wallS`/`speedup`/path) at
`--threads` 1, 2 and 4.

### 2026-07-28 — C0: the campaign layer — an order, three facts, and two determinism proofs (this round)

The fourth and last foundation contract, built where its spec put it: `core/FBCampaignFile` parses the
`.fbc`, `core/FBCampaignState` holds the three carried facts as canonical text and applies them,
`missions/FBCampaignRunner` loops `FBRunMission` and aggregates, `fb-gym --campaign` drives it. The
mission runner grew **one** optional parameter (`const FBMissionCarry *`) and no phase: the overlay
lands between step 1 and step 2, the outcome is read off the same actors step 4 judges. Null carry =
the run that existed before — measured over all **104** `sim/missions/*.fbm` + `negative/*.fbm`
against a reference binary built from the same tree with the five touched files reverted: **104/104
fingerprints identical** (exit code, telemetry bytes, `events.log`).
The overlay came out NARROWER than the contract allowed: the spec permitted changing the value of an
existing `set` line, nothing needed it, and `FBApplyCampaignCarry` therefore contains no path that
writes a value — it erases a `unit` block or a `set store` line and asserts afterwards that neither
count grew. That is measured in both directions: `viper-attrition` drops `bandit`, `bunker` and three
`set store` lines with a `campaign CARRY` line each, while a hand-written state file demanding
`mk82=9`, a unit `ghost` and a `ground newtarget` produces the **identical fingerprint to the run with
no state at all** and zero `CARRY` lines. Stores land as a per-(unit, kind) stock that enters the book
on the first sortie declaring the kind — so a type the jet has never carried is not capped, which is
the difference between attrition and inventing state.
Both acceptance criteria of §5 held, on BOTH ground bases: criterion 1 gives **9 runs, one campaign
fingerprint** each (`f6dda7e6…` under `swiss`, fb-gym's own default, `0811c2cc…` under `const`, what
the four missions declare), 7.9 s for nine runs; criterion 2, the one that matters, gives **4/4 steps
reproducing STANDALONE** from the previous step's `campaign-state.txt` alone, fingerprint and exit code,
under both. The layer adds no hidden state.
The first version of criterion 2 was a FALSE PASS that a central re-check turned into a false alarm:
`fb_campaign_verify.py` defaulted its own `--elev` to `const` while `fb-gym` defaults to `swiss`, so a
campaign started the way a human starts it replayed 4/4 DIVERGED on `groundAsl=782.97` against
`groundAsl=0`. The lesson is not a better default. A fingerprint compares two runs over the SAME ground
or nothing, so the run now RECORDS the ground beside its state (`elev`/`swiss_dem`/`base`/`threads` in
`campaign-summary.txt`, carried in by `FBCampaignEnv` because the runner sees only an
`FBElevationProvider&`), the check READS it, `--elev` became an override, and a tree without the record
is refused instead of replayed against an assumption. The comparability rule is now in the Spec, as a
fourth way determinism can be lost. Result-relevant switches, enumerated: `--elev`, `--swiss-dem`,
`--base` (recorded); `--threads` recorded and result-neutral by criterion 1; `--timeout` structurally
unreachable for a campaign step; weather and clock have no gym flag at all. The tool also names the
fingerprint's normalisation exactly (`wallS`/`speedup` and the `--out` path inside `telemetry=`,
nothing else) — without the second, criterion 2 cannot even be stated.
The campaign itself is the demonstration: two of its four steps change verdict because the carry
worked — the DLZ rig finds its bandit dead and flies with the one round the first sortie left it (0 →
3), and the CCIP pass finds the bunker already rubble (0 → 3). `fb_tournament.py` stays where it is;
one measures a pilot over independent geometries, the other a force over a dependent sequence.

### 2026-07-28 — C1: the unit level, specified — one class, nine rows, and two collisions (spec only)

Step 2 of the owner goal, and a **spec round with no code**: `doc/modules/ground/` (`INDEX.md`,
`module.md`, `catalogue.md`, `cast.md`). `C1` blocks six campaigns and is the top four rows of the
aggregated cast table collapsed into one system; the gap entry that stood in `weapons.md` with five open
questions is now a pointer, and the five are answered.
The decision the round exists for is a **line**, not a feature: a module is a *flown* airframe (FDM,
avionics bus, pilot phase machine, one class per type, a reference base of its own); a unit is one
data-driven class with N catalogue rows. The test is not importance but **"does anybody fly it"**. Both
levels share the whole of `FBSimUnit` — identity, published pose and signature, health register, damage
model, roster, telemetry, mission judge, the `.fbm` `unit` block, the registry key — and differ in exactly
the four things a jet has because somebody sits in it.
Three structural answers carry the round. **The registry gate does not widen:** the site's four detectors
derive from `FBRadarSystem` (×2), `FBVisualSystem` and `FBRwrSystem`, and a derivation adds no include —
`verify-layers` printing *6 restricted header(s) respected* is an acceptance criterion, not an intention.
**The health register is used, not inherited:** `FBSystemHealth` stays monotone, private-mutator,
one-friend and untouched, and killing a site's `Radar` silences it through §8's coupling, written years
before this. **Doctrine is a sensor:** `set emcon hold` means the set is dark until the site's own passive
receiver hears an airborne emitter — a timer or a range trigger would be the site knowing something it
never measured, and the resulting experiment (a silent attacker never cues the defence) is `w4-01/02` from
the defender's side.
The round asks the tree for **one** core change and two enum values: `FBUnitSignature` carries two emitter
beams (a battery is two antennas; collapsing them by precedence deletes the search→track transition for
every observer except the tracked one), `FBSeekerKind::CommandGuided` (the uplink branch of the existing
phase machine, forever), and `FBEmitterKind::SurfaceEarlyWarning`/`SurfaceFireControl` — the discriminator
`sensors.md` gap 25 was waiting for, deliberately left unwired.
**Two collisions found by writing it, both booked in `weapons.md`:** the weight-on-wheels interlock refuses
100 % of ground launches (a unit without an airframe reports `AnyWow = true` by definition), and the SMS
declares its station masses through an `FBFdm` a site does not have. Both are rules written for a pilot
meeting a machine that has none. A third finding stays open: `CombatEffective()` is `Structure` alone on a
site, so **suppressed and destroyed are the same word** — and SEAD is precisely the difference.
Two things fell out of existing measurements without being tuned. The eye's measured reach (3 784 m
beam-on, 2 493 m head-on, **zero at night**) is shorter than every MANPADS envelope, so the *sensor* binds
a MANPADS engagement and the weapon becomes a last-two-kilometres daylight weapon. And on a stationary
mount `OwnClosureOn = 0`, so the Doppler notch degenerates to "the target's own range rate" — the beam
manoeuvre works against a ground set with no new code, and a conical-scan row declaring notch 0 is simply
not notchable, which is the honest statement about that hardware.
The catalogue is nine rows with a source and a tier each, seven disputes carried unresolved, and thin
sourcing declared: eight rows rest on [T4], one has a [T3] monograph that disagrees with the [T4] entry on
the envelope, and no [T1] threat handbook was read. The honest headline of the whole spec: a site can be
**heard and not seen** (no air-to-ground radar mode, no HARM), so `C1` gives the ground the ability to
shoot back long before it gives the air the ability to shoot first.

## 2026-07-28 — `C1` gebaut: die aktive Bodenbedrohung

Neun Stellungen, EINE Klasse (`modules/ground/FBSiteModule` + `core/FBSite.h`), gebaut gegen das am
selben Tag geschriebene `## Spec`. Die drei benannten Hindernisse haben getragen, zwei davon anders als
vorgeschlagen: `FBUnitSignature` trägt jetzt `Radar[2]` und **303/303 Telemetriedateien plus 104/104
`events.log` aller Bestandsmissionen bleiben byte-gleich** (Threads 1/2/4) — bauartbedingt, weil ein
Flugzeug nur Index 0 schreibt und die RWR-Schleife bei einer Keule dieselbe Arbeit tut wie der
Skalarzugriff vorher. Die Gewichts-auf-Rädern-Verriegelung wurde NICHT gelockert, sondern als
unanwendbar erklärt: `DeclareGroundLauncher()` ist privat mit genau EINEM Freund (dasselbe Schreibtor
wie `FBSystemHealth`), gibt false zurück, sobald je eine Zelle gebunden wurde, und `AttachFdm`
assertiert die Gegenrichtung — ein Flugzeug kann die Zeile nicht einmal aufrufen. Die zweite Kollision
(„der SMS setzt eine Zelle voraus") war bereits gelöst: `PublishLoadout` und `Release` halten ihre
`Fdm_`-Wächter seit jeher.

Zwei vorhergesagte Effekte, beide reproduziert ohne eine Zeile dafür: der Doppler-Notch auf stehendem
Mast degeneriert zur reinen Zielradialgeschwindigkeit (`ownClosMs=0`, gemessen), und das Auge bindet die
MANPADS auf 3 288 m am Tag und auf NICHTS in der Nacht (dieselbe Datei, eine Zeile Unterschied).

Verworfen mit Messung: eine MANPADS trifft in dieser Geometrie NICHT (883 m), weil eine Runde auf der
Schiene keinen Sucher-Ton hat — die Lücke steht als B1 in `## Gaps` statt als breiteres Sucherfeld im
Code. Ein Abnahmekriterium des Vertrags maß nichts: `verify-layers` druckte die Zahl der geschützten
Header (zwei), nicht die Länge der Registry-Leserliste (sechs); das Werkzeug druckt jetzt die Liste
selbst, und sie ist unverändert **6**.

## 2026-07-28 — Verbundene Luftabwehr: das Netz bewegt eine Antenne, es erzeugt nie einen Track

`C22`/`C23`/`C24` gebaut, `C13` halbiert. Der ganze Bau ist ZWEI Wertheader (`core/FBNetReport.h`,
`core/FBZone.h`), vier Setter plus ein Test am bestehenden `sensors/FBDatalinkSystem`, vier kurze
Schritte an der bestehenden `modules/ground/FBSiteFireControl`, eine Zielart, zwei Missionsgeltungs-
bereiche und ein publizierter Skalar. **Keine neue Klasse liest die Registry** — `verify-layers` meldet
weiterhin *6 registry reader(s) inside the perception boundary*, und ein siebter wird nachweislich
abgewiesen (gegengeprüft, rc=1). `core/FBZone.h` kommt als geschützter Header mit LEERER Includer-Liste
dazu: ein Pilot, der einen deklarierten Gürtel läse, wüsste ohne Sensor, wo die SAMs stehen — auch das
gegengeprüft (rc=1).

Der tragende Satz ist gemessen: `net-blind-cue.fbm` legt eine Mk 82 auf 52,32 m neben eine eingewiesene
Stellung (2 086,81 J/m² — `Radar` FAILED, `FireControl`/`Structure`/`Stores` nur degradiert), die
Einweisung liegt von t=8,1 s bis zum Ende ununterbrochen an (`net_cue` = 1, keine weitere Transition),
und es entstehen **null** `site TRACK`-Zeilen.

Was das Netz wert ist, in einer Zahl: dieselbe Geometrie mit und ohne `net`-Block ergibt 2 `site LAUNCH`
gegen 0 und `site RADIATE` bei t=8,0 s gegen nie. Der Schichtkuchen: dieselbe Route, nur die Höhe
verschieden, ergibt 34,5 s in `flak` / 0 Starts / 54 Feuerstöße gegen 0,0 s in `flak`, 320,0 s in
`sambelt` / 1 Start / 0 Feuerstöße. Blind gegen zuversichtlich blind: mitten im Lauf gestört verliert
die Stellung ihren Knoten bei t=128,0 s, nachdem sie sich seit t=8,0 s durch Strahlen verraten hat; von
Anfang an gestört bleibt sie stumm und unsichtbar. Und Störung nimmt NUR die Leitung: die `site
TRACK`-Zeile ist byte-identisch zur ungestörten (`brgDeg=206.713 rangeM=21977.2 closureMs=223.135`).

Zwei Funde beim Bauen. Erstens: `RadioHorizonM` rechnete mit NN-Höhen — zwei Stellungen auf 936 m ASL
„sahen" einander 252 km weit. Jetzt sind beide Argumente Höhen ÜBER GRUND plus die deklarierte
Masthöhe des Netzes; **die Luftreichweiten ändert das nachweislich nicht** (336/336 Telemetrien,
112/112 events.log byte-identisch), weil der Horizont auf Jägerhöhe nie gegen die 150 nm des Terminals
bindet. Zweitens: `emcon hold` prüfte `ThreatCount > 0` statt „airborne emitter", wie die Spec es sagt —
eine Batterie ging hoch, weil das eigene Frühwarnradar nebenan drehte. Ab dieser Runde steht so eines
nebenan; korrigiert, byte-identisch.

Verworfen mit Messung: die Einweisung als DETEKTIONS-Vorteil ist in diesem Baum nicht messbar. Ohne
Terrainmaskierung (`C4`) findet ein 50-km-Suchset alles innerhalb von 50 km, gleich welches
Elevationsfenster — was der Cue messbar wert ist, ist das AUFWECKEN einer stummen Stellung und die
Feuerleitautorität, also Doktrin statt Detektion. Steht so in `## Gaps`.

## 2026-07-28 — Die Luft-Boden-Hälfte: eine Waffe, die auf Sender zielt, und ein Gegner, der abschalten kann

`C8` (ohne Raketenpod), `C26` und `C27` geschlossen; `C25` unberührt. Sechs Stores fliegen — `agm88`
`mk84` `gbu12` `cbu87` `fab250` `fab500` —, zwei neue `FBSeekerKind`-Werte, die Abwurfhülle als
Prüfung 8, `objective suppress`, `set emcon react`, `set attack_mode arm`. Der Antiradiationssucher IST
der Warnempfänger (`FBMissileArSeeker : FBRwrSystem`, zwei bestehende Hooks, **kein neuer Include**):
`verify-layers` meldet unverändert *6 registry reader(s) inside the perception boundary*. Alle 113
übrigen Missionen byte-identisch, Telemetrie UND `events.log`, über `--threads 1/2/4`.

**Abschalten hilft, und die Grenze ist gemessen statt gesetzt.** Das Gedächtnis ist eine RATE: nach dem
letzten Empfang wird die gemessene Sichtlinienrate 4 s gehalten, dann null — die Proportionalnavigation
befiehlt seitlich nichts mehr, die Schwerkraftvorspannung überlebt, der Ausrollflug ist gerade. Die
Entkommensgrenze bei 20 km liegt gemessen bei **85,0 %** der Flugzeit (5°-Schuss) und **88,1 %**
(35°-Schuss) — vorhergesagt waren 61 % und 76 %. Die QUALITATIVE Vorhersage hält exakt: der Frontalschuss
ist der schwer zu entkommende, und das fällt aus der Geometrie. Die quantitative ist zugunsten des
Angreifers optimistisch, aus drei benannten Gründen: `ZEM(0)` in der Spec ist nur die seitliche Hälfte
(die 11,3°-Depression trägt 3,9 km), der Abfall ist gemessen `(t_go/t_f)^2.3` statt `^4` auf einer
verzögernden Runde, und das Gedächtnis verschiebt die Grenze um +6,4 pp. Nicht angeglichen, gemeldet.

**Die Bruch-Vorhersage für `B1` war zweifach falsch.** Nicht zwei `events.log` ändern sich, sondern
fünf (dazu `deny-release-broken` `escort-protect` `escort-protect-lost` — genau der Fall, den die Spec
in ihrem eigenen Restrisiko-Absatz nannte: der RWR der F-16 ist per Default an), und fünfzehn
Telemetriedateien in acht Missionen bewegen zwei Spalten, die die Vorhersage gar nicht bedacht hatte:
`fcr_on` und `iff_xpdr` des SENDERS über sich selbst. Kein Verhalten ändert sich irgendwo — jeder Diff
ist eine reine Löschung von Phantomzeilen. Und `net-blind-cue`s `set alert cold` war KEINE Umgehung:
ohne die Zeile bekommt die Batterie einen festen Track und die Datei verliert die Null, die sie zeigen
soll. Kommentar korrigiert, Vorhersage zurückgezogen.

Zwei weitere Funde. `msl_sig` (Spec §9) und Byte-Identität (Spec §10, Kriterium 2) schließen einander
aus — `msl_*` ist nicht die letzte Quelle am Bus einer Runde, eine Spalte dort verschiebt 95 gemessene
Dateien; die Anhänge-Regel gewinnt, die Zahl steht in `rwr_leth` und in den Ereignissen. Und ein F-16
mit bugfestem Bezeichner kann eine Lenkbombe aus einem waagerechten Anflug **nie** bis zum Einschlag
beleuchten: eine antriebslose Bombe legt Boden mit `v·cos θ` zurück, der Jet mit `v`, also ist er immer
zuerst da. Bei 250 kt/4 000 m hält er den Fleck durch und die Bombe trifft auf 3,9 m — bei 450 kt
verliert er ihn 5,7 s vorher und liegt 229 m kurz. Steht als F4 in `## Gaps`.

Nicht gebaut und benannt: der Raketenpod (`hydra70`/`s8`, Design C) und die Luft-Boden-Entfernungs-
messung (`C25`). Ein Schlagzeug, das die Spec noch nicht hatte: `set emcon` nimmt jetzt einen
gebrieften Emissionsplan (`free <offS> [<onS>]`) — ein Wert an einem bestehenden Schlüssel, sonst ist
das Entkommensfenster nicht messbar, weil `scoot_s` einen Start und `react` einen Treffer braucht.

## 2026-07-28 — Vier Katalogzeilen von ALPHA auf ACCEPTED, und ein Instrument, das seine eigene Regel las

Vier gemessene Ursachen, in der Reihenfolge, in der sie gewirkt haben — und die erste war eine andere
als benannt. Der Schubkanal war seit `d1e1d79` da und der Nachbrenner brannte; an seiner Stelle stand
der **Prüfstand**: der Tank lief WÄHREND der Messung leer (`f15c` verlor die Augmentation bei t = 870 s
und M 2,04 wurde als Vmax gebucht, während `(T−D)/W` noch bei +0,025 stand), und acht Zeilen flogen
unter der Startmasse, auf die jeder Anker bezogen ist. Beides festgehalten macht die Residuen ZUERST
schlechter (A1 −18,4 → −23,4 auf `f15c`) und alles Folgende überhaupt lesbar.

Dann die eigentliche Ursache von A1, und sie war weder Widerstand noch Schub, sondern **Buchführung im
Deck**: der auftriebsabhängige Widerstand stand als Tabelle über α mit 5°-Stützstellen, und ein
Überschall-Dash sitzt bei 1,8° — die Sehne durch eine Parabel liefert dort das 2,9-fache
(`CDi = 0,00519` gegen `k·CL² = 0,00179`). Dazu `kCLmach` doppelt gebucht. Gegen `aero/cl-squared`
geschrieben, mit 1°-Stützstellen: A1 auf sechs von zehn Zeilen von −20 % auf −3 %.

Die Startstrecke war nicht das Fahrwerk. `Cmδe`, „INV gegen A5" gerechnet, kam 7,5-fach unter dem, was
das eigene Leitwerksvolumen aus §2 hergibt, und konnte keine Nase heben — Vollausschlag ab 167 kt hielt
2,8° Nicklage bis 226 kt. `[GEO]` aus dem Leitwerk: alle drei veröffentlichten Startstrecken im Band
(−17,8 / +0,7 / +9,7 %).

A4 bleibt Sonde, aber **je Zeile und gerechnet**: für welche Masse ist die veröffentlichte Steigrate mit
dem eingefrorenen Schub erreichbar? `f15c` 13 141 kg, `su22` 9 982, `mirf1` 6 024 — alle drei UNTER der
eigenen Leermasse. `mig17` erreicht seine 65 m/s bei Startmasse und behält A4 als Anker (−1,9 %).

Die Abfangmaschine flog jede Zeile mit den Gains der MiG-29. `FBFlightControl::Raw(P, α_lim, g_lim)` ist
der fehlende Satz: der Grenzwinkel der ZEILE, eine hergeleitete Nickautorität (voller Stick trimmt das
1,5-fache des eigenen Grenzwinkels) und die g-Gains auf diese Autorität normiert. Dazu der
g-Begrenzer, den §6 seit jeher versprach und den die Klasse nicht hatte. `air-bomber-intercept.fbm`
hält 8 002,8 m über 400 s statt bei 510 m aufzuschlagen.

**Das Instrument:** die 2,4 gegen 1 203,6 waren kein Widerspruch, sondern eine zweiseitige Lesart einer
einseitigen Regel. Eine differentielle Empfindlichkeit (±10 %) neben einer endlichen Differenz (ein
anderes Flugzeug) — die gesunde Signatur. Der Defekt, den §Spec 11 wirklich benennt, ist die andere
Kombination, und `fb_tournament.py` prüft ihn jetzt, statt die Zahl zu drucken. Für die Spenderzeile
selbst gibt es keine Zelle: Regel statt Zahl. Und die reparierte Prüfung fällt sofort durch — die Arena
ist gesättigt (`mig21`: 19 Läufe, ein einziger unterscheidet sich), was vorher ein Verhältnis zweier
Gleitkomma-Nullen als 0,6295 verdeckte.

**4 ACCEPTED** (`f15c` `mig21` `mirf1` `f5e`), 6 ALPHA mit je ein bis zwei benannten Ankern: die
Spreizflügel-Wahl kostet 26–28 % Dienstgipfelhöhe (R14, im Voraus deklariert), die Turbojet-Schublapse
−14/−23 % auf zwei Zeilen (R15), der Begrenzerabfall 1,2° auf `mig17` (R16), und `su27`s A1 −6,3 % ist
die eine Abweichung ohne gefundene Ursache (R17). Kein Band geweitet. Rückschritt, gemessen und
benannt: `air-awacs-cue`s Kriterium 5 galt nur, weil der Abfangjäger dabei abstürzte — er fliegt jetzt,
und die Zelle erfasst nicht mehr.


### 2026-07-29 — Doktrin-Evolution `E1`: Fitness, Genom, Archiv, Arena

**Wofür die Runde war.** `doc/doctrine-evolution.md` war eine reine Spezifikation. Diese Runde hat sie
gebaut, in der Reihenfolge, die sie selbst vorschreibt — erst die Eingabe der mittleren Stufe, dann die
Fitness, dann die Arena, dann Gene und Archiv — und nach jedem Schritt gemessen.

**Der Richter veröffentlicht jetzt einen Zielvektor.** `mission OBJECTIVE unit=… kind=… state=met|unmet|
violated`, eine Zeile je erklärtem Ziel, an dem EINEN Punkt, den jeder Abschluss passiert
(`FBMissionMonitor::Conclude`), nicht in `Finalize` — eine Einheit, die im `Tick` FAILt, käme dort nie
an. Preis wie angekündigt und gemessen: **432/432 Telemetriedateien byteidentisch**, 77 von 137
`events.log` unverändert, 60 um genau **136** Zeilen gewachsen, keine entfernt, keine andere bewegt.

**Die Fitness ist lexikographisch und wohnt in einer Datei.** `(V, M, C)`, links nach rechts verglichen,
Aggregation über paarweise Dominanz statt über einen Mittelwert. `hits landed` und `no shot` sind
ENTFERNT, nicht neu gewichtet; letzteres ersetzt ein Tor, das man nicht zurückkaufen kann.

**Beleg C, abgelesen statt hergeleitet.** Der Mechanismus stimmt exakt — ein Bündel IST ein Tick
(`gun BURST rounds=1 → 5 → 10` im 0,1-s-Takt), `NoteHit()` genau einmal je Bündel, `dmg_hits` gleich der
Zahl der `gun HIT`-Zeilen (8/8, 24/24, 23/23). Die ZAHL stimmt nicht: vorhergesagt 1.500 Fitnesspunkte je
Sekunde Feuer, gemessen **900 / 1.038 / 1.278** — die Herleitung ist eine Obergrenze, weil
`kMinReportedHits` 15–40 % der Bündel wegfiltert.

**Beleg A hat sich NICHT umgedreht, und das ist der wichtigste Befund.** Auf `mirror` steht die
Einzelkämpfer-Variante weiter oben — aber der Abstand fällt von 120,7 Punkten `hits landed` auf **0,9
Punkte `shot lead`** bei exaktem Gleichstand in V und M über alle acht Läufe. Auf `split`, der einzigen
Geometrie des Paars, die überhaupt etwas entscheidet, liegt die kooperative Variante auf **beiden**
entscheidenden Stufen vorn (V 4,25 gegen 4,00). Die Spezifikation behauptet in ihrem Wissensteil, die
neue Fitness sei in einer gesättigten Arena „ehrlich still" — sie ist es nicht, weil C bei Gleichstand
konsultiert wird und zwei Fließkommazahlen nie gleich sind. Statt an der Ordnung zu drehen, meldet das
Werkzeug es jetzt: `decided at level: V 2  M 0  C 18` und ein `SATURATED`-Block.

**Die Arena war gesättigter als die Zahl, die das Kriterium erzwungen hat.** `mirror` — die Geometrie,
auf der jedes veröffentlichte F-16-Doktrinergebnis dieses Baums gemessen wurde — hat **100 % modale
Ergebnisklasse und 1 von 9 Hebeln**. Die alte Arena (2 Geometrien, 1 informativ) fällt durch. Elf
F-16-gegen-F-16-Kandidaten wurden geflogen und alle fielen durch; entsättigt hat nicht die Geometrie,
sondern die **Zelle**: die neue Arena hat 8 Geometrien und **4 informative** (`far`, `split`, `xmirror`,
`xclose`), drei davon mit einer MiG-29 im Ostsitz oder mit langem Anflug. S3 ist auf dieser Arena nicht
berechenbar und sagt das, statt eine Null zu drucken — ihr Instrument stört einen GENERIERTEN
Katalogdeck, und F-16 und MiG-29 sind Prinzip-1-Modellkopien.

**Das Genom kann keinen Absolutwert buchstabieren, und das ist Syntax.** `FBPilotTuning` trägt jetzt
`Free` und `Scale`; ein `Scale`-Eintrag verlässt die Klasse nur durch `Scaled(p, own) = own · Or(p, 1)`
und zwei `static_assert`s weisen ein `Scale`-Band zurück, das nicht dimensionslos ist oder keinen Haken
nennt. Die Laufzeit-Hälfte ist ein Exit-Code: `genome-absolute-refused.fbm` schreibt die eigene
Eckgeschwindigkeit des Jets (380) ins Gen und **endet mit 1** vor dem ersten Tick;
`genome-scale-flown.fbm` ist dieselbe Datei mit 0,85 und bewegt den Gashebel in **2.979 von 3.001**
Ticks. `fb-gym --pilot-keys` druckt das Alphabet, damit kein Werkzeug eine zweite Kopie der Tabelle führt.

**Archiv und Kreis-Messung stehen, und der erste Lauf misst einen Gleichstand.** Nicht-dominierte
Aufnahme, deterministische Schrittprobe, Kappe 64, alle drei Instrumente (fester Maßstab, zyklische
Tripel `T`, Doktrin-Trajektorie). Beide Läufe: **jedes Individuum exakt 0,500**. Grund, benannt statt
kaschiert: G4 wirkt nur in der `bfm`-Phase, die ein BVR-Abfang bis zum Timeout nie betritt, und G2 ist
inert — `flt_defer_s` = 0,0 in **132 von 132** Spuren an beiden Enden des Bandes, weil die AIM-120 0,3 s
bindet und die Regel damit nie feuert. Genom und Arena schneiden sich noch nicht; das steht als E-13.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` und `verify-models` grün,
`git status --porcelain sim/assets` vor und nach jedem Evolutionslauf leer, Determinismus über
`--threads 1/2/4` (434 Telemetriedateien, 139 Logs, 0 Unterschiede), und die von
`fb_tournament.py` erzeugten Missionen sind gegen das HEAD-Skript über beide Altgeometrien
**480 verglichen, 0 verschieden**.


### 2026-07-29 — Doktrin-Evolution `E2`: der Gleichstand war der VERGLEICH, nicht das Genom

**Der Auftrag war „Genom und Arena zum Schneiden bringen".** Die Diagnose der Vorrunde — jedes
Individuum 0,500, weil die Gene in dieser Arena nicht wirken — ist zur Hälfte falsch, und die falsche
Hälfte war die Ursache.

**Erst je Gen der Hebel, an einer Einzelmission, ohne Aggregation.** G1 und G5 sind keine Schlüssel:
`set pilot_flight_shape 1` bzw. `set pilot_emcon_frac 0.5` liefern `SET_INVALID_VALUE` +
`SET_REJECTED` bei t = 0,0 und **Exit 1** — harte Sperren (F5, D3), nichts zu messen. G4 greift, aber nur
in der `bfm`-Phase: `bfm_ctrl_s` 267,4 / 267,9 / **268,8** s und `bfm_es` 17.397 / 17.392 / **17.378** ft
über das Band, gegen einen LEBENDEN Gegner `bfm_es` 42.690 / 44.976 / **46.360** ft. G3 greift und die
Allel-Tabelle war falsch: `SORT_ASSIGN` 0 / 2 / 6 / 41 / 41 / 41 — die drei `dl=on`-Zeilen sind bis auf
die letzte Stelle identisch, weil die kooperative Zuweisung den gebrieften Vertrag überstimmt. Vier
Allele waren zwei. G2 ist NICHT inert: die 132/132 Nullen der Vorrunde standen auf `xmirror`, dessen
Ostsitz eine MiG ohne Datalink ist — mit Netz auf beiden Seiten `flt_defer_s` 0,0 → **6,3** s und
`flt_both_s` (5,6; 4,6) → (0; 0) auf `split`. Die obere Hälfte des Bandes bleibt tot.

**Und dann die eigentliche Ursache.** Auf `split --flight 2` trägt der WESTSITZ den Schlüssel:
West-C = +505,5…+526,7, Ost-C = **+69,0 in 12 von 12**. §1.4 vergleicht die beiden SEITEN EINES Laufs,
also über die Sitze hinweg — und gibt damit in beiden gespiegelten Läufen denselben Sieger zurück, jede
Variante nimmt genau einen von zwei Punkten, das Feld steht **konstruktionsbedingt** bei 0,500. Der Sitz
ist dort 457 Handwerkspunkte wert, das Genom 21,2. Gebaut: `fb_fitness.match_points` — ein MATCH ist das
gespiegelte Laufpaar, verglichen wird **Sitz gegen denselben Sitz**. Die Ordnung ist unangetastet.
A/B auf DENSELBEN Telemetrien: quer 0,500 × 12, gleich-Sitz **0,227…0,773**. Die veröffentlichten
Turniere bewegen sich nicht (Beleg A und B bit-gleich in ihren Zahlen) — ein heterogenes Feld kollabiert
unter der Querregel nicht, ein homogenes total.

**Der Nahkampf ist gebaut und entscheidet keine Doktrin.** Drei neue Geometrien mit eigenem
Missions-Profil (`merge`, `xmerge`, `xmergesplit`), weil `FBPilot` genau EINEN Übergang nach `Phase::Bfm`
hat und das der gebriefte Task beim Spawn ist. Gemessen über 70 Nahkampfläufe: **6 Feuerstöße, 0 Treffer,
0 Startvorgänge**, und was die Klasse `(2,0)` wirklich ist, ist die MiG-29 im Boden — 9 von 11
Ost-Ergebnissen `CRASH`. G4 bewegt als einziger Hebel eine Ergebnisklasse (2 von 9 auf `xmergesplit`),
und jede Geometrie, auf der es das tut, fällt durch S2. **Das Tor wurde nicht gelockert** — G4 bleibt
gesperrt, jetzt mit drei benannten Ursachen statt einer Vermutung.

**Die Arena, zweimal gemessen.** Bei `--flight 1` mit den erklärten neun Hebeln: 12 Geometrien, **4
informativ, BESTANDEN**. Bei `--flight 2` mit dem ALPHABET DES GENOMS (`tools/levers-genome.txt`,
`--flight N` neu im Tor — E-12): **1 informativ von 12, ABGELEHNT.** Die eine ist `xfarsplit` (langer
Anflug + Energiesplit + Waffenbindung, 3 Beweger von 9, alle drei G3-Allele, Feld 4 Klassen bei 48,3 %
Modalanteil); 17 Kandidaten wurden dafür gesiebt, 16 lieferten 0 oder 1. Ein 2v2 ist **gesättigter** als
ein 1v1: acht von zwölf Geometrien legen bei `--flight 2` jeden Lauf in dieselbe Klasse.

**Der Schnitt-Nachweis.** `xfarsplit --flight 2`, 4 Generationen × 6: Verteilung statt Mittelwert —
Gen 0 `0,700 0,700 0,400 0,400 0,400 0,400`, Gen 2 `0,773 0,773 0,500 0,500 0,500 0,227`. Entschieden
wird beim ERGEBNIS, nicht beim Handwerk: **312 Sitzvergleiche, V 96 · M 0 · C 152 · exakt gleich 64**,
vier Ergebnisklassen über 720 Seitenschlüssel. Kein §1 veröffentlicht: der Champion ist ein Fixpunkt
(Maßstab 0,583 flach, T = 0,0000), und die eine bewegende Geometrie hängt an einer einzigen
Hebel-Familie.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` (297 Dateien, 12 Schichten) und
`verify-models` grün, fünf Harnesses rc=0, `git status --porcelain sim/assets` vor und nach jedem
Evolutionslauf leer, Determinismus `--threads 1/2/4` über `xfarsplit`/`merge`/`xmerge`/`xmergesplit`/
`split` byte-gleich, und alle 140 bestehenden `.fbm`-Dateien unverändert.

## 2026-07-29 — Der Merge starb an einem Dämpfer, den nur der Autopilot hatte

**Der Befund kam vor der Korrektur, und er hat den Schuldigen ausgeschlossen statt ihn zu vermuten.**
E-15 las „was die Merge-Geometrien entscheiden, ist ein CFIT" — bei n = 120 Läufen je Durchgang sind es
**77 Monitor-K.O.s, und in 77 von 77 ist es die MiG-29** (38 ATTITUDE_CONTACT, 37 CFIT, 2
STRUCTURE_CONTACT). Die F-16 stirbt in keiner Merge-Zelle, in keinem Sitz. Sitztausch: die MiG stirbt
auch im Westsitz (t = 351,2); MiG gegen MiG überlebt 420 s. Also weder Geometrie noch Sitz.

**Der isolierende Versuch hat gar keinen Gegner.** Eine MiG-29 auf `set task bfm`, nächster Feind
100 km weg — nur der kalte verankerte Suchlauf, 300 s, drei Starthöhen, derselbe Missionstext ein Modul
weiter für die F-16: mittleres g-Kommando **4,57** gegen 1,22, mittlere Querlage **76°** gegen 40°,
p95 |VS| **183 m/s** gegen 9, **CFIT aus allen drei Höhen** gegen keinem. Die Ursache steht in den
ersten acht Sekunden: 0,32 Nickstick erzeugten **3,05 g auf ein 0,37-g-Kommando**, danach schlägt die
Schleife im 1,5-s-Takt zwischen +PitchStickMax und −kBfmPushMax um.

**Die Ursache war eine Zeile in der Zellenschicht, nicht im Piloten und nicht am Boden-Deckel.**
`KqDamp`/`KpDampRoll` sind der Dämpfer DIESER Zelle (SAU-451 „DAMPER", drei Achsen; flight-model-spec
§7.4 nennt ihn wörtlich „FBFlightControl's inner rate loop") und banden nur im FLCS-Zweig — BFM
kommandiert `Manual`. **Die MiG flog jeden Nahkampf mit ausgeschaltetem Dämpfer.** Es ist dieselbe
Auslassung wie bei `PitchStickMax` und dem Anstellwinkel-Begrenzer, eine Ebene tiefer (pilot.md §5.10a,
die fünfte Schraube). Beide Gains sind auf sich selbst getort; die F-16 hat sie exakt 0 und läuft den
Zweig nicht. Die Gierachse bleibt aus: dieser Ruderzweig ist GEMESSEN abgeschaltet.

**Was das Deck angeht: nicht angefasst.** Ein `Cmq` hätte dieselbe Kurzperiode gedämpft und niemand
hätte es gemerkt — es wäre kein belegtes Delta gewesen, und ein besseres Missionsergebnis ist kein
Beleg. `verify-models` grün, `sim/assets` byte-gleich.

**Vorher/nachher, dieselben 120 Läufe je Durchgang:** K.O.s **77 → 0**; Kanonen-Salven **191 → 386**,
und **→ 2 652** mit `gun HIT` **0 → 897**, nachdem das Merge-Profil die KANONEN-Kontrollposition brieft
(0,15–0,40 nm; die Vorgabe 0,5–1,5 nm ist eine Raketen-Halteposition außerhalb des Trichters, und
`Phase::Bfm` hat überhaupt keinen Raketenschuss — gemessen: 132,4 s Kontrollposition bei median 2,64 nm,
`gun_in_funnel` 0 in 4 200 Takten). `duel-merge` Exit 2 → 3, volle 300 s, kein K.O.; `mig29-bfm`
`bfm_ctrl_s` **0,0 → 287,6 s** bei unverändertem 60-°/s-Rolldeckel — die Kontrollposition, die §5.7.3
als Preis der Roll-Schranke verbucht hatte, war nie deren Preis.

**Und die Energieregel? Der Hebel greift, das Ergebnis nicht.** Dreipunkt-Sweep auf `xmergesplit`:
`bfm_es` 33 592 / 39 358 / 42 492 ft, `bfm_ctrl_s` 34,2 / 31,9 / 2,1 s, Schuss 150 / 150 / 0, Treffer
23 / 23 / 0 — **die NIEDRIGE Schiene konvertiert.** Die Ergebnisklasse bewegt sich trotzdem nicht: die
ganze Trommel legt **6,37 Schuss von 150** bei 3,41 m mittlerem Fehlabstand auf die F-16, drei Systeme
aus, `dmg_effective` 1,00. Also kein §1 veröffentlicht — und die Merge-Zellen verlieren ihren S1-Pass
mit dem Defekt, der ihn getragen hat (2 Klassen bei 50,0 % → 1 Klasse bei 100 %). Das Tor wurde NICHT
gelockert. Der Blocker ist jetzt ein einziger benannter: eine Kanone, die trifft und nicht tötet.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` (297 Dateien, 12 Schichten) und
`verify-models` grün, zehn Harnesses rc=0, Determinismus `--threads 1/2/4` über alle 139 Missionen
byte-gleich, und **134 von 139 Missionen byte-identisch** — die fünf, die sich bewegen, sind alle
MiG-29 und je einzeln begründet (`duel-merge` 2 → 3, `mig29-bfm` ctrl_s 0 → 287,6, `mig29-full` /
`mig29-landing` weichere Aufsetzer bei 143,2 → 143,3 bzw. 141,0 → 142,2 kt, `mig29-takeoff` −0,2 s).

---

## 2026-07-29 — Der Kurvenkampf kann töten: eine Waffe, die nie eingesetzt wurde, und ein Abzug, der in die Zukunft zielte

**Zwei Befunde, und der erste ist eine Herleitung, keine Zahl.** Die GSh-301 legte 6,37 von 150 Schuss
bei 3,41 m mittlerem Fehlabstand auf die F-16 und tötete nie. Aufgeteilt: ein `damage KILL` durch 30 mm
kostet **17,0 Treffer** in einer Zone (`kFlcsFail` 1,5·10⁵ J/m² gegen 8 803 J/m² je Treffer) — und bei
der Streuung, mit der der Merge tatsächlich gefochten wird (σ 3,78 m auf 630 m Geschossweg), landet eine
**perfekt gezielte** Trommel 20,2 Schuss und tötet. Also ist die Wirkung nicht der Deckel, die **Zielgüte**
ist es: die Trommel tötet bei einem mittleren Fehlabstand ≤ 2,38 m, gemessen wurden 8,72 m. Und
`missM ≈ RangeM·tan(GunAimErrorDeg)` auf den weiten Bündeln beweist, warum: der Zielfehler **an der
Mündung** IST der Fehlabstand.

**Der Abzug sagte etwas voraus, das die Geschosse nichts anging.** `pred = err + rate·(Latenz +
Flugzeit)` extrapolierte eine EIN-TICK-Ableitung eine volle Sekunde weit. `FBGunSolveLead` beantwortet
aber „wohin muss das Rohr zeigen, damit eine JETZT abgefeuerte Runde später trifft" — die Zielbewegung
während der Flugzeit steckt bereits in der Lösung, eine abgeflogene Runde hat von einer sich danach
bessernden Zielung nichts. **11 von 13 Feuerstößen wurden außerhalb des eigenen Trichtertors
kommandiert, der erste 14,6-fach.** Der Horizont ist Latenz + halber Feuerstoß. Ergebnis über die
120-Lauf-Merge-Arena: Schuss auf dem Ziel **139,8 → 449,1** bei **6 318 → 5 850** verfeuerten (2,21 %
→ 7,68 %); `gun-turning` behält seinen Abschuss auf **279 → 209** Schuss.

**Der zweite Befund war ein fehlender Pfad, kein fehlender Wert.** `Phase::Bfm` endete im Kanonenfeuer;
AIM-9 und R-73 hingen ohne Einsatzweg an den Schienen. Erst spezifiziert (`pilot.md` §5.11), dann gebaut:
fünf Tore, jedes ein Instrumentenwert, keine neue Rechnung — gewählter Store ist Infrarot, Lock, in der
Startzone, im **Cueing**-Winkel der ZELLE (`BfmWvrCueDeg`, MiG 60° vom Schtschel-3UM, F-16 der Kardan der
Runde), und die vorige Runde hatte ihre Flugzeit. **Aspekt ist kein Tor** (beide Runden sind dokumentiert
allaspektfähig) und **eigene Last auch nicht** — dafür gibt es keinen Mechanismus, und eine Zahl ohne
Mechanismus wäre erfunden. Nach dem Start bindet nichts: `FBSeekerHandoverS(Infrared) = 0`.

**Der Kurvenkampf wird jetzt entschieden, und beide Zellen können sterben.** `duel-merge` Exit 3 → 0:
die AIM-9 kommt 1,93 m heran, 218 781 J/m², Flugsteuerung ausgefallen, Abschuss bei t = 10,5 s. Die neue
`duel-merge-stern.fbm` ist der Gegenbeweis — die R-73 auf der dokumentierten Heckviertel-Geometrie, 590
m/s Annäherung statt 1 050, kommt 1,86 m heran und tötet die F-16 bei t = 21,3 s. Frontal verliert die
MiG **auch wenn sie zuerst schießt**: die Abschussradien gegen diese Zelle sind 2,32 m (AIM-9M, 9,4 kg)
und 2,08 m (R-73, 7,4 kg). Über die Arena: Ergebnisklassen 60/60 (2,1) auf allen drei Zellen →
`xmerge` **30 (3,2) + 30 (1,0)**, jeder Lauf entschieden, alle 40 Abschüsse durch Flugkörper, keiner
durch die Kanone.

**Und die Energieregel bewegt jetzt die Ergebnisklasse — auf `merge`, mit allen drei Allelen, und der
Beweger ist ein CFIT.** Drei von 43 Monitor-K.O.s sind gesunde Zellen, und genau diese drei sind es; die
anderen 40 sind bereits abgeschossene Jets, die fallen. E-15s Regel gilt unverändert: eine Geometrie,
deren Klasse ein CFIT bewegt, misst den CFIT. G4 wird nicht veröffentlicht, `kMoversMin` nicht gesenkt,
`fb_arena_check.py` unverändert REFUSED.

**Die Versuchung, benannt und abgelehnt.** Die schnellste Art, die Kanone tödlich zu machen, war die
Fragilitätsleiter in `FBF16Damage` — vier `[SET]`-Zahlen, die auch jeden Gefechtskopf im Baum bepreisen.
Sie zu heben hätte AIM-120 und R-27R neu bewertet, um ein Ergebnis zu kaufen. Die Messung sagt, dass sie
nicht der Deckel ist. Ebenso abgelehnt: Rtr als WVR-Tor (die gespeicherte Tabelle gibt der AIM-9 22 740 m
Rtr auf 3,2 km Startentfernung — ein Tor, das nichts misst) und ein g-Limit ohne Trennmodell.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` (297 Dateien, 12 Schichten, 3
restricted headers, 6 Registry-Leser) und `verify-models` grün, acht Harnesses rc=0, Determinismus
`--threads 1/2/4` je ein Fingerabdruck, `git status --porcelain sim/assets` leer, **130 von 139
Missionen byte-identisch** — die neun, die sich bewegen, je einzeln begründet, plus eine neue
(`duel-merge-stern`). Ein Exit-Code bewegt sich: `duel-merge` 3 → 0.

## 2026-07-29 — Kampagne O4 gebaut und geflogen: die Zehn-Meilen-Behauptung, vermessen

**Schritt 4 des Eigner-Ziels beginnt, und O4 ist die erste**, weil sie als einzige der zehn Kampagnen
beide FlightBox-Zellen zeigt, die historisch wirklich gegeneinander flogen (JG 73 "Steinhoff", Laage,
~450 Einsätze gegen F-16 [T4]). Zehn `sim/missions/o4-*.fbm` plus `sim/campaigns/o4-gaf-mig29g-dact.fbc`,
Arena über der Ostsee (55,20 N 13,60 E [SET], `--elev const`, weil 0 m dort die Wahrheit ist), Bodenziele
in jeder Mission, jede Mission mit ihrer verbindlichen Leseregel im Kopf.

**Das Ergebnis ist eine Aussage, keine Zahl.** Die berühmte Behauptung des Fulcrum-Piloten — *"inside ten
nautical miles I'm hard to defeat"* — hält an ihrer eigenen Außenkante und stirbt im Messerkampf:
**10 nm MiG (R-73 Raero 20 km und ±60° Helmvisier bieten den Schuss 11,0 s vor der AIM-9), 5 nm
gegenseitiger Abschuss 0,1 s auseinander, 2 nm F-16 (9,4 kg gegen 7,4 kg Gefechtskopf, Ankunft 1,40 m
gegen 2,61 m).** Zwei der drei Gründe, die das Zitat nennt, sind modelliert und entscheiden; der dritte,
das IRST, trägt bei KEINER Entfernung etwas bei — kein Konsument im Piloten UND bei Frontalanflug 10 km
Reichweite statt 25 km von hinten (`irst_contacts` = 0 in beiden Läufen).

**Was die Kampagne findet und der Anker nicht nennt: das Magazin.** Die F-16 trägt einen
Drei-Einsatz-BVR-Vorrat, die MiG-29 einen Ein-Einsatz-Vorrat. Einsatz 3 fliegt mit dem, was 1 und 2
übrig ließen, und das Gefecht KIPPT: allein geflogen schießt die MiG zweimal und die F-16 nie
(R-27R 10,16 m), in der Kampagne schießt die F-16 (AIM-120 5,13 m) und die MiG hat keinen Radarflugkörper
mehr. Beide unentschieden, aus entgegengesetzten Gründen.

**Die zwei blockierten Missionen, gegen den heutigen Baum geprüft statt geglaubt.** Mission 6 (Merge)
läuft und ENTSCHEIDET — der Blocker war Wiedererfassung, und die Frage stellt sich nicht mehr, weil der
Kampf auf dem ersten Pass fällt. Mission 9 (Nacht) läuft, `C2` und `C3` sind gebaut — und kann ihre Frage
weiterhin nicht beantworten: von **184 Telemetriespalten unterscheiden sich sechs**, alle visuell, und
beide Läufe töten denselben Jet im selben Tick. Das Loch hat jetzt eine Zahl statt einer Behauptung.

**Kein Ergebnis mit zwei möglichen Ursachen ohne Kontrolllauf.** Das Wetter-Fixture ändert Wolke UND
Wind; der Wind-ohne-Wolke-Kontrolllauf reproduziert den Wetterlauf auf drei Dezimalstellen
(3,033/1,615/2,026 gegen 3,007/1,608/2,027 m). **Der Umschwung ist zu 100 % der Wind.** Was die Wolke
nimmt, ist gemessen und folgenlos: das AUGE der MiG, 50 Kontaktframes und RECOGNISED auf 0 Kontakte und
7 `vis MASKED` bei Transmission 0,011.

**Der Fingerabdruck fand einen echten Defekt in der Kampagnenschicht.** Kriterium 2 war beim ersten
Versuch **9 von 10 DIVERGED** — `fb-gym --mission --state` konnte die KAMPAGNENUHR nicht empfangen, und
nur Mission 9 (eigene Uhr) passte. `viper-attrition` deklariert keine `time` und konnte das nie zeigen.
Geschlossen wie §5 den Boden schließt: die Uhr wird AUFGEZEICHNET (`campaign-summary.txt: time`) und
GELESEN, Empfänger ist `fb-gym --campaign-time ISO` — Kampagnendaten, nie eine Client-Uhr.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` (297 Dateien, 12 Schichten) und
`verify-models` grün, acht Harnesses rc=0, `git status --porcelain sim/assets` leer. Determinismus:
**9 Läufe, 1 Kampagnen-Fingerabdruck** `461e0ff5299d83d03b…`, und **10/10 Schritte** einzeln aus dem
Vorzustand nachgespielt. Konservierung: **515/515 `telemetry*.csv` und 150/150 `events.log`
byte-identisch** gegen ein Binary mit den zwei berührten Quellen zurückgesetzt, 0 Unterschiede über
`--threads 1/2/4`. `viper-attrition` unverändert (9 Läufe 1 Fingerabdruck, 4/4 Replays).

## 2026-07-29 — Kampagne O1 gebaut und geflogen: die kanonische Niederlage hängt an 3,5 Sekunden

**Zehn `.fbm` + eine `.fbc`, ohne eine Zeile C++.** `sim/missions/o1-*.fbm` +
`sim/campaigns/o1-bekaa-1982.fbc` — Bekaa-Tal, 9. Juni 1982, die syrische Seite. `git status
--porcelain` listet elf unverfolgte und **null geänderte** Dateien: die Kampagne ist reiner
Missionstext, genau wie ihr eigenes Spec es vorhergesagt hatte. Konservierung ist damit keine Messung,
sondern eine Konstruktion — das Binary, das O1 flog, ist das Binary, das alles davor flog.

**Der Aufbau: ein Baseline, sechs Ein-Hebel-Varianten, ein Kontrollpaar, eine Zwei-Schritt-Kette.**
Jede Mission nennt ihre Kontrolle im Kopf. Die Regel, die dabei herausfiel und die die übrigen acht
Kampagnen erben: **ein Kettenschritt kann nicht zugleich eine kontrollierte Variante sein** — der
Übertrag ist auf den Rufnamen geschlüsselt, also unterscheidet sich ein erbender Schritt von jedem
Geschwister in zwei Dingen. Zehn Plätze fassen deshalb genau das obige, und O1 musste ein Spec-Paar
(gestaffelt gegen massiert) STREICHEN. Es sagt im `.fbc`-Kopf welches und warum.

**Das Spec irrte sich über seinen eigenen Mechanismus.** Es sagte, „zuversichtlich blind" brauche einen
Leitoffizier, der mitten im Lauf verstummen kann (`C6`). Falsch: `set brief_gci <atS> …` trägt seit
jeher seine eigene Zeit, also IST ein abgeschnittener Brief genau dieses Experiment. Die Fähigkeit
wurde gelesen, nicht gebaut.

**Die Hebel-Tabelle, gegen die Baseline (2 MiG kampfunfähig, 0 F-16):**

| Hebel | Rot verliert | Blau verliert | bewegt |
|---|---:|---:|---|
| — Baseline `o1-01` | 2 | 0 | — |
| GCI gelöscht, frontal | 1 | 0 | Rot-Kontakte 8→4, Schüsse 2→1; die Ausgangsänderung ist ein **Sort-Artefakt** (beide AMRAAM auf dieselbe MiG) |
| GCI abgeschnitten, frontal | 2 | 0 | **nichts** |
| Eintritt 45° | 0 | 0 | Blaus ganzen Schuss |
| GCI gelöscht bei 45° | 0 | 0 | **die ganze Begegnung**: 9→0 Kontakte, 2→0 Schüsse |
| RWR aus | 2 | 0 | **nichts** |
| `pilot_shot_rtr 1.4`, `lock_nm 16` | **0** | **2** | **die ganze Schlacht**, um 3,5 s Tempo |
| Kommunikationsstörung 0→90 km | — | — | `site LAUNCH` **5→0**, Cues 79→32 — **Bodenschaden auf den Meter und den Tick identisch** |

**Was übrig bleibt, wenn nichts es bewegt.** Der gesamte Ausgang sitzt in einer 3,5-Sekunden-
Abschussentscheidung. Alles andere — Leitoffizier frontal, Warnempfänger, Gürtel, Netz, die vom Anker
als entscheidend benannte Störung, der Kampagnen-Übertrag — bewegt Mechanismen und **keinen Ausgang**.
Zwei dieser Nullen sind Modelleigenschaften (der SPO-15 warnt 13 s vor dem Einschlag, weniger als
Reaktion + erste Abwehr + Düppelprogramm; frontal zeigt der gebriefte Wegpunkt die Nase schon auf den
Gegner), zwei sind Defekte.

**Der größte Fund ist vorbestehend und stand in einer eingecheckten Mission.** Die 3M9 des 2K12 und die
V-601 der S-125 kippen nach vorn und erreichen den Boden **an den eigenen Startkoordinaten**, 0,8–1,6 s
nach dem Abschuss; der 59-kg-Gefechtskopf der 3M9 wird dann als `damage KILL` auf der eigenen Batterie
verbucht. Sichtbar in `net-cue.fbm`, t = 172.8 s. Fünf Starts in Sortie 08, **null Ankünfte**, zwei
Selbsttötungen. Das macht vier geschlossene Lücken (`C1`/`C22`/`C23`/`C24`) und eine ganze Themendatei
unfähig, irgendeinen Ausgang zu bewegen. **Nicht behoben** — die Decks liegen unter
`sim/assets/aircraft/`, das diese Runde nicht anfassen darf.

**Zweiter Fund: eine Batterie hat kein IFF.** `FBSiteModule` setzt `SetIffInterrogator(false)`, ein
`FBRadarContact` trägt keine Identität — also beschießt eine Stellung die nächste feste Spur in ihrer
Hülle, egal wem sie gehört. Gemessen auf dem ersten Layout: drei V-750 in die eigene Kampfpatrouille
binnen 7 s. O1 weicht geometrisch aus und **kann damit die eigentliche syrische Taktik des Ankers
(„zurück unter den Schirm") gar nicht fliegen.**

**Dritter Fund: `FirstFlightKo` beendet den ganzen Lauf.** Ein Jagdduell und ein SEAD-Anflug in einer
Datei messen das Duell zweimal — die erste Fassung von Sortie 08 endete bei t = 237.0 s, mit den
Angreifern 130 km vor dem Ziel. Konsequenz: die SEAD-Paarung fliegt ohne Jäger, und die große Sortie
setzt ihre Angreifer hinter die Patrouillenlinie.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` (297 Dateien, 12 Schichten, 3
restricted, 6 Registry-Leser) und `verify-models` grün, sechs Harnesses rc=0, `git status --porcelain
sim/assets` leer. Determinismus: **9 Läufe, 1 Kampagnen-Fingerabdruck**
`81b549fd04c4591987b9dadf233deffdabbbfb01f9dc89f4f7f0d4486d7bba8e`, und **10/10 Schritte** beim ersten
Versuch einzeln aus dem Vorzustand nachgespielt — das Uhrenloch, das O4 gefunden hat, blieb geschlossen.

---

## 2026-07-29 — Der Bodenstart: drei Defekte auf unserer Seite der Naht, kein Deck angefasst

**Der größte Fund der O1-Runde ist behoben, und die Prognose dieser Runde war falsch.** „Das Beheben
heißt `sim/assets/aircraft/` anfassen" stand in `o1-bekaa-1982.md` und in diesem Journal — es hieß
nichts dergleichen. Sieben Dateien unter `sim/src/`, kein Deck, `verify-models` unverändert grün
(1 deklariertes Delta, 34 FlightBox-eigen).

**Defekt 1: die „kein Boden"-Regel der Waffe griff einen Aufruf zu spät.** `FBFdm::LoadUnguarded` rief
`RunIC()`, während JSBSims Gelände noch auf seinem eigenen Vorgabewert stand; die Geländehöhe kam erst
danach. `FGLGear` löst seine Kontakte **innerhalb** von `RunIC()` auf. Eine 6,09 m lange V-601 auf einer
70°-Schiene hat ihren Heckpunkt damit (6,09/2)·sin 70° = **2,86 m unter** dem Datum, die Feder antwortet
mit einem Drehimpuls, und der Integrator trägt ihn aus Schritt 1 heraus. Belegt mit einer **rohen
JSBSim-Sonde ohne jede FlightBox-Lenkung** — die Zahl gehört der Zelle, nicht dem Regler:

| Runde | Schiene | q bei Schritt 1 |
|---|---:|---:|
| `v601` | 90° | **0,000 °/s** |
| `v601` | 70° | **−79,284 °/s** |
| `v601` | 45° | **−114,76 °/s** |
| `3m9` | 45° | **−179,81 °/s** |

Behoben durch ein **eigenes** Feld `FBFdmSpawn::TerrainElevM`, vor der Anfangsbedingung angewandt und
getrennt von `GroundElevM`, das nur den Spawn platziert. Vorgabe ist JSBSims eigenes Datum — also genau
das, wogegen jeder Spawn in diesem Baum seine IC immer schon gerechnet hat. Die Konstante zog von zwei
anonymen Namensräumen in **eine** Klasse: `FBFdm::kNoGroundElevM`.

**Defekt 2: der Motor war 0,55 s kalt.** Bei t = 0,51 s war die Runde noch im freien Fall
(**4,98 m/s = 9,81·0,51**); aus 0,5 m Starthöhe sind das ½·9,81·0,55² = **1,48 m Sinken durch den Boden,
bevor überhaupt Schub anliegt**. Ein luftgestarteter Flugkörper fällt frei und zündet dann — genau das
ist die Schubrampe. Ein **Schienenstart** trennt sich, *weil* der Motor ihn von der Schiene geschoben
hat. Neues `FBFdmSpawn::MotorRunning`, gesetzt aus `FBStoreRelease::HaveRail`, also falsch für jeden
Speicher, der je einen Pylon verlassen hat.

**Defekt 3: `FBStoreSpec::GatherS` wurde von keiner Zeile Code gelesen.** Deklariert, für alle sechs
Bodenrunden mit Wert belegt, in zwei Doku-Dateien spezifiziert, nie gebaut — die Sammelphase existierte
nur auf dem Papier. Jetzt der frühe Rücksprung in `FBMissileGuidance::FlyCommand`: die Ruder **schleppen**,
das Gesetz darüber läuft weiter, und `msl_nz_cmd` ungleich null neben `msl_fin_pitch` gleich null **ist**
die Phase in der Spur. **Keine neue Zahl:** die sechs Werte standen bereits belegt im Katalog. Neben den
gerechneten Brenndauern `t = P·Isp/T` (4,499 / 2,498 / 3,995 / 1,992 / 1,975 / 1,982 s) fällt nur beim
V-601 beides zusammen; die fünf anderen bleiben bewusst unangeglichen — „Sammelphase endet bei
Boosterabwurf" ist bei einer Schulterwaffe schlicht falsch.

**Gemessen.**

| Prüfung | Ergebnis |
|---|---|
| V-601 im Flug, vorher/nachher | +70° → **−41°** in 1,6 s, Einschlag 7 m unter Grund **gegen** 70,00 / 69,97 / 69,95° gehalten und **868,8 kt = 447 m/s** am Ende der 2,5-s-Sammelphase |
| Selbstzerstörung | **null** — keine Stellung zerstört sich mehr, in keiner Mission |
| `o1-08` Detonationen am Angreifer | **9,15 / 8,57 / 4,87 m** (V-601, Zünder 10 m) und **0,28 / 4,09 m** (3M9, Zünder 8 m) |
| Der Störsender kostet jetzt alles | `o1-08` ungestört: 8 Starts, 7 Detonationen, `bolt1` abgeschossen, **alle fünf Bodenziele intakt**. `o1-09` gestört: **0 Starts, 0 Detonationen**, beide Angreifer „objectives met", **zwei Stellungen zerstört**. Vorher war der Bodenschaden beider auf den Meter und den Tick identisch — die Messung, die O1 als Defekt gebucht hatte, ist aufgelöst |
| Konservierung | **10 von 160** Missionen geändert, **150 byte-identisch**, alle zehn mit Bodenstart. Zwei Exit-Codes bewegen sich: `o1-08` 3→2 (der Angreifer wird abgeschossen und trifft *dann* den Boden), `sam-sa2-command` 0→1 |
| Determinismus | `--threads 1/2/4` auf vier bewegten Missionen: je ein Hash. Beide Kampagnen bestehen weiterhin beide Kriterien (O1: 9 Läufe ein Fingerabdruck, 10/10 Schritte; O4: 10/10) |
| Tore | `verify-layers` (297 Dateien, 6 Registry-Leser), `verify-models` grün, Bau warnungsfrei |

**Drei Defekte werden JETZT ERST SICHTBAR, und keiner davon ist der behobene.** Der Bodenkontakt hat sie
verdeckt — die Runde starb, bevor sie sie zeigen konnte. Alle drei stehen als Lücken B4/B5/B6 in
`modules/ground/module.md`:

1. **Die V-750 kann ihr eigenes Überkippen nicht ausführen.** Flach fordert die Lenkung nie mehr als
   **0,53 g** — eine 80°-Schiene wird gegen eine 2,5°-Sichtlinie nie herumgeholt; steil erreicht sie
   −1,23 g, geht von 80° auf 42° und trifft. Reine Proportionalnavigation plus Schwerkraftvorspannung hat
   **keinen Mechanismus** für ein großes kommandiertes Überkippen; eine echte S-75 fliegt ein
   **programmiertes**.
2. **Eine tragbare Runde mit ungültigem Feuerleitzustand beim Start entkäfigt ihren Sucher nie.** Der
   frühe Rücksprung `if (!HaveTarget_)` liegt **über** dem Block, der den Infrarotkopf entkäfigt; Sucher
   und Querbeschleunigung bleiben den ganzen Flug null. Das ist die genaue Ursache der älteren Lücke
   „MANPADS ohne Sucherton" (B1), die damit **offen bleibt**.
3. **Die geteilten Flugkörper-Reglerbeiwerte lassen die 9,8-kg-Schulterwaffe departieren** —
   Kontrollverlust bei 5,1 s Flugzeit, Ruder an den Anschlägen, Anstellwinkel ±4°. Ein Beiwertsatz über
   drei Größenordnungen Masse.

**Die Lehre, und sie korrigiert eine Regel im Kampagnen-Index:** nicht *ein* Fund pro Kampagne, sondern
ein **Stapel** — der erste Defekt verdeckt den nächsten. Und: ein deklariertes, belegtes, zweimal
spezifiziertes Feld, das niemand liest, ist keine Spezifikation, sondern eine Lüge mit Herleitung.

## 2026-07-29 — Kampagne O5 gebaut und geflogen: das Vokabular war die leichte Hälfte

Zehn `sim/missions/o5-*.fbm` + `sim/campaigns/o5-airfield-defence.fbc`, Batajnica 24.–26. März 1999,
`--elev const`, drei erklärte Nächte. Kein `sim/src/`, kein Deck, keine Zeile an den 160 bestehenden
Missionen — die Binärdatei, die O5 geflogen hat, ist die, die alles vorher geflogen hat. Beide
Determinismus-Kriterien beim ersten Versuch: 9 Läufe eine Kampagnen-Signatur
(`f59fc642c86ccecd2691…`), 10/10 Schritte einzeln reproduziert. Vollständige Messung in
[`campaigns/o5-airfield-defence.md`](campaigns/o5-airfield-defence.md) `## State`.

**Die Antwort auf die eigene Frage — was hält den Flugplatz?** Eine Rotte, die schon oben steht. Sie
verweigert einem Zweierpack die Hälfte seiner Bomben, und nichts sonst in diesem Baum kommt in die
Nähe. **Was umsonst ist:** der Lotse (6 s auf den ersten Blick des Flügelmanns, kein Ergebnis — die
dritte Kampagne, die das sagt), die Nacht, die innere Rohrwaffe, die gehärteten Shelter und die Bahn.
Der Gürtel verweigert einem von drei Angreifern die Freigabe — und **eine Mk 84 auf seinen P-18-Knoten
in Nacht eins kostet ihn zwei Nächte lang jeden Start** (7 → 0 und 6 → 0, standalone gegen Kampagne
gemessen). Genau die `C22`-Vorhersage der eigenen Spec, angekommen als Zahl.

**Drei Defekte, alle auf unserer Seite der Naht, keiner hier repariert:**

1. **Der Alarmstart ist nicht ausdrückbar.** `set task` setzt die Phase BEIM SPAWN und `FBPilot` hat
   keinen Übergang `Route` → `Intercept`. Ein Bodenstart mit Kampfauftrag rollt von der Bahn (FAIL bei
   t = 11,1 s) oder überschlägt sich (`ATTITUDE_CONTACT`, t = 35,4 s). Das ist der Parameter, den die
   Spec selbst den wichtigsten der Kampagne nennt.
2. **Keine Katalog-Zelle kann eine Waffe einsetzen.** `modules/air/FBAirModule` komponiert *keine*
   Feuerleitung, also wird `FBState::FireControl` nie geschrieben und alle drei Einsatztore in
   `FBPilot` bleiben zu. Eine `f15c` mit vier AIM-120 hält 28 s einen festen Lock von 18,6 auf 8,8 nm
   und drückt nie. Vier Zeilen sind `ACCEPTED` — als **Flugmodelle**, nicht als Kämpfer.
3. **Die GCI-Suchelevation ist ein Weltwinkel in einem Körperkommando.** `FBMig29Pilot` postet
   `atan2(Δh, R)` direkt auf `RadarSlewEl`; das eigene Suchgesetz in `FBPilot` zieht dort ausdrücklich
   `st.pitch` ab. Im waagerechten CAP fällt der Unterschied in die Kommando-Totzone — bei einem
   steigenden Abfangjäger ist er der ganze ±6°-Balken: **null Kontakte in 700 s über 726 m minimalen
   Abstand.**

**Und die Messung, die O5 als einzige Geometrie erzwingen konnte:** eine FlightBox-Stellung hat keinen
IFF-Abfrager, und ein Flugplatz ist der eine Ort, an dem das eigene Flugzeug die nächste feste Spur
ist. Der Gürtel schießt seine **ersten drei Runden nach Osten** (`brgDeg` 116,5 / 90,6 / 91,0) — auf die
eigenen MiG-29. Dass die Kette keine Maschine an eigene Flugkörper verliert, liegt einzig daran, dass
der Angreifer in Nacht eins den Knoten zerstört hat, der die Starts freigegeben hätte.

**Die Lehre:** das Zielvokabular (`C12`) war die leichte Hälfte und ist geflogen — `deny release`,
`protect`, `avoid zone`, `no_fire`, alle vier. Was es ersetzt hat, ist kleiner und härter: die Kampagne
konnte ihre eigene Hauptfrage nicht stellen, weil das Format den Alarmstart nicht kennt. Erwarte den
Defekt in der Naht, in die du nicht geschaut hast.

## 2026-07-29 — Ferne Berge waren nicht vernebelt: eine Luft für Decke und Gelände

Der Eigner hat es gesehen, die Messung hat es bestätigt: **`FBTilesStage` hatte null Referenzen auf
Sichtweite, Dunst oder Extinktion.** Die drei Stellen, an denen der Renderer die gemeldete Sichtweite
las, lagen alle in `FBCloudLayerStage.cpp`. Zwei Bilder derselben Kamera bei **5 km und bei 80 km
Sicht waren sha256-gleich** — null von 921 600 Pixeln verschieden. Genauso zwei Bilder bei 0 % und
100 % Bewölkung: der Geländeausschnitt **byte-identisch**, weil der Boden nicht wusste, dass eine
geschlossene Decke zwischen ihm und der Sonne stand.

An der Stelle lag `FB_AP`: eine vollständige, seit dem 23.07. abgeschaltete Luftperspektive. Sie ist
gelöscht, Schalter und Block. Der Grund ist nicht, dass sie kaputt war — sie war ein **Klarluftmodell**
aus der Rayleigh/Mie-Tabelle und konnte das Wetter prinzipiell nicht sehen. Ersetzt durch
`render/stages/FBAtmoHaze.h`: σ₀ = 3,912/Sichtweite (Koschmieder), ausgedünnt mit 8 km Skalenhöhe,
Einstreufarbe aus derselben Himmelstabelle — **eine Funktion, von beiden Shadern gespliced**, C++-Hälfte
danebengelegt und von `--cloudcheck AIR_RESULT` gegen ihren Shader-Zwilling gemessen (max |Δ| 1,19·10⁻⁷).
Danach: 100 % der Pixel verschieden, +57,6 % Helligkeit im Nahband zwischen 5 und 80 km.

Die Beleuchtung unter der Decke ist **keine Schattenkarte**, sondern der Anteil der Sonnenstrahlen, der
die Decke verfehlt: `(1−cover) + cover·exp(−τ·frac)`, pro Deck und pro Fragment. `frac` ist der Anteil
des Decks über dem Fragment — und genau der macht die Messung, die den Ansatz belegt: unter der Decke
**−30,8 %** Helligkeit, auf den Gipfeln über ihrer Obergrenze **+0,8 %**. Der richtungsabhängige
Direktanteil der Bodenstrahlung fällt von 0,882 auf **3,6·10⁻⁶**. Kosten: +0,36 ms fürs Gelände,
+0,29 ms zahlt der Wolkenpass für die geteilte Einstreufarbe — +4,1 % auf den Frame.

**Zwei Funde, die teurer sind als der Umbau.** Erstens: ein WGSL-Übersetzungsfehler
(`step(f32, vec3f)`) hat die Terrain-Pipeline still ungültig gemacht — das Bild sah plausibel aus, es
war nur der Himmel ohne Gelände. Seitdem bricht das Aufnahmeskript bei jedem `gpu_error` ab, statt ihn
zu protokollieren. Zweitens, und das ist die eigentliche Rechnung: **8 km ist die ISA-*Dichte*-Skalenhöhe,
Aerosol dünnt rund sechsmal schneller aus.** Mit den 24,1 km der Fixture überträgt Gelände 13,9 km unter
dem Flugzeug **0,275** bei H = 8000 m gegen **0,946** bei H = 1200 m. Das ist der Unterschied zwischen
„Neuenburgersee durch die Lücken" und Milchglas — und `p1` ist jetzt Milchglas. Die Konstante war
vorgegeben und wird geteilt wie verlangt; die Zahl steht als Lücke 5.7, nicht als Kommentar.

## 2026-07-29 — Zwei Skalenhöhen: der Dunst hört auf, ein Milchglas zu sein

Die Vorgabe der Runde davor war falsch, und zwar meine: **8 km ist die Dichte-Skalenhöhe der Luft, nicht
die des Aerosols**, das bei 24,1 km gemeldeter Sicht 92 % der Extinktion stellt. Der Mechanismus blieb —
eine Datei, beide Shader spleißen sie —, nur das Gesetz darin ist jetzt eine **Summe zweier Terme**:
molekular mit 8 000 m, Aerosol mit 1 200 m. Beide Zahlen sind veröffentlicht und stehen ohnehin schon im
Renderer: `FBAtmoCommon.h` baut seine Himmelstabelle mit `exp(-h/8.0)` und `exp(-h/1.2)` (Bruneton &
Neyret 2008; die 8 km zusätzlich Bucholtz 1995, die 1,2 km zusätzlich Elterman 1968).

**Die Aufteilung ist hergeleitet, nicht gestellt.** Die molekulare Extinktion sauberer Luft ist eine
Naturkonstante — dieselbe, mit der die Himmelstabelle rechnet, 1,3558·10⁻⁵ /m bei 550 nm, also 288 km
Rayleigh-Sichtweite. Also ist der molekulare Anteil fest und das **Aerosol trägt den Rest**: 8,4 % zu
91,6 % bei 24,1 km, 27,7 % bei 80 km, bei über 288 km ist der Aerosolterm exakt null. Bei z = 0 und
550 nm summieren sich beide **genau** auf σ₀ — die gemeldete Sichtweite bleibt unangetastet, gemessen:
T = 0,0200 auf 24,1 km horizontal, vorher wie nachher.

Zahlen am `p1`-Kamerastand: Gelände 13,9 km entfernt **0,274 → 0,853**, die Decke 9 km entfernt
**0,499 → 0,935**. Der Neuenburgersee steht wieder in der Lücke; die Luminanz-Struktur in den drei
Lückenausschnitten stieg um Faktor 2,4 / 6,8 / **26,5** (der letzte war vorher ein toter Wisch, σ = 0,34).

**Und die Kanaltrennung fiel als Nebenprodukt an.** Ein getrennt geführter molekularer Term kann sein
eigenes λ⁻⁴ tragen — die Koeffizienten stehen schon da, ihre Verhältnisse SIND das Gesetz. Am
`p1`-Standort transmittiert Rot 0,908 gegen Blau 0,730. Lücke 5.8 ist damit zu, aber mit einer Korrektur
ihrer eigenen Prämisse: die Extinktion rötet, das **Bild wird trotzdem blauer** mit der Entfernung, weil
der Kanal mit dem größten Transmittanzverlust am meisten von einer himmelblauen Einstreuung dazugewinnt,
die heller ist als das Gelände dahinter. Deshalb sind ferne Berge blau. Gegen ein Kontrollbinary, das
sich NUR im grau erzwungenen Kanal unterscheidet: 99,5–100 % der Pixel verschieden, max 35/255.

Kosten des zweiten Terms: **+0,050 ± 0,027 ms** auf den reinen Geländeframe, auf dem vollen 17-ms-Frame
nicht vom Rauschen zu trennen (|Δ| < 0,4 ms). Drei zusätzliche `exp` pro Fragment, mehr ist es nicht.

Der eigene Vorschlag der Vorrunde ist mitgezogen: `capture_cloud_proofs.sh` **bricht** bei jedem
`gpu_error` ab statt zu protokollieren, und `VERIFY=1` nimmt den Satz zweimal auf und scheitert an jedem
Frame, der sich bewegt hat — 12/12 byte-gleich. Beim Bauen dieser Sperre gleich der nächste Fund
derselben Sorte: `grep | grep -q` liefert unter `pipefail` das SIGPIPE des ersten greps, ein `if` liest
einen echten Fehler dann als „kein Fehler". Die erste Fassung der Abbruchprüfung ließ deshalb sieben
kaputte Frames durch. Prüfvorrichtungen brauchen ihre eigene Prüfvorrichtung.

## 2026-07-29 — Achtzehn Katalogzeilen flogen, keine konnte schießen: die grobe Feuerleitung

Der Befund kam aus O5 und war schärfer als er aussah: eine `f15c` mit vier AIM-120 erfasst bei 18,64 sm,
hält `eng_locked=1` über 28 Sekunden bis auf 8,8 sm herunter — und drückt nie. Ursache eine Ebene tiefer
als die Mission vermutete: `FBAirModule` komponierte **keine** Feuerleitung, `FBState::FireControl` wurde
für keine der achtzehn Zeilen je geschrieben, und alle drei Freigabetore von `FBPilot` lesen genau diesen
Block. Vier Zeilen waren `ACCEPTED` — als **Flugmodelle**. Kein Eintrag war ein Kämpfer.

`modules/air/FBAirFireControl` ist bewusst kleiner als die der F-16. Drei Produkte mit je einem
benannten Leser: die Startzone (Freigabesperre + Schusstor), die Zielschätzung (der Midcourse-Uplink —
**und damit die Bindung**), die Kanonenlösung. Weggelassen und begründet: die ganze Luft-Boden-Hälfte
(keine Katalogstufe akzeptiert `attack`, kein Katalogradar sieht den Boden), die Steuerpunkt-Entfernung
(ein Anzeigeprodukt für ein Cockpit, das eine Katalogzelle nicht hat). Der Trichter wurde von zwei
Entfernungen auf zwei **Flugzeiten** verallgemeinert — 600/3000 ft durch die 1030 m/s der M61A1 sind
0,178 s / 0,888 s, und dieselben 0,888 s ergeben an der GSh-301 764 m gegen deren belegte 800-m-Grenze.
Eine Zeile bekommt die Feuerleitung genau dann, wenn sie eine Waffe deklariert: zehn Decks ja, alle acht
Mover nein. Der Tanker hat keinen Rechner, und das ist in einer Spalte prüfbar.

**Ob eine Runde ihren Schützen bindet, ist eine Ladungs- und keine Zelleneigenschaft.** Dieselbe F-15C,
dieselbe Geometrie, vier `store`-Zeilen Unterschied: AIM-7 `ttaS = −1`, 39,2 s an das Ziel gefesselt, bis
zum Einschlag; AIM-120 `ttaS = +8,25 s`, nach 8,6 s frei. Beide töten (Fehlabstand 1,72 m / 0,905 m). Was
ein **Abbruch** der Führung kostet, zeigt dieselbe Mission, die den Defekt aufzeichnete: die MiG-23
schießt bei 20,78 km, verliert die Beleuchtung 16,9 s in einen 26,8-s-Flug hinein — die Runde kommt bei
nichts an. Und zwar aus einem belegten Grund: das ±6°-Elevationsfeld des N003E kann einen 1000 m höher
fliegenden Bomber im Anflug nicht halten.

Auf dem Weg dorthin ein zweiter Fund derselben Sorte wie `pilot.md` 2.15, nur in der anderen Achse:
`set brief_gci 90` ist eine **rechtweisende** Peilung und wurde als körperbezogener Azimut gepostet. Die
±30°-Antenne der MiG-23 stand damit 90° neben der eigenen Nase, und die Abfangmaschine kommandiert nur
Elevation — sie kam nie zurück. Vorher null Radarkontakte, nachher erster Kontakt bei 27,37 sm, an einer
unveränderten Missionsdatei. Ein gebriefter Anruf sind zwei Zahlen im Rahmen des LOTSEN, und beide müssen
gegen die eigenen Instrumente umgerechnet werden.

Was **nicht** geht, gemessen statt behauptet: `mig17` und `su7` — die zwei reinen Kanonenzeilen — haben
kein Radar, eine Kanonenlösung braucht eine Entfernung, und das Auge veröffentlicht eine Winkelgröße und
keine (A14; der Mechanismus heißt Kreiselvisier mit eingestellter Spannweite und ist benannt, nicht
gebaut). Und mit dem per Rezept-Schritt 8 eingetragenen, gemessenen Rollwerk feuert eine `mig21` zwar
ihre GSh-23L (vier Treffer, `structure degraded`) — danach greift sie nie wieder an, braucht 76 s für
3,0 → 0,9 sm und sinkt aus 5000 m in den Boden (A15). Eine Katalogzeile kann ihre Kanone **abfeuern** und
nicht mit ihr **kämpfen**. Keine Kampagne darf ein Kanonenduell werten.

## 2026-07-29 — Kampagne O2 gebaut und geflogen: der Lotse ist alles wert, wenn der Jet still startet

Zehn `sim/missions/o2-*.fbm` plus `sim/campaigns/o2-pvo-intercept.fbc`, vierte der zehn Kampagnen, kein
`sim/src/`, kein Werkzeug, kein Asset angefasst — 11 neue Dateien, 0 geänderte, also sind die 173
bestehenden Missionen byte-identisch **per Konstruktion**. Beide Determinismus-Kriterien beim ersten
Versuch: 9 Läufe ein Fingerabdruck (`93b5869298b6b8a5924…`, `--elev const`), 10/10 Schritte replayen
allein. Kampagnen-Exit 3, Schritt-Exits `3 0 3 3 3 0 0 0 3 3`.

**Die Schleife ist 11,0 s und sie teilt sich 8,0 + 3,0.** `gci BRAA` → drei getippte Eingaben →
`n019 EMISSION` = 8,0 s, und das reproduziert `mig29-intercept.fbm` auf einer anderen Geometrie, ist also
eine Eigenschaft der Eingabekette. Von dort bis zum festen Track: 3,0 s, ein RAD-Rahmen. Der Preis steht
auf dem anderen Jet: dessen RWR meldet `kind=fire-control` **0,1 s** nach der Emission.

**Und damit dreht sich der Befund dreier Vorkampagnen um.** O1 und O5 haben den Lotsen dreimal bei null
gemessen — weil in allen dreien `set n019_emission illum` beim Spawn stand: die gelöschte Peilung ließ
die Antenne **falsch gerichtet** zurück. O2 fliegt die dokumentierte Einschaltstellung `off`, und dort ist
die dritte getippte Eingabe das Einzige im Baum, das das Radar überhaupt anschaltet. Falscher
Azimut-Sektor: **0 Kontakte in 400 s**. Lotse gelöscht: 0 Emissionen, 0 Kontakte, und der Eindringling
erfährt nie, dass jemand da war. Später Einsatzbefehl: **45,3 s Schweigen gekauft, 77 % der
Erfassungsreichweite bezahlt**, null Schüsse gegen zwei Treffer. Die vergleichbare Größe über alle vier
Kampagnen ist nicht *"was der Lotse wert ist"*, sondern *"was er bei gegebener Emissionspolitik wert
ist"* — drei Dateien maßen die Richtung, diese misst die Existenz.

**Die Identifizierungs-Gegenprobe hält in der starken Form.** `o2-06` gegen `o2-08`, ein Token
Unterschied (`team neutral` → `team friendly`): **5 von 5 `telemetry*.csv` byte-identisch**, `events.log`
in **genau einer Zeile von 53** verschieden — `mission UNIT_RESULT … team=`, geschrieben vom Runner,
lesbar von keinem simulierten System. Vier Wahrnehmungskanäle liefen 300 s lang (N019 mit Abfrager, KOLS,
Auge, SPO-15), keiner bewegte sich. Es gibt keinen ersten Diskriminator, bis zu dem man identisch sein
könnte: IFF Mode 4 ist zweiwertig, ein Fremder und ein Feind sind dasselbe Schweigen. **Ein solches Paar
braucht eine dritte Datei** — `o2-07`, derselbe Anflug mit einem Kontakt, der ANTWORTET: zwei Logzeilen
Unterschied, null Telemetriebytes auf dem Abfangjäger. Ohne sie hätte "identisch" zwei mögliche Ursachen.

**Zwei CIA-Dokumente, seit Lauf 1 als „höchstwertige ungelesene Quelle des Verzeichnisses" geführt,
gelesen.** Der `cia.gov`-Pfad ist Akamai-geblockt (302 → *Access Denied*); die Wayback-Aufnahmen
derselben URLs sind es nicht. Mitronin (Warschauer-Pakt-Journal 12/1976) trägt die Kampagne: zwei Formen
der Zusammenarbeit, fünf Zuteilungswährungen, die **Identifizierung als das zentrale Problem** ("sonst
müsste die Feuerfähigkeit der Flaraketenverbände wegen der Gefahr, eigene Flugzeuge zu treffen,
eingeschränkt werden"), die Korridore für eigene Flugzeuge — und **10 bis 15 Minuten** (DRUŽBA-76), um
die eigene Luftlage über eine Codetabelle bis zum Richtschützen zu bringen. Damit hat `C6` eine Zahl:
FlightBox misst eine **Cockpit**-Schleife mit der Stoppuhr und einen Gefechtsstand mit gar nichts.

Drei Funde, keiner behoben. (1) **Ein falscher Brief hat keine absichtliche Korrektur** — was danach
aussieht, ist das 2,0°-Totband von `FBPilot`s eigenem Suchgesetz, das driftet; es rettete eine von zwei
Maschinen, 28 s zu spät, die andere schaute 400 s lang 7,5° über ihr Ziel. (2) **D3 als Byte-Differenz
bepreist**: `set kols_mode ir` über fünf Einsätze ändert **4 von 184 Telemetriespalten und sonst nichts**
— während genau dieser Sensor 90 s lang einen Kontakt hielt, den niemand las. (3) **Eine R-27R innerhalb
ihres eigenen Zünders ist kein Abschuss**: 4,85 m und 4,75 m in die Vorderzone ließen das Ziel
kampffähig, 2,48 m anderswo töteten. Eine Ergebnisachse nach Abschüssen liest Sprengkopfgeometrie.

Und der Elevations-Defekt aus `pilot.md` 2.15 wurde auf der **anderen** Seite seiner Schwelle vermessen:
der Fehler ist exakt `st.pitch`, das Steigflug-Nickband liegt bei **5,36…5,89°**, der RAD-Balken bei
±6,0° — Rand **0,11–0,64°**. Beißbedingung: `|pitch| + |Zielelevation im Körperrahmen| > 6,0°`.

## 2026-07-29 — Kampagne W5 gebaut und geflogen: die Aufgabe hing an einer Besetzungszeile, nicht am Auge

**W5 Baltic Air Policing / QRA** ist die fünfte gebaute Kampagne, die erste, in der die **F-16** fliegt,
und die einzige der zehn, deren Siegbedingung **keine Waffe** enthält. Zehn `sim/missions/w5-*.fbm` plus
`sim/campaigns/w5-baltic-qra.fbc`; **keine Datei unter `sim/src/`, `sim/tools/` oder `sim/assets/`
angefasst**, elf neue Dateien, null geänderte. Kampagnen-Exit 3, Schritt-Exits `0 0 0 0 0 0 0 0 0 3` —
die erste Kampagne, deren Einsätze überwiegend ein **echtes Urteil** liefern statt eines Messstands, weil
`identify` + `no_fire` (Runde `C12`) für genau diese Aufgabe gebaut wurden.

**Die eigene Überschrift der Spec war falsch, und die Messung sagt warum.** Sie führte W5 als „die
Kampagne, deren Gegenstand FlightBox nicht simulieren kann", weil es kein Auge gab. Das Auge existiert
seit dem 28.07. — und es war trotzdem nicht das Entscheidende. Entscheidend ist die **Spannweite des
Gegenstands**: eine An-26 wird mit dem Auge bei **1 086 m** identifiziert, eine Tu-95 bei **2 049 m**,
während zwei MiG-29 im selben Verband bei 1 600 m nicht einmal *erkannt* werden. Ein Auflösungsgesetz,
zwei veröffentlichte Maße. Der Blocker war eine **Katalogzeile** (`an26`, ein MOVER — und ein Mover hat
kein generiertes Deck, also greift `C7`s `ALPHA`-Urteil nicht), kein Sensor.

**Die Gegenprobe, dreiläufig von Anfang an geplant.** `w5-02` gegen `w5-03`, ein Token Unterschied:
**6 von 6 `telemetry*.csv` byte-identisch, 1 abweichende `events.log`-Zeile von 75** — das `team=`-Feld
des Runners. Der Kontrolllauf `w5-01` (der Gegenstand ANTWORTET) bewegt **5 von 184 Telemetriespalten
und null Meter**. Das ist ein **Widerspruch zu O2**, wo derselbe Versuch null Spalten bewegte, und
Regel 11 löst ihn auf: die F-16 fliegt im Verband, `FBFlightPicture` sortiert über Tracks mit
IFF-Feld, ein `friendly` antwortender Track wird nie zugewiesen. Gemessen wird also *„was eine Identität
einem VERBAND wert ist"*. Auf beiden Flugzeugen gilt: **kein Meter Flugweg bewegt sich.**

**Was eine Identifizierung kostet** (40-km-Nachlauf, 900 s): **412,9 s und 243,5 lb** bis zur visuellen
Identifizierung, **494,3 m** Annäherung, **null Risiko** — ein Nachflug wird in genau die Richtung
geflogen, in die ein vorwärtsblickendes Radar nicht zeigt: der Gegenstand strahlt ab t=0, seine Keule
erreicht den Abfangjäger erst bei t=201,3, **30 s NACH** dem gerasteten Urteil. Der Anflug vom Platz
steht nicht in der Rechnung (`C6`).

**Vier Funde, keiner behoben.** (1) **Im Spawn-Tick meldet jeder Radarwarnempfänger die WAHRE statt der
relativen Peilung** — die Lage, gegen die transformiert wird, ist noch nicht publiziert; isoliert
gemessen **180° gegen −95,5°** bei identischer Geometrie, Fehler **275,5°**, 2,0 s gehalten,
vorbestehend und im eingecheckten `pair-2v2-f16.fbm` sichtbar. (2) **`FBPilot` hat für diese Aufgabe
kein Verhalten**: `FBPilot.cpp:1040` nennt *innerhalb 5,0 nm und nie geschossen* einen ABBRUCH — also
genau die Identifizierungsgeometrie; mit `set task intercept` dreht der Abfangjäger bei t=5,1 s weg.
Jeder Anflug in allen zehn Dateien ist eine von Hand geschriebene Route. (3) **Ein Verband kann zwei
Ziele nicht sortieren, die jeweils nur ein Mitglied sieht** — `FBFlightPicture::Assign` matcht alle
Mitglieder gegen die Kontaktliste des *rechnenden* Flugzeugs, also melden beide Jets `dup=1` über zwei
Maschinen in 17,8 km Abstand. (4) **Die Führung krabbt nicht**: ein 176-km-Bein biegt **3,4 km** nach Lee
und verfehlt eine 2-km-Box um 4 038 m, wo dieselbe Maschine auf 34-km-Vektoren bei 677,7 m identifiziert.

**Und der akzeptierte Preis wurde öffentlich bezahlt.** `doc/missions/verdict.md` urteilt über die
GEOMETRIE statt über das Sensorereignis und nennt den Preis: „wer die Box mit geschlossenen Augen
fliegt, punktet". Der Nachteinsatz hat **0** `vis`-Zeilen gegen 9, unterscheidet sich in **6 von 184**
Spalten — und liefert `mission IDENTIFIED` beim **identischen Tick, Bereich und Verweilwert**. Genau der
Preis, kein Cent mehr.

Beide Determinismus-Kriterien beim ersten Versuch: 9 Läufe eine Kampagnen-Signatur
`49d3320f5e9761db2f1df85a12d9008e0d8559395c141c31e2e06903b9fe0200`, 10/10 Schritte standalone
bit-identisch — **und der `replay` lief diesmal nach der ERSTEN Mission**, auf einer
Wegwerf-Ein-Schritt-`.fbc`, was zwei Vorrunden eingestanden hatten zu versäumen.

## 2026-07-29 — Kampagne W3 gebaut und geflogen: der Wert eines Hebels ist eine Eigenschaft der Topologie

Zehn `sim/missions/w3-*.fbm` plus `sim/campaigns/w3-desert-storm.fbc` — die sechste der zehn Kampagnen,
und die erste, deren Gegner ein **System** ist statt eines Flugzeugs. **Keine Datei unter `sim/src/`,
`sim/tools/` oder `sim/assets/` angefasst**: elf neue unversionierte Dateien, null geänderte, also sind
die 195 bestehenden Missionen bit-identisch per Konstruktion.

**Die eigene Schlagzeile der Spec ist widerlegt.** Sie sagte: *„Von den drei Dingen, die an Package Q
schiefgingen, kann FlightBox heute NULL messen."* Regel 7 — jeder Blocker gegen den **Baum** geprüft,
nicht gegen eine Statuszeile: `C1` `C8` `C22` `C23` `C24` `C26` `C27` `C2` `C0` sind seither geschlossen.
**Zehn von zehn Missionen liefen**, und von den drei Fehlerarten kann FlightBox **genau eine** stellen —
die Kampagne sagt welche und warum. Fehlerart 2 (die Weasels gingen früh) ist vollständig stellbar.
Fehlerart 1 (Tanker/Sprit) ist **doppelt** blockiert: `C5` blockiert die Ursache, und darunter blockiert
ein unerreichbarer Zweig die Wirkung. Fehlerart 3 (der gesättigte Funkkanal) ist gar nicht stellbar, weil
es keinen Kanal mit Kapazität gibt (`C18`); Einsatz 09 sagt das und misst stattdessen das einzige
sättigbare Kommandoobjekt im Baum.

**Was ein Unterdrückungselement wert ist — vier Läufe auf einer Geometrie.** Drängt es auf 20 km vor,
trifft die AGM-88 die S-125 auf **2,8 Millimeter** und alle Bomber kommen durch. Schießt es aus 42 km,
fällt die Runde **10,5 km zu kurz** — und die Batterie feuert trotzdem ihr ganzes Magazin auf die
**abdrehenden** Weasels, also kommen die Bomber ebenfalls durch. Erst ohne SEAD (Zuordnungslauf A2)
kippt es: vier V-601 zwischen 5,86 und 7,99 m auf einen Bomber, **1 von 2** erreicht den Auslösepunkt,
das Ziel bleibt stehen. **Das Element ist eine Auslösung und das Ziel wert — auch dann, wenn seine
Rakete 10 km zu kurz fällt**, denn eine FlightBox-Batterie hat keinen Freund-Feind-Abfrager und keine
Bedrohungsrangfolge und verteilt ein endliches Magazin auf das, was in Reichweite ist.

**Was Emissionsdisziplin wert ist.** Bei **57,4 %** der 52,08 s Flugzeit dunkel: die AGM-88 rollt aus und
schlägt **214 m** vor der Stellung ein, die Stellung lebt. Sie kommt bei t = 200 zurück und verschießt
ihr Magazin auf 25,7–34,3 km — wieder auf die abdrehenden Weasels, null Ankünfte. **Sie ist die Stellung
wert und sonst nichts**: wer einer HARM ausweicht, hat sich selbst 170 s lang unterdrückt.

**Und derselbe Hebel, zweimal gezogen, ist zweimal etwas anderes wert.** Eine Mk 84 auf denselben
Frühwarnradar: gegen ein Netz mit EINEM Knoten die ganze Operation (0 Starts gegen 10; 2 von 2 Bomber am
Auslösepunkt gegen 0 von 2), gegen ein Netz mit einem zweiten Knoten **9 von 25 Einweisungen und sonst
nichts** — dieselbe Datei als Kampagnenschritt und standalone, 30 von 58 Telemetriedateien bit-identisch,
keine einzige Bahnspalte bewegt. Das ist Regel 11 eine Schicht tiefer und die direkte Einschränkung von
O5s *„eine Mk 84 auf die P-18 kostet die Flugkörperschicht zwei Nächte"*: **O5s Platz hatte einen Knoten.**

**Regel 11 diesmal auf beiden Seiten gemessen.** `w3-07`/`w3-08` unter der vorab erklärten Politik
`n019_emission off`: **50 rote Radarkontakte und 5 Startlösungen gegen 0 und 0**. Zuordnungslauf A3,
dieselbe Datei mit `illum`: 50 Kontakte, 7 Lösungen, Lauf endet beim **identischen** t = 272,8 s wie die
gebriefte Kontrolle. Der Fünf-Theater-Widerspruch ist damit gemessen statt geerbt.

**Vier Funde, keiner behoben.** (1) **`FBPilot::CanPressOn` ist unerreichbar** — die einzige Zeile im
Piloten, die die BINGO-Warnung liest, hängt an `EngState_ == Defend && elapsed >= DefendHoldS`, und der
allgemeine Zweig `else if (EngState_ != Abort)` nimmt den Zustand im ersten Takt weg, nachdem `defendDue`
fällt. Gemessen: das Bit über **5 200 von 5 200** Zeilen gesetzt, `eng_state` bit-identisch zum Lauf ohne
Brief, sieben von 184 Spalten Unterschied und null Meter. (2) **Ein Näherungszünder hat keinen
Team-Test** — bei 24 Flugzeugen detonierte `qamia1`s R-27R **11,74 m** neben einer MiG-29 der anderen
roten Rotte und tötete sie; 1 von 3 Verlusten des Schlusseinsatzes ist eigenes Feuer. (3) **`C15` hat
jetzt einen Preis**: drei von vier AGM-88 einer Viererrotte gingen in dieselbe Batterie, zwei davon,
nachdem sie schon tot war. (4) **Eine Batterie hat keine Bedrohungsrangfolge** — viermal gemessen, in
drei verschiedene Richtungen.

**Und die Zahl, die eine Schlagkampagne wirklich braucht:** ein Bomber wird in diesem Baum häufiger von
**Systemschaden** gestoppt als von Zerstörung. `w3-02`s zweite Welle nimmt vier V-601 innerhalb 8,4 m,
überlebt alle vier und steht 56 s später über ihrem Ziel mit elf ausgefallenen Systemen, `stores`
darunter. Als Verlustliste gelesen: 0–0. Als Paketergebnis gelesen: Totalausfall.

Beide Determinismus-Kriterien beim ersten Versuch: 9 Läufe eine Kampagnen-Signatur
`3490c4fab3f25f533ead565e393cc23d234067e827e5ea7ba733408988f1fa1a`, 10/10 Schritte standalone
bit-identisch — und der `replay` lief nach der **ersten** Mission, auf einer Wegwerf-Ein-Schritt-`.fbc`.
Der Schlusseinsatz fliegt **24 Flugzeuge + 8 Bodenobjekte + 34 Waffen** in 11,7 s Wanduhr, **8 von 8**
Bombern am Auslösepunkt und **15 von 16** zurück.

## 2026-07-29 — Kampagne W4 gebaut und geflogen: die Höhenuntergrenze steht über der Decke ihrer eigenen Waffe

Siebte von zehn, zehn `.fbm` + eine `.fbc`, nichts unter `sim/src/`, `sim/tools/` oder `sim/assets/`
angefasst (`git status --porcelain`: elf neue, null geänderte Dateien), also sind die 205 vorhandenen
Missionen bauartbedingt byte-identisch. Die Spec nannte **vier** von zehn flugfähig und vier Missionen
auf `C1` blockiert; nach Regel 7 gegen den heutigen Baum geprüft liefen **zehn von zehn**. Zwei
Spec-Missionen wurden nicht blockiert, sondern **verworfen** — das Gebirgstal (`C4` + `--elev const`:
es gibt kein Tal) und der Wetterabbruch (`FBPilot` hat keinen Zweig, der die Auslösung verweigert, dieselbe
Form, die W3 an der BINGO-Warnung gemessen hat) — beides mit Begründung im `.fbc`-Kopf.

**Der zentrale Fund ist eine Kollision zweier Ankertatsachen.** Beide sind belegt: NATO flog mit einer
harten Untergrenze von 15 000 ft, und die F-16CJ trug die AGM-88. Auf einer Neun-Punkte-Leiter (ein
`p18`, eine Runde, 20,0 km, Abschusshöhe die einzige Variable) trifft die Runde von 3 000 bis 4 150 m
(0,009–4,47 m), verfehlt bei **4 200 m um 74,8 m** und bei **4 572 m um 2 484 m** — und der letzte
FRISCHE Blick jedes fehlschlagenden Schusses liegt bei exakt **15,00°**, gemessen an der Stellung: der
publizierten Elevationsabdeckung des P-18 (`SearchElCenterDeg 5 + SearchElHalfDeg 10`). 15 000 ft =
4 572 m. **Die Untergrenze liegt 372–422 m über der Decke ihrer eigenen SEAD-Waffe**, also fliegt jeder
Weasel dieser Kampagne auf 3 000 m und schreibt es in seinen Kopf.

**Was ein Verteidiger gewinnt, der grundsätzlich nicht strahlt — eine Geometrie, ein Hebel, drei
Stellungen.** Knoten strahlt: nach 66,5 s tot. Alles auf `emcon hold`:
`site RADIATE`/`TRACK`/`LAUNCH`/`net CUE` **3/2/4/4 → 0/0/0/0**, Stellungsverluste 1 → 0. Knoten plus
drei `p18`-Attrappen: beide AGM-88 sterben auf einer Attrappe (0,019 m und 4,75 m), der Knoten lebt,
`net CUE` **26 gegen 4**, der Gürtel verschießt **7 statt 4** Runden. **Und keine der drei Politiken
bewegt den Angriff** — dieselben zwei Bomber, dieselben Takte, dasselbe `aimErrM`. Die Doktrin ist die
Stellung wert und sonst nichts; die Attrappe kauft dieselbe Überlebensfähigkeit und behält das Gefecht.

**Der Radarköder funktioniert und kostet nichts.** `cast.md` veranschlagte ihn als *"`p18` mit
`rounds 0` und kleinem Tor"* — beides falsch: ein `p18` hat ohnehin `Channels 0`, ein Suchreichweiten-Key
existiert nicht, und was ihn wirken lässt, ist gerade, dass er dieselbe Zeile ist wie der Knoten
(`1 − (r/2R)²`, also gewinnt schlicht der nächste). Zwei Grenzen sind gemessen: die `arm_class`-Sortierung
ist binär, und unterhalb von `Höhe/tan(15°)` — **17,1 km von der Untergrenze aus** — ist eine Attrappe
überhaupt nicht hörbar.

**Vom Schlechtwetter ist der WIND gemessen, die Decke ist Kulisse.** Sechs-Punkte-Leiter auf einer Datei:
**5,014 m Bombenfehler pro Knoten** auf 4 572 m; die 46 kt der Fixture kosten 216 m, und jeder
`wx fixture`-Angriff dieser Kampagne verfehlt; 20 kt Seitenwind machen aus **3 von 6** Treffern **0 von
6**. Die Wolke wird genau **einmal** gemessen, am Auge: `vis MASKED … transmittance=8,00571e-13`.
`irst_masked` ist in allen zehn Dateien 0, und eine reine Angriffsdatei protokolliert überhaupt keine
`vis`-Zeile.

**Drei Funde, keiner behoben.** (1) Eine halbaktive Batterie, die einen **Schienen**-Nachladevorgang
beginnt, verwaist jede Runde in der Luft — beim dritten Start geht die 2K12 in `RELOAD`, der Beleuchter
schweigt, 0,2 s später melden alle drei 3M9 `ILLUMINATION_LOST`, Runde 2 **1 776,6 m vor dem Ziel nach
27,1 s Flug**; mit einem `set rounds 4`-Kontrolllauf zugeordnet, der beim identischen Takt verliert. (2)
`objective suppress … emitting <s>` liest **MET mit `emittingS=0`** — es unterscheidet nicht, ob wir sie
niedergehalten haben oder ob sie nie an war. (3) Der „erstes zulässiges Symbol"-Rastvorgang der
Antiradarwaffe hat kein Gedächtnis für ihr Startziel: gegen einen verteilten Gürtel rastete sie
**sechsmal in 11 s** auf vier Symbole, das letzte 36,7° neben der Nase.

Beide Determinismus-Kriterien beim ersten Versuch: 9 Läufe eine Kampagnen-Signatur
`6185addc27ec3ef896cd1aed4750d7a6bdf8555f9a3a1e2c6b12971533b8d80a`, 10/10 Schritte standalone
bit-identisch — und der `replay` lief nach der **ersten** Mission. Der Übertrag ist eine Rufkennung
(`kosnod`, 08 → 10) und **25 % des Meldeverkehrs** wert (60 gegen 80 `net CUE`), sonst nichts: 21 von 41
Telemetriedateien byte-identisch, die übrigen 2–6 von 184–202 Spalten, alle sieben RWR- oder
Datenlink-Buchhaltung, **keine Bahnspalte bewegt sich**. Und `--elev tiles` über das echte Kosovo
(Boden 547,88 m) verschiebt `predErrM` 58,08 → 46,50 m und erzeugt **null Maskierungen** — die fehlende
Hälfte von `C4` ist eine Rechnung, keine Datenlage.

## 2026-07-30 — Der Direktor der MiG-29: ungenauer als der Rechner, und genau das war der Nachweis

`C9` war die einzige Lücke, die eine ganze Kampagne auf null setzte, und sie hieß nicht „CCIP fehlt"
sondern etwas Genaueres: das Abwurfverfahren der MiG-29 ist ein **Direktor** und kein Auslösecue —
das Flugzeug wählt den Moment, der Pilot fliegt eine Anweisung. Gebaut als `core/FBDirector.h` plus
`modules/mig29/FBMig29Director.*`.

Die Abnahme war deshalb nicht „es wirft ab", sondern die eine Messung, die eine Abkürzung entlarven
kann: **dieselbe Geometrie, dieselbe `fab500` auf beiden Seiten** (damit die Ballistik keine Variable
ist), F-16 im Rechnerverfahren **34,02 m** gegen MiG-29 im Direktor **65,65 m** — Faktor 1,93. Ein
Direktor, der *besser* abschneidet als ein Rechner, wäre das Verfahren der F-16 mit kyrillischen
Beschriftungen gewesen. Er tut es nicht.

Die Verweigerung ist ein eigener, belegter Fall und keine geschriebene Regel: der Abzug darf erst 1–10 s
nach der Entfernungsmessung gedrückt werden, der Ablauf danach muss lang genug sein, um den Abwurf zu
fliegen — also muss die Messung mehrere Kilometer vor dem Abwurfpunkt liegen, und der einzige
Entfernungsmesser dieses Flugzeugs reicht 6 km. Ein Anflug, der dafür keinen Raum lässt, wird
abgewiesen: `mig29-opt-refused.fbm` fliegt den Grenzfall (löst aus, 102,9 m) und den verweigerten Fall
(löst **gar nicht** aus) in einer Datei.

Byte-Identität hält: eine MiG-29 ohne Angriffsauftrag bewegt sich nicht, Sammelhash über zwölf
bestehende Missionen gleich, Spaltenzahl unverändert bei 184. `verify-layers` 304 Dateien, sechs
Registry-Leser, eine Antennenbefehls-Stelle.

Zwei Ehrlichkeiten zum Zustand: die drei Beweismissionen brauchen `--elev const`, weil sie auf 300 m
spawnen und das Schweizer Geländemodell dort 492 m hoch ist — die Runde starb zweimal an Serverfehlern,
bevor sie das prüfen konnte, und es ist beim Nachfahren aufgefallen. Und O3 ist damit **am Modul** nicht
mehr blockiert, wohl aber an ihrer Besetzung: die Zeitgenossen sind ALPHA und dürfen keine
Kampagnenfrage beantworten. Die Zahl der lauffähigen O3-Missionen ist bewusst **nicht** neu gezählt
worden — das ist die erste Aufgabe der O3-Runde selbst.

## 2026-07-30 — Kampagne O3: ein befreundeter Flugkörperschirm ist keine Deckung, er kostet ein Flugzeug

O3 war die einzige der zehn Kampagnen mit **null** lauffähigen Missionen — blockiert nicht an einer
Mission, sondern am Modul (`C9`). Mit dem Direktor von heute Morgen ist sie gebaut: zehn `.fbm`, eine
`.fbc`, beide Determinismus-Kriterien im ersten Versuch (**9 Läufe, ein Fingerabdruck**
`01e4f956…`; **10/10** Schritte reproduzieren standalone), Replay nach der **ersten** Mission, kein
Byte unter `sim/src/` angefasst. Die Zahl lauffähiger O3-Missionen ist damit **10 von 10**, und alle
zehn sind auch beantwortbar.

**Die Frage, die keine andere der zehn stellen kann, hat eine Zahl.** Der Schirm über dem Kanalübergang
gehört *uns*. Über zehn Einsätze: **28 Boden-Luft-Starts, 28 von 28 auf die eigenen Flugzeuge gerichtet**
— zugeordnet über jede `sms LAUNCH_SOLUTION`-Zielkoordinate gegen die Telemetrie beider Seiten, 22–95 m
zur gemeinten MiG-29 und 1,1–10,8 km zum Gegner —, **null** je auf einen Gegner, **null** Gegner je in
einer festen Spur, **eine eigene MiG-29 abgeschossen** (3M9 auf 4,74 m gegen 8 m Zünderradius) und **ein
gegnerischer F-16 mit zehn ausgefallenen Systemen** — getroffen von einer V-601, die auf eine MiG-29
geschossen wurde. Der Mechanismus sind zwei nachprüfbare Sätze: `FBSiteFireControl` enthält überhaupt
keinen IFF-Pfad, und jede SAM-Zeile hat `Channels = 1`. Ein Freund im Bereich **verschlechtert** die
Bekämpfung nicht, er **löscht** sie: die 44 Entscheidungszeilen von `o3-06` sind byte-identisch mit denen
von `o3-04`, das keinen Gegner enthält.

Regel 11 beidseitig geflogen: `wcs hold` (o3-05, ein Token gegen o3-04) kostet **nichts** und behält das
Magazin — und gibt die Fähigkeit auf, überhaupt jemanden zu bekämpfen. Und die zweite Hälfte derselben
Regel: „der Schirm ist harmlos" ist nur um **1,1–5,2 m** wahr. Eine Leiter über 300/1 000/3 000/5 000 m
misst `closestM` 9,07 m gegen 8 m Zünder und 11,4–15,2 m gegen 10 m — dieselbe Batterie, die 27-mal
vorbeischoss, tötete beim 28. Mal, und die Entscheidung darüber lag bei 500 kg unabgeworfener Bombe.

**Der zweite Befund ist nicht der Schirm, sondern das Flugzeug: dieses Muster kann die Operation seines
eigenen Ankers nicht fliegen.** Querabweichung gegen 68,4 m Wirkradius: **+48,3 m** auf 6 km geradem
Endanflug, +87,2 m auf 12 km, +90,9 m auf 24 km — und **48,3 m + 33,9 m je Grad Knick** [abgeleitet,
Drei-Punkt-Leiter]. Größter zulässiger Kurswechsel im Endanflug: **0,59°**. Der Eröffnungsschlag des
6. Oktober waren 220 Flugzeuge auf koordinierten Anflugwegen; so etwas trifft hier nichts. Der Direktor
selbst wird dabei *besser* (`openLoopAlongM` −70,3 → −1,0 m) — es gewinnt die Achse, die gegen einen
Wirkradius gemessen wird.

Drei Funde, keiner behoben: (1) eine bodengestartete kommandogelenkte Runde verfehlt einen tief
fliegenden, nicht manövrierenden Querflieger um **knapp mehr als ihren eigenen Zünderradius** — dritte
sichtbare Schicht der Bodenstart-Familie aus O1, jetzt mit Zahlen auf **beiden** Seiten der Schwelle;
(2) **eine unabgeworfene Bombe auf einer Innenstation kehrt den stehenden Querversatz des Flugzeugs um**,
+48,3 → −39,2 m, ein Schwung von 87,5 m gegen 68,4 m Wirkradius — deshalb ist dieser `carry` der
wirksamste der acht gebauten Kampagnen (**0 von 48** Telemetriedateien byte-identisch, gegen W3s 30 von
58) und **eine unabgeworfene Bombe ist ein Flugzeug wert**; (3) der FAB-Kommentar in `core/FBStore.h`
behauptet weiterhin, die MiG-29 könne `set task attack` nicht fliegen — dreißig Abwürfe später.

Und eine Ehrlichkeit zum Gelände, die gestern zwei Missionen stilllegte und heute Voraussetzung war:
alle O3-Angreifer spawnen auf 300 m, weil ein 6-km-Entfernungsmesser die Wurfhöhe dieses Flugzeugs auf
2,0–2,2 km deckelt. `fb-gym`s **eigener** Standard ist `--elev swiss`, und der prüft eine explizite
Spawnhöhe gegen den aufgelösten Boden. `--elev const` ist für diese Kampagne keine
Vergleichbarkeitskonvention, sondern Bedingung — und steht in jedem Befehl ihres Protokolls.

## 2026-07-30 — Kampagne W1: die Übungsleiter steigt, aber nur ihre Bodenhälfte lässt sich noch benoten

W1 galt als blockiert, weil die Nellis-Aggressoren im Katalog **ALPHA** sind und `A15` kein
Katalog-Kanonengefecht wertet. W1s eigene Pointe löst das: in Nellis ist die „MiG-29" ein verkleideter
F-16 — bei uns ist sie das echte Modul. **Keine der elf Dateien fliegt eine Katalogzeile**, `A15` bleibt
unberührt, und die Richtung der Ersetzung steht in jedem Kopf: dieser Aggressor hat R-73, KOLS und
Helmvisier, die der echte nie hatte, ist also **stärker** als die Vorlage. Zehn `.fbm`, eine `.fbc`, beide
Determinismus-Kriterien im ersten Versuch (**9 Läufe, ein Fingerabdruck** `5de43dd5…`; **10/10** Schritte
reproduzieren standalone), Replay nach der **ersten** Mission, kein Byte unter `sim/src/` angefasst.

**Der seit Lauf 1 mit HTTP 403 vermerkte Faktenzettel ist gelesen** — über die Wayback-Kopie derselben
URL, dieselbe Lehre wie bei O2s zwei CIA-Dokumenten, jetzt auf einem zweiten Host bestätigt. Sechs
Aussagen steigen auf **[T1]**, darunter die Zehn-Missionen-Begründung im Wortlaut: die Missionszahl dieser
Kampagne ist damit die Zahl des Ankers und keine `[SET]`-Wahl. Paketgrößen stehen nicht darin — nur
Summen seit 1975 — und bleiben `[SET]`.

**Die eine Messung, die W1 den anderen neun voraushat, ist ein Fehlschlag mit drei Zahlen.** Gegen das
Sättigungstor aus `doctrine-evolution.md` §4.2, mit den neun deklarierten Doktrinhebeln auf jeder der
zehn Sprossen und auf beiden Sitzen (180 Läufe, zweimal, byte-identisch): `S4` 10 Geometrien ≥ 6 **ok**,
`S6` sauber, `S3` n/a — aber `S5` **2 informative gegen 3 gefordert: VERWEIGERT**. Modale Ergebnisklasse
je Sprosse 100 / 66,7 / 55,6 / 88,9 / 100 / 88,9 / 88,9 / 88,9 / 88,9 / 55,6 %, Hebel, die die Klasse
bewegen, 0/3/4/1/0/1/1/1/1/5 von 9 auf dem F-16-Sitz und 0/1/1/3/0/4/0/2/4/2 auf dem MiG-Sitz. **Die Hebel
beißen in gegenläufigen Sitzen auf gegenläufigen Sprossen** — das ist die Asymmetrie aus `duels.md` auf
handgeschriebenen Geometrien. Und die drei Sprossen, die überhaupt etwas entscheiden, sind genau die drei
mit höchstens zwei Flugzeugen: **ein Luftkampfergebnis über 2v2 ist in diesem Baum ein Fixpunkt**, die
Bodenhälfte entscheidet auf jeder Größe. Damit sind die 4v4-Höhepunkte der acht früheren Kampagnen
ergebnisblind gebaut, und das ist die übertragbare Zeile dieses Laufs.

Was die Leiter sonst gemessen hat: der Abzug fällt auf **0,978 × Rtr** (F-16) und **0,977 × Rtr** (MiG)
und ist **blind gegen ein 1,26-fach längeres Raero** — `duels.md` Zeile 1 auf zwei Dezimalen reproduziert,
auf einer Bahn, die kürzer ist als die eigene Radarreichweite (93,9 km gegen 100,0 km Gate, also gar keine
Suchphase); die CAP der Luftverteidigungssprosse **verhindert nichts** — 11 von 184 Telemetriespalten,
**null Bahnzellen**, und der Kontrolllauf ohne CAP wiederholt den 101,05-m-Fehlwurf auf fünf Dezimalen;
totale Funkstille kostet Blau jeden Schuss und Rot **10 Starts bei 0 Treffern**; ein Flügelmann ohne
eigenes Radar bekommt **gar kein Ziel zugewiesen**; der 18-Sekunden-Schussvorsprung der Aggressoren macht
aus **vier blauen Abzügen einen**; und der Übertrag — Kill-Removal, die Verfahrensweise des Ankers selbst,
diesmal also andersherum als bei O4 — ist **ein F-16 wert**, sauber attribuiert (`units` allein: ein
Verlust, `stores` allein: ein Verlust, keins von beiden: zwei).

Drei Befunde, keiner hier behoben. Der teuerste ist der billigste zum Hineinlaufen: **eine Kampfphase kann
durch einen fehlenden Navigationswegpunkt lautlos entwaffnet sein.** `FBF16FireControl` verwirft den ganzen
Block, wenn `state.Nav` unlesbar ist, und `FBNavSystem` publiziert ohne Steuerpunkt nichts — also hat ein
`set task bfm`-Jet ohne `wp` keine Kanonenlösung, keine DLZ und kein Raketentor. Gemessen am ersten
Zuschnitt dieser Kampagne: `blk_firecontrol` 0 über 3 001 Zeilen, 0 Schüsse, Exit 3 nach **14,8 s
ununterbrochenem Lock von 7,5 km bis 185 m** — eine `wp`-Zeile je Jet, sonst nichts, und der Lauf endet
0 bei t = 1,0 s. **Vorbestehend:** die Schützen von `bfm-basic`, `bfm-merge`, `bfm-offset` und `bfm-blind`
deklarieren alle keinen `wp`, `gun-bfm` und `gun-turning` — die beiden, die schießen — beide.

## 2026-07-30 — Kampagne W2 Osirak: die zehnte, und ihr Ergebnis ist eine Subtraktion

Die letzte der zehn. Zuvor aber die Nachprüfung, die die vorige Runde ausdrücklich offen gebucht hatte:
**beide Determinismus-Kriterien auf allen neun gebauten Kampagnen**, unter der Zweig-Umkehrung von
`b433950`. **81 Kampagnenläufe, 90 Einzelnachspiele, null Abweichungen**; jeder neue Fingerabdruck steht
jetzt im jeweiligen `## State` **neben** dem alten, der mit Datum stehen bleibt.

Und die Nachprüfung widerlegt eine Zeile der vorigen Runde. Sie hatte behauptet, die Schrittmuster aller
neun stimmten weiter; **zwei stimmen nicht**: `o3-07-top-cover` geht von Exit 1 auf 3 und
`w4-10-allied-force` von 3 auf 2. Beides war eine Ebene tiefer längst hergeleitet (`pilot.md` §7.4b, Zeile
für Zeile), nur nirgends in den Kampagnen gebucht. Jetzt steht es dort, mit Ursache. **Acht von neun
Fingerabdrücken haben sich bewegt, W5s nicht — byte-identisch** — und der Grund ist W5s eigene publizierte
Eigenschaft: null verschossene Waffen über zehn Einsätze, also kein Jet, der je in `Defend` geht, den
Zustand also, den der umgestellte Zweig besitzt. Dritter Befund: **Kampagnen-Fingerabdruck und
Missions-Regression messen nicht denselben Lauf.** `pilot.md` nennt fünf W1-Dateien als Bewegte, der
Fingerabdruck bewegt **acht von zehn** — `fb_regress.sh` fährt jede Mission standalone und **ohne Uhr**,
neun der zehn W1-Dateien deklarieren keine `time`, und die Kampagnenuhr allein bewegt 2 bzw. 7 Spalten
(`blk_env`, `vis_*`) und **null Bahnspalten**. Eine Missionsliste aus dem einen Instrument sagt über das
andere nichts.

**Dann W2.** Die Kampagne, deren eigene Spezifikation über sie schrieb, sie sei *„die, von der FlightBox
am weitesten entfernt ist"*, und deren erste Lieferung *„keine Missionsdatei, sondern ein
Zusatztank-Eintrag und ein Betankungsausleger"* sei. Die Hälfte davon ist am Vortag gelandet, und die
Kampagne fliegt: zehn `.fbm`, eine `.fbc`, Schritt-Exits `3 0 0 2 0 1 0 3 0 1`, beide Kriterien im ersten
Versuch (**9 Läufe, ein Fingerabdruck** `bdf58c2e…`; **10/10** Schritte reproduzieren standalone), kein
Byte unter `sim/src/` angefasst. Vier der zehn galten als baubar, zehn liefen, zehn antworteten.

**Das zentrale Ergebnis ist negativ und es ist eine Subtraktion.** Fünf Konfigurationen, ein und dieselbe
Strecke auf 240 m bei 400 kt bis zum Verlöschen: sauber **1 492,6 km**, mit zwei Mk-84 **1 162,3 km**, mit
zwei Tanks **2 173,4 km**, mit abgeworfenen leeren Tanks **2 327,9 km**, mit der vollen Kriegslast
**1 748,8 km**. Halbiert, ohne jede Reserve, ergibt das einen Kampfradius von **874,4 km gegen die 982,9 km,
die der Anker je Richtung braucht — 108,5 km zu wenig, 11,0 %**, und zwar bei Luftstart ohne Rollen,
Starten und Steigen, ohne Reserve, ohne Gefechtszuschlag und auf gerader Linie statt auf dem Dogleg, das
der Verband wirklich flog. Der Einsatz ist in diesem Baum nicht fliegbar, und das Loch hat exakt die Größe
der ungebauten Hälfte von `C5`.

**Der größte Hebel ist nicht der, nach dem die Kampagne gebaut wurde.** Die Tanks bringen +45,6 % sauber
und +50,5 % unter Kriegslast; sie fallenzulassen, wenn sie leer sind, noch einmal 7,1 % — und die Außentanks
sind nach **675,8 km** trocken gegen die *„etwa 1 000 km"* des Ankers, also dieselbe Größenordnung aus
völlig unabhängiger Richtung. Aber **die Anflughöhe allein kostet 43,2 % der Reichweite** (1 492,6 gegen
2 627,4 km auf 8 000 m). Die eigene taktische Entscheidung des Einsatzes ist das Teuerste an ihm, und keine
seiner Quellen sagt das.

**Der strittige Wert wurde in beiden Hälften geflogen, nicht gemittelt.** 30 m gegen 240 m, Faktor acht.
Über der Ebene der Kampagne hält die Lenkung 240 m auf **0,85 m über 300 km** und 30 m ebenso — und der
30-m-Fall ist dort kein Geländefolgeproblem, sondern ein **Zünderproblem**: `armMarginS` **0,486 s** von
2,0 s Schärfzeit. Über dem Boden, den der Einsatz wirklich überflog, ist **keine der beiden Höhen
fliegbar**: unter `--elev tiles` scheitert die Mission vor dem ersten Takt
(`spawn altitude is below ground, altM=240 groundM=487.48`), und die Strecke erreicht **1 599,22 m**.

Die Bodenhälfte ist eine Herleitung, die aufgeht: `target_hard` fällt innerhalb **17,7 m** einer Mk-84
(`2,81e7/r²` gegen 9,0e4 J/m²). Über siebzehn Abwürfe liegt `aimErrM` in einem Band von **6,36 bis
50,83 m**, zu 96–99 % längs, und `predErrM` ist Bodengeschwindigkeit mal konstant **0,228–0,241 s**, also
eine Latenz. Die Kuppel ist damit mit einer **Rate** zu töten: der Schlussangriff legt **fünf von acht**
Bomben innerhalb 17,7 m und die Kuppel fällt, bei acht von acht zurückgekehrten Angreifern. Die vier Pärchen
lösen auf **290,8 / 295,8 / 300,8 / 305,9 s** aus — die vom Autor gerechneten 1 029 m Abstand ergeben
**5,00 s, viermal, auf den Takt**; genau das heißt `C15`.

Die Mindestsprit-Entscheidung, tags zuvor erreichbar gemacht, ist jetzt in ihrer schärfsten Form gemessen:
`BINGO_ABORT … from=closing haveTgt=1` — der Pilot bricht **aus dem Anflug auf ein Ziel** ab. Seine
Kontrolle eine Zeile daneben schießt und fliegt heim; der Abbrecher endet **199,2 km von seinem eigenen Heimatwegpunkt**, 87,1 km jenseits des Ziels,
das es gerade verließ und spart über das Fenster nicht einmal Sprit.

Vier Befunde, keiner behoben: ein Abwurf an einem bombentragenden Jet wirft **die Bombe** (Stationsordnung,
`station=3 mk84` vor `TANK_JETTISON station=4`), sodass der selektive Abwurf des Ankers unausdrückbar ist;
die Zielerfassung einer Kanone hält **eine fallende Bombe für ein Flugzeug** (`rangeM=1250 closureMs=0
altM=111.256`); ein Frühwarnknoten weist eine Feuereinheit auf ein Ziel ein, das er selbst 200 km außerhalb
ihrer Reichweite misst; und die Angriffsphase löst **einmal je Anflug** aus, womit aus den sechzehn Bomben
des Ankers acht werden.

Und der Riss, der nur dieser Kampagne gehört: **die Übertragsschicht trägt genau das nicht, worum es hier
geht.** `campaign.md` verweigert Sprit als übertragene Tatsache, mit gutem und genanntem Grund — die Folge
bleibt, dass die eine Kampagne, deren Gegner der Sprit ist, von der Schicht über ihren Missionen blind
gesehen wird. Was der Übertrag kann, hat er scharf gezeigt: eine `action=drop`-Zeile, und **das Streichen
des Begleiters, der gestorben wäre, tötet den, der überlebt hätte** (standalone 1 von 2, in der Kampagne
0 von 1) — bei **8 von 29** byte-identischen Telemetriedateien, und die acht sind genau die acht Bomben.

## 2026-07-30 — Doktrin-Evolution `E5`: die Kampagnenbreite als Arena, und das Tor verweigert sie

**Schritt 5 des Eigner-Ziels, und sein Ergebnis ist eine begründete Verweigerung mit Zahlen.** Gebaut
wurde das Instrument, das `w1-red-flag.md` als fehlend benannt hatte: `tools/fb_campaign_arena.py`
spleißt ein Genom in eine **Kopie** einer committeten Mission, fliegt sie, liest sie im Worker und
löscht sie wieder. Eine Zelle ist `(Mission, Team, Modul)`; die Liste entsteht aus einer genannten
Regel und nicht aus einer Auswahl — **154 Zellen aus den 100 Missionen der zehn Kampagnen**.

**Zuerst gemessen, welche Gene überhaupt wirken können — 2 464 Läufe, je Gen sein veröffentlichter
Kanal.** G2 (`pilot_cover_frac`): `flt_defer_s` ist **0,0 in allen 2 464 Läufen auf allen 154 Zellen** —
das netzfähige Element trägt die AIM-120, deren Bindung 0,3 s dauert. G7 (`pilot_attack_ccip_m`):
strukturell tot, weil **keine der 54 Angriffsmissionen CCIP fliegt** (42 Dateien `ccrp`, 12 `opt`, 9 `arm`) und
`FBPilot.cpp:1368` den Schlüssel nur im CCIP-Zweig liest. G4 wirkt auf genau den zwölf Zellen, die
`set task bfm` erklären. G6 ist F-16-only: `FBMig29Pilot` überschreibt den Angriffsdurchgang mit
eigenem `ATTACK_CONSENT` und liest `AttackBiasS` nie. Bleibt G3, und nur seine Vertragshälfte auf der
**MiG-29** — 75 Kanal-, 33 Klassenbewegungen.

**Das Sättigungstor, mit dem festen Maßstab als S1-Population wie §4.2 es definiert (924 weitere
Läufe): 0 informative Zellen von 154.** In drei Lesarten geprüft, damit die Zahl nicht am Hebelfile
hängt: mit E2s eigenem `levers-genome.txt` 0, mit dieser Runde 15 Punkten 0, und in der lockersten
Lesart, die das Tor zulässt, **2** — genau das Urteil, das W1 auf seinen zehn Sprossen erreichte. Die
Verteilung ist die ehrliche Form: **89 Zellen bewegt kein Hebel, 46 einer, 15 zwei, 3 drei, 1 vier.**
Das Tor wurde nicht gelockert; `fb_arena_check.py` ist byte-identisch, und ein bodentauglicher
Maßstab, der 46 Zellen auf dem Papier informativ gemacht hätte, wurde aus E2s Grund nicht geschrieben.

**Keine Doktrinverschiebung wird veröffentlicht.** Die bindende Regel gilt: was auf einer Zelle
gemessen ist, die S1–S3 nicht besteht, ist ausdrücklich kein Befund.

**Was die Selektion trotzdem fand, und es ist die zweite Pflichtlieferung.** Vier Einträge, jeder mit
Kanal und Zahl. Der schärfste ist ein Exploit **unserer eigenen Fitness**: `FBMissionRunner` endet am
ersten Flugmonitor-K.O., und `ExpectedLoss` verzeiht nur einem bereits kampfunfähigen Flugzeug — ein
gesunder Strömungsabriss der **Gegenseite** beendet den Lauf, bevor irgendein Monitor abschließt, es
wird **keine einzige `mission OBJECTIVE`-Zeile** veröffentlicht, und Stufe M liest 0. In
`w4-10-allied-force` fällt `kamig4` bei t = 695,3 von 700 s: acht F-16 stehen bei `V = 16, M = 0`. Drei
unabhängige Hebel halten die MiG in der Luft — darunter einer, der die Bomben **2 794 m** danebenwirft
— und dieselben acht Jets stehen bei `V = 18, M = 8`. **17 von 154 Zellen** haben einen Hebel, der
diese Grenze überschreitet.

**Und zwei Defekte der Abwurfkette, gemessen als konstante ZEIT und nicht als Strecke.** Über acht
Angriffszellen, vier Kampagnen, zwei Waffen und vier Höhen liegt das Minimum von `aimErrM(bias)` bei
**−0,20 ± 0,05 s** — das ist eine Latenz, und es ist dieselbe, die W2 als `predErrM = Grundgeschwindigkeit
× 0,228…0,241 s` gemessen hat. Der Hook des Moduls (`AttackReleaseBiasS()`) steht auf **0,0 s**. Mit
−0,20 s fällt die gehärtete Kuppel von W2 (36,38 → **10,06 m**, Klasse (2,1) → (3,2)); X1 besser auf 2
von 8 Zellen und **auf keiner schlechter**, X4a acht Spawn-Störungen über ±3 m ohne Klassenwechsel,
X4b mit +50 % Zeitlimit gehalten. Darunter liegt eine Quantisierung: der Abwurf wird einmal je
Entscheidungstakt geprüft, `aimErrM(bias)` ist eine **Treppe** mit einer Stufe je 0,1 s, und eine Stufe
ist bei 231 m/s **23,1 m** — breiter als der 17,7-m-Radius, den eine Mk-84 gegen ein gehärtetes Ziel
braucht.

**Vierter Befund, unbequem und mit Kanal:** auf der EMCON-Sprosse `w1-07` kostet das kooperative
Datenlink einen F-16 — bei `flt_assign` = `sort_assign` = `eng_shots` = **0 in beiden Varianten**, also
nicht über die Zielaufteilung. Die Divergenzkette ist veröffentlicht und beginnt bei t = 0,1
(`dl_on` → `dl_tracks`/`flt_mates` → `rwr_brg` → Bahn), das Datenlink ist nicht hörbar, also bewegt
sich die **Verbandsgeometrie**. Determinismus über `--threads 1/2/4` an beiden Messpunkten: identische
Telemetrie-Prüfsumme.

`sim/src/` wurde nicht angefasst, `sim/assets` und `sim/missions` sind vor und nach jedem Lauf
byte-identisch, `verify-models` und `verify-layers` grün, sieben Harnesses rc = 0,
`core-lib`/`gym`/`native`/`wasm` warnungsfrei. Was einen Lauf möglich machen würde, steht als E-17 in
den Gaps: eine Fitness, die zwei Angriffsdoktrinen ordnen kann (Stufe C ist auf **32 der 46**
bombenwerfenden Zellen `GATE`), ein fester Maßstab, der auf der Zelle wirkt, die er beurteilt, und für
G2/G7 eine Arena, die es in den Kampagnen nicht gibt.

---

## 2026-07-30 — Doktrin-Evolution `E6`: die Handwerksstufe lernt den Boden, und der Richter schließt immer ab

`E5` hat zwei Defekte an unserer eigenen Fitness gemessen und beide stehen als Zahl da. Diese Runde
repariert sie und veröffentlicht wieder keine Doktrinverschiebung — sie fliegt gar keinen
Evolutionslauf. Bewegt haben sich genau zwei Dateien: `sim/tools/fb_fitness.py` und **ein Block** in
`sim/src/missions/FBMissionRunner.cpp`.

**Erstens: Stufe C hatte am Boden keinen Gradienten.** Jeder Posten war Luft-Luft, also war der
Schlüssel einer Angriffszelle `(V, M, GATE)` und eine Bombe 20 m daneben exakt so viel wert wie eine
2 km daneben — auf **32 der 46** bombenwerfenden Kampagnenzellen. Der neue Posten kommt aus `aimErrM`,
das der Richter ohnehin in jede `stores DELIVERY`-Zeile schreibt: `100 · Mittel über die Abwürfe von
1/(1 + e/10 m)`. Mittel und nicht Summe — eine Summe zahlte pro Abwurf, also pro Waffe, die der
Missionsautor an den Jet gehängt hat, und das ist Exhibit C in einer zweiten Währung.

**Und die zwei Währungen werden nicht addiert.** Ein sechster Summand hätte einen Meter Zielfehler in
Abschussgeometrie-Punkten bepreist — genau das stehende Angebot, gegen das §1.2 argumentiert. `C` ist
jetzt das Paar `(air, aim)`, verglichen über **Dominanz**: besser in einem und nicht schlechter im
anderen gewinnt, besser in einem und schlechter im anderen ist **unvergleichbar** und ist ein
Gleichstand. Es gibt keinen Wechselkurs, den eine Suche annehmen könnte, in keine Richtung. Preis:
Auflösung, nie Ordnung — und die Zellen, auf denen niemand schießt, sind mit demselben Argument frei,
mit dem das Tor sie vorher sperrte, denn die Torbedingung bekommt denselben Bodenarm (eine
veröffentlichte Lieferung).

Gemessen, ein Lauf je Zelle, gelesen von BEIDEN Fitness-Modulen: `C = GATE` fällt von **74 auf 42 von
154** Zellen und von **32 auf 0 von 46** liefernden; **(V, M) bewegt sich auf 0 von 154**. Auf
`w2-01-dome` liegen vier Hebel bei identischem `(V, M) = (2,1)` mit 36,4 / 59,5 / 82,6 / 61 294 m
Zielfehler — vorher vier Mal `GATE`, also exakt gleich, jetzt streng geordnet 21,6 > 14,4 > 10,8 > 0,0.

**Zweitens: X-1, der Exploit, den die Evolution an unserer Fitness gefunden hat.** Der Lauf endet am
ersten Flugmonitor-K.O., und wer dann noch offen war, schloss nie ab — also null `mission
OBJECTIVE`-Zeilen und Stufe M null für alle. **Geändert wurde nicht, WANN ein Lauf endet, sondern dass
der Richter trotzdem abschließt:** `FirstFlightKo` ist bis auf den Takt unangetastet, und die
Abschluss-Schleife läuft **nach** der Urteilskombination, kann also weder `ko` noch `failed` noch
`judged` noch das Ergebnis verschieben. `w4-10-allied-force` liest jetzt in der Grundlinie
`V = 18, M = 8` — genau das, was die drei Hebel liefern, die die MiG am Leben halten; die Beweger auf
dieser Zelle fallen von 3 auf 0, und der Lauf endet unverändert bei t = 695,3 mit `LOC`.

**Erhaltung, gemessen statt angenommen.** Über alle **251** `sim/missions/*.fbm`: **0 bewegte
Telemetriewerte, 0 bewegte Exit-Codes**, 27 `events.log` mit neuen Zeilen (80 `OBJECTIVE`, 68
`RESULT`, 58 `UNIT_RESULT`), Determinismus über `--threads 1/2/4` identisch. Drei Zeilen sagen etwas
anderes statt mehr — `net-belt-high`, `o1-08-belt-netted`, `o3-10-october-six` —, und alle drei sind
die **bestehende** Regel `ShotDownFirst`, die endlich greift: wer kampfunfähig geschossen wurde und
danach den Boden traf, wird vom Missionsrichter gemeldet, nicht vom Physikrichter. Ein gesunder
Abriss meldet weiter `LOC`. Die drei veröffentlichten Turnierergebnisse (`duels.md`, `formation.md`)
wurden auf **beiden** Instrumenten neu geflogen — altes Binary + alte Fitness gegen neues + neues — und
sind identisch bis auf die Ziffer, weil dort keine Bombe fällt und die Zielwährung in allen 70 Läufen
+0,0 ist. Elf Kampagnen: 99 Läufe, 11 Fingerabdrücke, 0 Divergenzen; 104 Einzelwiederholungen, 0
Divergenzen. Fünf Fingerabdrücke halten byte-genau, fünf bewegen sich — und die **acht** bewegten
Schritt-Fingerabdrücke sind exakt die acht Kampagnenmissionen aus der 27er-Liste.

Was diese Runde NICHT getan hat, mit Zahl: das 154-Zellen-Tor wurde **nicht vollständig neu geflogen**.
Die Handwerksstufe kann es nicht bewegen (S1/S2 rechnen auf `(V, M)`), die X-1-Reparatur schon — sie
verschiebt die Klasse jeder Zelle, deren Grundlinie oder Hebel die K.O.-Grenze kreuzte. Geflogen sind
**29 von 154** vollständigen Zellen, Beweger-Verteilung **24 × 0 · 4 × 1 · 1 × 2 von 15**; keine Zelle
erreicht `kMoversMin`. Das steht als Schuld in E-17 und nicht als Argument. Das Tor wurde nicht
gelockert, kein Genom-Schlüssel bewegt, kein Modell angefasst; `verify-models` und `verify-layers` grün,
sieben Harnesses rc = 0, `core-lib`/`gym`/`native`/`wasm` warnungsfrei.

## 2026-07-31 — `E7`: die Schuld ist bezahlt, und S1 hat das falsche Genom gemessen

Zwei Dinge waren offen, beide sind geliefert: das 154-Zellen-Tor **vollständig** neu gefahren nach dem
X-1-Fix, und S1s festes Feld mit dem Genom kommensurabel gemacht. **4.158 Läufe**, `sim/src/`
unangetastet, kein Torkonstante gelockert.

Der Befund der Runde brauchte **null Läufe**: das feste Feld unterscheidet seine sechs Mitglieder in
`pilot_shot_rtr`, `pilot_lock_nm`, `pilot_react_s` — das Genom besteht aus fünf ganz anderen Schlüsseln,
und alle sechs tragen dieselbe `sort`-Allele. Die Schnittmenge ist **leer**. „Informativ = S1 ∧ S2" war
also die Konjunktion zweier Fragen über verschiedene Dinge, und drei Runden Verweigerung gehörten zuerst
dem Instrument. Spec §9 (E15 Kommensurabilität, E16 Erweiterung statt Neuschrift, E17 die gebuchten
Kosten) und zwei Prüfer, die sich weigern statt zu behaupten — Beleg für E16 ist die **Commit-Reihenfolge**
(`f0d8115` vor jedem Lauf, der das Feld liest).

**Die Schuld, bezahlt — und sie widerlegt die Vorhersage, mit der sie gebucht wurde.** `E6` hatte
geschrieben, der X-1-Fix bewege die Klasse jeder der 17 Zellen an der K.O.-Grenze. Gemessen bewegt er die
Beweger-Verteilung um **zwei Zellen** (89·46·15·3·1 → 89·46·16·2·1). Eine Beweger-Zahl ist eine
*Differenz*, und der Fix hat Basis und Hebel meist gemeinsam über die Grenze geschoben.

**S1: 13 → 0, und der Mechanismus ist Arithmetik.** Das kommensurable Feld spaltet **mehr** Zellen (61
statt 52) und besteht S1 auf **keiner**. Alle dreizehn früheren Bestehen sind Zeile für Zeile verfolgt:
die Klassenzahl bleibt exakt gleich (2→2, 3→3, 4→4), der Modalanteil geht 50,0 % → 72,7 % bzw. 33,3 % →
63,6 % — also genau `(alt + 5)/11`. Kein einziges der fünf neuen Mitglieder erzeugt irgendwo eine neue
Klasse. Ein auf einer Zelle inertes Mitglied ist eine **Stimme für den Status quo**. Die Schranke ist
allgemein: die Basisklasse hält auf **allen 154** Zellen ≥ 2 der sechs Mitglieder, also ist der
Modalanteil bei inertem Genom mindestens 63,6 % — über S1s 60 %. Die dreizehn waren **Falschpositive,
jedes einzelne**. `E4`s ungeklärte Beobachtung „S1 und S2 bestehen auf verschiedenen Zellen" ist damit
aufgelöst statt gemildert.

**Die bindende Schranke ist S2, und kein Feld erreicht sie**: S2 zählt über die Hebel, nie über das Feld.
0 von 154 Zellen erreichen die geforderten 5 Beweger, die beste der ganzen Kampagnenbreite hat 4 — und
**5 der 15 Hebel sind auf allen 154 Zellen strukturell tot** (G2 dreimal, G7 zweimal). Selbst die
großzügigste ehrliche Rechnung lässt **eine** Zelle bestehen, gegen S5s drei.

Die Evolution ist **nicht** gelaufen, und das ist kein Versäumnis, sondern §6: auf einer Zelle, die das
Tor nicht besteht, ist nichts ein Befund. `tools/arena-informative.txt` wird vom Tor selbst geschrieben
und enthält null Zellen.

Die fehlende Arena ist jetzt datiert statt beschrieben: **keine** der 100 Kampagnenmissionen wirft in
CCIP (102 × `ccrp`, 32 × `opt`, 20 × `arm`), obwohl Modus und Rig existieren und kein C++ fehlt. Das ist
zuerst eine **Realismuslücke** der Kampagnen und darum baubar, ohne die Arena um ein Gen herum zu bauen.
Neu gebucht: E-19 — S1s Schwelle ist ein Anteil, und ein Anteil ist nicht invariant unter der Größe des
Feldes, in dem er genommen wird. Nicht hier repariert, mit Absicht.

**Nachtrag `E7`, gleicher Tag.** Die Runde hat auch die *andere* Arena gefragt, und sie ist ebenfalls
verweigert: die generierten Geometrien haben bei `--flight 1` noch **1 informative von 12**, gegen die
**4**, die E-12 verzeichnet. Die drei fehlenden sind an den eigenen Reparaturen des Baums verloren
gegangen (E-15s FLCS-Dämpfer, X-1s Richter) — E-15s eigener Satz, auf E-15 angewandt: eine Geometrie,
deren Informativität daher kommt, dass eine Seite an einem Bug stirbt, ist eine Messung des Bugs.

Sind **beide** Arenen zu, kann die Ursache keine Eigenschaft einer Arena sein. Sie ist das Genom, und
die Zählung ist exakt: von den fünf Erweiterungen, die der Eigner-Auftrag nennt, sind **zwei überhaupt
keine Schlüssel** — `set pilot_flight_shape` und `set pilot_emcon_frac` werden bei t = 0.0 abgelehnt und
der Lauf endet mit exit 1, blockiert von `formation.md` F5 und `duels.md` D3. G2 ist mangels
Waffenbindung inert (0 Beweger auf 154 Zellen), G4 lebt nur in `Phase::Bfm` (9 Zellen), und G3 allein
bewegt die Breite (30 Zellen). Das erklärt vier Runden ohne veröffentlichbare Doktrinverschiebung
vollständig — und es ist ein Baurückstand, kein Torproblem. Als E-20 gebucht, mit einer nach
Freischaltwirkung geordneten Liste; der erste Posten ist F5, der zweite D3.

## 2026-07-31 — `E8`: die Arena besteht, die Evolution läuft, und X4 verweigert das Ergebnis

`E-20` hatte gemessen, dass der Blocker das Genom ist, und F5 als ersten Posten benannt. F5 ist gebaut,
G1 ist ein Schlüssel — und alles Weitere folgt in einer Kette, bis zur Verweigerung am Ende, die der
schärfste Befund der Runde ist.

**Ein Gen freizuschalten hat die Kampagnenbreite von 0 auf 3 informative Zellen gehoben** und die erste
Evolution dieser Linie vollständig laufen lassen. G1 bewegt die Ergebnisklasse auf **13 Zellen**; bei 21
Hebeln liegt S2s Schwelle bei 7, und **vier Zellen** erreichen sie — die ersten S2-Bestehen überhaupt.
Das Tor: `S4 154 ≥ 6 ok · S5 3 ≥ 3 ok · S6 0 ok · ARENA: PASSED`. Alle drei informativen Zellen sind der
MiG-29-Sitz; `w3-09-saturation:f16` besteht S2 mit 7 Bewegern und fällt an S1. Diese Runde evolviert
also eine Doktrin, nicht zwei.

**Die Evolution:** 723 Läufe, sechs Generationen, Population 40, acht lebende Gene. Der Champion stammt
aus Generation 0 und hat sich nie bewegt — sechs Generationen Gitterabtastung über sieben numerische
Gene bewegen nichts, entschieden hat allein `sort=near`. Der feste Maßstab bleibt über alle sechs
Generationen bei 0,556, also **flach**; mit T = 0,0000 ist das nach E-16 ein Fixpunkt und kein Kreisen.
Gesättigt ist die Arena nicht: Stufe V entscheidet in Generation 5 **612** Vergleiche.

**X3 besteht.** Die Kette ist mit Zahlen benennbar: auf `o3-10` geht `flt_src` 0 → 2, `flt_assign` 0 → 4,
`SORT_ASSIGN` 0 → 20 — ohne den gebrieften Vertrag hat der Verband **gar keine Zuweisungsquelle**, denn
die MiG-29 hat kein kooperatives Terminal. E2s Satz „ein Vertrag neben einem lebenden Netz ist toter
Text" ist hier umgekehrt: der Vertrag ist der einzige Text. Auf `w3-09` sortiert er zusätzlich
**stabiler**, `flt_switch` 12 → 9.

**Und X4 verweigert.** Die Bahnstörung über ±3 m kippt die Ergebnisklasse auf `o3-10` in 3 von 8 und auf
`w1-09-lfe-four` in 3 von 8 Proben; §5s Rauschboden ist 2 von 8, und darüber gilt „no claim may be made
on that geometry at all". Die zweite Messung klärt, wem das Chaos gehört: mit einem Genom, das ein
einziges unbeteiligtes Gen setzt, kippt `w1-09-lfe-four` in **8 von 8**. Es ist die Zelle, nicht der
Champion. Damit bleibt X1 eine Geometrie, und §6 ist bindend: **es wird kein §1 veröffentlicht.**

Das ist das Produkt der Runde, und es qualifiziert das Tor selbst: S1 und S2 können **einen Hebel nicht
von einer Münze unterscheiden**. Ausgerechnet die Zelle mit den meisten Bewegern der ganzen Breite (8 von
21) kippt bei jeder 0,8-m-Störung. Als E-21 gebucht, mit dem Vertrag, den es verlangt — ein siebtes
Kriterium S7: informativ nur, wenn die Ergebnisklasse der Basis dasselbe 0,8-m-Gitter überlebt, acht
Läufe je Zelle. Auf diese Runde angewandt bliebe **eine** Zelle, und die Arena wäre verweigert — ehrlich.

**Korrektur zu `E8`, noch am selben Tag und aus eigener Prüfung.** E8s Kernzahl — „von 0 auf 3
informative Zellen" — ist **kontaminiert und keine Messung**. Der Lauf hat Basis und die 15
Vertragshebel aus einem Kanalindex übernommen, der **vor** F5s Änderung an `FormationTrailM`
geschrieben wurde, und nur die 6 Formhebel mit dem neuen Binary geflogen. Damit wurden die sechs gegen
eine Basis aus dem *alten* Simulator verglichen — und sie sehen genau auf den Zellen wie Beweger aus,
die der Vorgabenwechsel bewegt: den Vierer-Verbänden.

Gefunden habe ich es an einem Widerspruch, den ich nicht wegerklären konnte: S7 las
`w3-09-saturation` als 8-von-8-Kipper, während das eigenständige Audit dieselbe Zelle als 0 von 8
gelesen hatte. Die Ursache war die Vergleichsbasis, nicht die Zelle — zwischengespeichert `(11,5)`,
frisch geflogen `(10,4)`. In einem Codepfad nachgemessen ist `w3-09-saturation` bei **0 von 8** robust,
`o3-10` bei 3, `w1-09-lfe-four` bei 8.

Der Fix ist strukturell und nicht eine Gewohnheit: ein Kanalindex trägt jetzt den SHA-256 des
Simulators, der ihn geschrieben hat, und **verweigert** die Wiederaufnahme unter einem anderen.
Negativtest grün. Der komplette Hebel- und Feldpass fliegt neu (3.388 + 2.156 Läufe); die korrigierten
Zahlen kommen als `E9`.

## 2026-07-31 — `E9`: die korrigierte Zahl, und warum ein wachsendes Genom dieses Tor nicht öffnen kann

E8s Kernzahl ist zurückgezogen; hier ist dieselbe Messung sauber geflogen — **3.388 Hebelläufe, jeder
unter Simulator `4b10f951`**, und der Kanalindex trägt jetzt dessen SHA-256 und verweigert die
Wiederaufnahme unter einem anderen.

| | `E8`, kontaminiert | `E9`, sauber |
|---|---:|---:|
| beste Zelle, Beweger von 21 | 8 | **6** |
| Zellen über S2s Schwelle 7 | 4 | **0** |
| informativ | 3 | **0** |
| Urteil | PASSED | **REFUSED** |

G1s echte Reichweite je Hebel: 5 / 5 / 5 / 4 / 1 / 1 Zellen — gegen 9 / 9 / 8 / 8 / 3 / 3 im
kontaminierten Lauf. Der S7-Schirm lief gar nicht, weil er S1∧S2-Kandidaten schirmt und es keine gab.

**Der Befund ist eine Arithmetik.** S2s Schwelle ist ein Verhältnis, also hebt ein Gen mit *k* Hebeln
die Schranke um *k/3*. G1 brachte sechs Hebel, lieferte auf der besten Zelle **drei** Beweger, und die
Schranke stieg um **zwei**: von *beste 4 von 15, Schwelle 5* auf *beste 6 von 21, Schwelle 7* — das
**Defizit bleibt 1**. Das ist E10 genau wie geschrieben; ungeschrieben war die Folge: **ein Gen hilft
nur dort, wo seine eigene Reichweite JE ZELLE k/3 schlägt.** Ein Gen, das viele Zellen um je einen
Hebel bewegt (G1: 13 Zellen), kann dieses Tor nicht öffnen — nur eines, das EINE Zelle in dreien seiner
eigenen Hebel bewegt. Als E-22 gebucht, und für das nächste Gen vorab falsifizierbar: G5s
EMCON-Hebel müssen auf einer einzelnen Zelle ≥ 3 der eigenen bewegen, sonst bewegen sie die Schranke
und nicht das Urteil.

Die beste Zelle der Breite ist jetzt `w3-09-saturation:f16` mit sechs Bewegern aus **drei Familien
gleichzeitig** — und damit ein F-16-Sitz. Die drei MiG-29-Zellen, die E8 zertifiziert hatte, waren ein
Artefakt.

Nebenbei und ohne ein Ergebnis zu ändern: der feste Maßstab fliegt nur noch auf Zellen, die S2 schon
bestanden haben. Diese Runde sparte damit 2.156 Läufe, weil S2 nirgends hielt.

**Nachtrag `E9`: D3 ist lokalisiert, und es ist eine Zeile.** `pilot/FBPilot.cpp:765` — `CanPressOn`
liest `state.Radar.Radiating`, also entscheidet ein Pilot, der die Emission abschaltet, im selben Takt,
dass er den Auftrag nicht fortsetzen kann. Der Befehlsweg, den er bräuchte, ist vollständig da und wird
von ihm nicht benutzt: `FBCommandTarget::RadarEmission` existiert, `FBMig29Emission::Off` existiert,
beide Module ehren ihn, und `FBMig29Pilot` schaltet auf GCI-Stichwort bereits auf `Illum`. Worauf jede
Zelle still fliegen würde, ist ebenfalls schon veröffentlicht: die MiG-29 auf dem IRST-Block (Winkel,
keine Entfernung, keine Identität), die F-16 auf Datalink und NetLink. Die Abnahme steht nach E-22
vorab fest: G5s Hebel müssen auf EINER Zelle ≥ 3 der eigenen bewegen. Beste Zelle heute
`w3-09-saturation:f16`, 6 von 21 gegen Schwelle 7 — Defizit 1.

## 2026-07-31 — `E10`: jede Reparatur nimmt dem Tor Doktrinsignal weg

`duels.md` D3a ist gebaut — `CanPressOn` fragt nach einem **Bild** statt nach einem Sender — und das
154-Zellen-Tor ist unter dem neuen Simulator neu geflogen (3.388 Läufe, frischer Index; den alten hat
der Wächter verweigert). Die Arena ist wieder verweigert, und *wie* sie verweigert ist, ist der Befund.

`w1-07-emcon:f16` fällt von **5 Bewegern auf 0**, Basis (3,1) → (4,2). Alle fünf waren der Defekt: die
Hebel haben umgeschaltet, **ob der Jet abbricht**, nicht wie er kämpft. Die beste Zelle der Breite bleibt
`w3-09-saturation:f16` mit 6 von 21 gegen Schwelle 7 — Defizit weiterhin 1.

**Das Muster ist jetzt vier unabhängige Fälle:** E-15s Flugregler nahm `xmerge`/`xmergesplit` ihren
S1-Pass (2 Klassen bei 50 % → 1 bei 100 %); X-1s Richter nahm `w4-10-allied-force:f16` seine 3 Beweger;
beide zusammen nahmen der generierten Arena 3 ihrer 4 informativen Geometrien; und D3a nimmt der
EMCON-Sprosse alle fünf. E-15 hatte die Regel für eine Geometrie geschrieben — eine Geometrie, deren
Informativität daher kommt, dass eine Seite an einem Bug stirbt, ist eine Messung des Bugs. Vier Fälle
später ist es eine Eigenschaft dieser Arena: **die scheinbare Doktrinempfindlichkeit der Kampagnenbreite
war überwiegend defektgetrieben, und jede Reparatur senkt sie.**

Daraus folgt keine Ausrede. Jede der vier Reparaturen hat den Simulator korrekter gemacht, und
`w1-07-emcon` gelingt jetzt, wo es zweimal scheiterte. Es folgt eine Aussage über das **Instrument**:
ein Kriterium auf „bewegt sich die Ergebnisklasse" misst eine Mischung aus Doktrin und Defekt, und in
diesem Baum war die Mischung überwiegend Defekt. Was nach den Reparaturen übrig bleibt, ist das echte
Signal — und das ist heute **einen Beweger von S2 entfernt, auf einer Zelle von 154**. Als E-23 gebucht.

Und noch eine eigene Bedingung ist gefallen und bleibt mit ihrer Messung stehen: D3as Spec verlangte ein
NO-OP über alle 251 Missionen, gemessen bewegen sich vier. Es ist eine Verhaltensänderung, keine reine
Vorbedingung — alle 251 Exit-Codes stehen, Determinismus über 1/2/4 Threads hält.

## 2026-07-31 — `E11`: das Genom ist vollständig, S2 fällt zum ersten Mal, und S7 hält

G5 ist gebaut. **Kein Gen der Auftragsliste ist mehr blockiert** — neun lebende Gene, null Blocker, wo
vor zwei Runden zwei von fünf nicht einmal Schlüssel waren. Das 154-Zellen-Tor ist mit 24 Hebeln neu
geflogen (3.850 Läufe, frischer Index).

**`w3-09-saturation:f16` besteht S2 — die erste Zelle überhaupt in dieser Datei**, und nicht knapp:
**11 Beweger von 24** gegen Schwelle 8, drei Ergebnisklassen bei 53,3 % Modalanteil (S1 ok), und **vier
Genfamilien wirken gleichzeitig** — Netz, Abwurfvorhalt, Form und Emission.

E-22 hatte die Abnahme für G5 **vorher** festgelegt: seine Hebel müssen auf einer Zelle ≥ 3 der eigenen
bewegen. Zwölf Probeläufe vor dem Sweep zeigten 2 von 3, und der Sweep hob die Zelle von 6 von 21 auf 11
von 24. Das ist die erste quantitative Vorhersage dieser Datei, die vor dem Lauf stand und eintrat.

**Und S7 verweigert sie trotzdem: 1 Kipper von 8.** Die Schwelle bleibt bei null — sie wurde in §10 mit
der Begründung gesetzt, dass Zulassung strenger prüft als §5s 2-von-8-Boden für das Lesen eines
Champions, und ausdrücklich mit der Erwartung geschrieben, E8s Arena verweigern zu müssen. Sie jetzt zu
lockern, wo ich weiß, dass es das Tor öffnete, ist genau der Griff, den diese Runde dreimal abgelehnt hat.

Legitim ist, das **Instrument** zu schärfen statt des Kriteriums. Auf einem 0,25-m-Gitter mit 24 Proben:
**3 Kipper von 24 = 12,5 %** — exakt derselbe Anteil wie 1 von 8. Die Zelle ist wirklich zu einem Achtel
eine Münze, und S7 hat recht.

Damit hat sich der Grund der Verweigerung zum ersten Mal verschoben: nicht mehr „das Genom kann nicht
wirken" — es wirkt mit vier Familien gleichzeitig —, sondern **die Kampagnenbreite hat keine Sprosse, die
zugleich benotbar und robust ist**. Das ist eine Aussage über die Missionen, und sie ist jetzt beziffert:
gebraucht werden ≥ 3 Sprossen mit ≥ 8 Bewegern von 24 **und 0 Kippern von 24**.

**Nachtrag `E11`: benotbar und robust stehen NICHT im Widerspruch.** Die naheliegende Sorge nach S7 war,
dass eine Zelle nur dadurch doktrinempfindlich wird, dass sie auf einer Messerschneide sitzt. Gemessen an
den zwölf beweglichsten Zellen, jede über S7s eigenes 0,8-m-Gitter: **zehn von zwölf sind sauber.**
Chaotisch sind nur die zwei beweglichsten — und selbst das ist kein Gesetz, denn `o5-09-night-two:f16`
trägt fünf Beweger bei null Kippern.

Das Ziel ist damit nicht „das Chaos beheben", sondern **robuste Zellen beweglicher machen**. Die zwei
besten Kandidaten scheitern aus verschiedenen, strukturellen Gründen: `o5-09-night-two` ist eine ROTTE,
also können die Trail-Hebel gar nicht wirken (`aftM = element × trail`, und eine Rotte hat kein zweites
Element) — zwei von G1s sechs Hebeln sind auf ihr unerreichbar. `w3-10-package-q` ist ein Vierer mit Netz
und sechzehn `datalink on`, und trotzdem bewegen ihn weder Form noch Emission; das ist das Nächste zu
messen, nicht das Nächste anzunehmen.

Und was daraus NICHT werden darf: eine committete Sprosse zu ändern, WEIL es die Beweger höbe, wählt die
Arena nach dem Ergebnis aus. Ob eine Rotte ein Vierer wird, ist eine Doktrinfrage dieser Kampagne, und
das Tor ist die Prüfung darauf — nie der Grund dafür.

**Nachtrag `E11`: die Kampagnenschicht ist unter dem aktuellen Simulator frisch nachgewiesen.** Nach F5,
D3a und G5 stand die Determinismus-Abnahme aus — sie ist gefahren, nicht angenommen:

| | |
|---|---|
| Kampagnenläufe | **99** = 11 Kampagnen × 3 Wiederholungen × `--threads 1/2/4` |
| Fingerabdrücke je Kampagne | **genau einer**, 11 von 11, rc = 0 |
| Einzelnachspiele | **104** Schritte, jeder standalone aus dem Zustandsfile des vorigen |
| Divergenzen | **0** in beiden Kriterien |

Das ist rund 1.100 Missionsläufe. Beide Kriterien aus `missions/campaign.md` §5 halten: dieselbe
Kampagne gibt über drei Threadzahlen und drei Wiederholungen denselben Fingerabdruck, und jeder Schritt
ist aus dem Zustand seines Vorgängers einzeln reproduzierbar — die Kampagnenschicht fügt also keinen
verborgenen Zustand hinzu. Damit ist „mehrfach und deterministisch durchgespielt" für den heutigen Stand
belegt, nicht für einen von gestern.

**Nachtrag `E11`, letzter: was die zwei robusten Kandidaten publizieren.** `w3-10-package-q:f16` ist ein
Vierer mit drei Kameraden, dessen Sortierung **nie greift** — `flt_src = 0` und `flt_assign = 0` auf der
Basis und auf jedem Form- und Emissionshebel gleichermaßen. Ob das ein Defekt der Verbandslogik im
Vierer ist oder eine Eigenschaft einer Geometrie, in der es nichts zu teilen gibt, ist als Nächstes zu
**messen** — geraten wird es hier nicht.

`o5-09-night-two:f16` dagegen trägt eine **Kette**: `emcon-tight` hebt `releases` von 0 auf 2,
`deliveries` von 0 auf 2 und damit `M` von 1 auf 4. Der Jet, der nicht strahlt, wird nicht gewarnt,
überlebt bis zum Abwurfpunkt und drückt. Jedes Glied ist eine publizierte Spalte, und **die Zelle ist
robust: 0 Kipper von 8.** Das ist das Nächste an einer Doktrinverschiebung, was diese Datei je gemessen
hat — und §6 verbietet weiterhin, es zu veröffentlichen, weil die Arena eine solche Zelle hat und drei
braucht. Es steht hier als Messung, nicht als §1.

**Nachtrag `E11`, und er widerlegt meine eigene Vermutung von einer Stunde vorher.** Ich hatte aus
`w3-10-package-q:f16`s `flt_src = 0` geschlossen, dass sechzehn summierte Einheiten einen Doktrineffekt
**verdünnen**. Über alle 154 Zellen gemessen ist das falsch, und die Wahrheit läuft andersherum:

| Einheiten der benoteten Seite | Zellen | Ø Beweger von 24 | max |
|---:|---:|---:|---:|
| 1 | 41 | 0,44 | 3 |
| 2 | 66 | 0,45 | 2 |
| 4 | 30 | 1,37 | 6 |
| 8 | 6 | **2,00** | **11** |
| 16 | 1 | **4,00** | 4 |

**Benotbarkeit steigt mit der Größe der benoteten Seite.** Und damit liegt die Masse der Arena am
falschen Ort: **107 der 154 Zellen sind ein einzelnes Flugzeug oder eine Rotte**, im Mittel bei 0,44
Bewegern. Ein einzelner Jet hat keinen Verband zu formen, keinen Kameraden zum Sortieren und niemanden,
hinter dem er schweigen könnte. Diese Datei hat vier Runden lang im Genom und im Tor gesucht, was eine
Eigenschaft der **Seitengröße** ist.

Damit ist das Ziel aus §4 in seiner schärfsten Form da: die drei Sprossen, die das Tor braucht, sind
nicht irgendwelche drei — es sind drei mit **vier Flugzeugen oder mehr**, die zugleich robust sind. Von
denen gibt es in der Breite 38, und genau eine erreicht heute acht Beweger. Es ist die chaotische.
