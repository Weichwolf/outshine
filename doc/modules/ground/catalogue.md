# The ground threat catalogue — nine rows

**Subject:** the **real** systems behind `kSiteCatalogue`. One row per `FBModuleRegistry` key, each with
its two radars separated, its envelope, its timing, whether its guidance **binds** the emitter to the
target, and what happens when it is killed or countered.

**Delimitation:** this file is the DATA. The class that consumes it, the layer it lives in, the hooks it
needs and the anti-cheat argument are [`module.md`](module.md).

**Status: BUILT.** All nine rows are `core/FBSite.h`, all six rounds `core/FBStore.h`, both guns
`core/FBGun.h`, and the six missile decks `sim/assets/aircraft/<key>/` from one generated recipe.
FOUR fields were added while building and they are named here rather than discovered in a result:
`RailCount` and `ReloadS` (the magazine: how many rounds leave before the crew has to work, and for how
long the position is then out of action), `SalvoS` (the spacing inside a doctrinal salvo) and `WarmupS`
(`set alert cold`). Three launch masses — V-601 953 kg, 9M32M 9.8 kg, 9M39 10.8 kg — are **[T4\*]**:
they come from the row's ALREADY-CITED page and were NOT re-read in this round. The 3M9's diameter
(330 mm) and length (5.8 m) carry the same mark.

**Schema:** the same as [`../mig29/`](../mig29/INDEX.md) — every number carries a **source** and a
**confidence tier**, values that disagree between sources are carried **both**, and a value nobody
published is `[SET]` with a reason or `[TODO]` and not guessed.

| Tag | Meaning |
|---|---|
| **[T1]** | official / government / service document |
| **[T3]** | established specialist literature (here: Air Power Australia's system monographs) |
| **[T4]** | encyclopaedic / community consensus (here: Wikipedia, Military Wiki) |
| **[DISPUTED]** | sources conflict; both values carried, neither preferred |
| **[SET]** | a FlightBox setting, with its reason |
| **[DERIVED]** | computed from a named number by a named relation |
| **[TODO]** | not sourced and not set — the field is open |

**The sourcing is honestly thin.** Eight of the nine rows rest on **[T4]** encyclopaedic entries; exactly
one system (the 2K12) has a **[T3]** monograph, and it disagrees with the [T4] entry on the envelope. No
[T1] source (a service threat handbook, a TRADOC Worldwide Equipment Guide, a declassified intelligence
assessment) was read for this round. That is the same asymmetry `../mig29/` declares for the aircraft, and
it is declared rather than hidden: **the envelope numbers are the load-bearing ones, and they are the ones
a [T1] source would most likely move.**

---

## The one-glance table

| Key | System | Class | Search set | Fire-control set | Envelope (km) | Alt band (m) | React | Ch | Binds shooter? | Countered by |
|---|---|---|---|---|---|---|---|---|---|---|
| `p18` | P-18 "Spoon Rest D" | EW / GCI, **no weapon** | VHF, 250 km | — | — | — | — | 0 | — | killing it |
| `sa2` | S-75 Dvina/Volkhov | fixed strategic SAM | P-12/P-18, 250–275 km | SNR-75 "Fan Song", E/F/G, 120 km | 8–30 / …45 | 450–25 000 | `[SET]` | 1 | **yes, to impact** (command) | breaking its track |
| `sa3` | S-125 Neva/Pechora | fixed low/medium SAM | P-15 "Flat Face", 250 km | SNR-125 "Low Blow", I/D, 40 km | 3.5–35 | 100–18 000 | `[SET]` | 1 | **yes, to impact** (command) | breaking its track |
| `sa6` | 2K12 Kub/Kvadrat | **mobile** SAM | 1S11 in the 1S91, G/H | 1S31 CW illuminator in the same vehicle | 4–24 / 6–22 | 50–14 000 / 100–7 000 | 22–28 s | 1 | **yes, to impact** (CW illumination) | breaking the illumination, chaff |
| `sa8` | 9K33 Osa | **mobile, self-contained** SAM | 1S51 "Land Roll", H band, 30–35 km | same vehicle, J band | 1.5–15 | 10–12 000 | 26 s | 1 (2 rounds) | **yes, to impact** (command) | breaking its track, chaff |
| `zsu23` | ZSU-23-4 Shilka | radar-directed AAA | RPK-2 "Gun Dish", Ku, 20 km | the same set | 0–2.5 | 0–1 500 | `[SET]` | 1 | trivially (it tracks) | flying above it |
| `zu23` | ZU-23-2 | optical AAA | **none — the eye** | ZAP-23 optical sight | 0–2.5 | 0–1 500 | `[SET]` | 1 | trivially | flying above it, darkness |
| `sa7` | 9K32 Strela-2M | MANPADS, rear aspect | **none — the eye** | the seeker | 0.8–4.2 | 50–2 300 | 6–13 s | 1 | **no** — free at launch | **flares**, aspect |
| `sa18` | 9K38 Igla | MANPADS, all aspect | **none — the eye** | the seeker | …5.2 | …3 500 | `[SET]` | 1 | **no** — free at launch | aspect only (no IRCCM model) |

**The two facts a pilot has to learn from this table:** a battery is a **one-target machine** — every SAM
row has one engagement channel, so a package saturates it by arithmetic — and everything except the two
MANPADS rows **binds its emitter to its target for the whole flight of the round**, so breaking the track
breaks the shot.

---

## `p18` — P-18 "Spoon Rest D", the emitter with no weapon

Seven campaigns want a radiating ground object that can be switched off (W2, W3, W4, W5, O1, O2, O5). This
is it, and it is the cheapest row in the catalogue: a search set, a health register and nothing else.

| Quantity | Value | Source |
|---|---|---|
| Band | VHF (metric) | [T4] [Wikipedia, P-18 radar](https://en.wikipedia.org/wiki/P-18_radar) |
| Detection range, fighter-sized target | **250 km** | [T4] ibid. |
| Altitude coverage | 35 km | [T4] ibid. |
| Azimuth / elevation coverage | 360° / −5°…+15° | [T4] ibid. |
| Antenna rotation | **10 rpm** | [T4] ibid. |
| Role | early warning, and "developed to work independently or as part of a C3 system directing SAM and aircraft to hostile targets" | [T4] ibid. |
| Antenna | sixteen Yagi elements in two sets of eight | [T4] ibid. |

**FlightBox row:**

| Field | Value | Provenance |
|---|---|---|
| `SearchRangeM` | 250 000 | [T4] above |
| `SearchAzHalfDeg` / `SearchElCenterDeg` / `SearchElHalfDeg` | 180 / +5 / 10 | [DERIVED] from the −5…+15° coverage: centre +5°, half-width 10° |
| `SearchFrameS` | **6.0** | [DERIVED] 10 rpm = one revolution per 6 s. **The one sourced antenna rate in the catalogue**, and the reference every `[SET]` frame time below points at |
| `EmitterKind` | `SurfaceEarlyWarning` | behind this antenna sits a telephone, not a launcher |
| `Channels` / `RoundsDefault` | 0 / 0 | it has no weapon |
| Damage ids | `Radar`, `Structure`, one zone | killing the antenna kills the emission by the coupling; killing the van meets `kill unit` |

**When killed:** silent, immediately and permanently (`Radar` failed → block `Invalid` → `Emission()`
`None`). **When countered:** nothing — a VHF EW set is exactly what the tree cannot jam (`C13`), and its
long wavelength is precisely why real jamming of it was hard.

**What it does NOT do:** it cues nobody. A `p18` beside an `sa3` does not make the `sa3` smarter, because
the only legal cross-unit channel is a sensor ([`module.md`](module.md) §Knowledge 5). It is a target and
a warning, not a network. The network is `C6`/`C18`.

---

## `sa2` — S-75 Dvina/Volkhov, the fixed strategic battalion

| Quantity | Value | Source |
|---|---|---|
| Guidance | **radio command guidance** (CLOS) | [T4] [Wikipedia, S-75 Dvina](https://en.wikipedia.org/wiki/S-75_Dvina) |
| Engagement range | **30 km** max, **8 km** min | [T4] [Military Wiki, S-75 Dvina](https://military-history.fandom.com/wiki/S-75_Dvina) |
| Engagement range | **45 km** operational maximum (later variants) | [T4] Wikipedia ibid. **[DISPUTED]** — both carried |
| Short-range cutoff | 500–1 000 m, "making them fairly safe for engagements at low level" | [T4] Military Wiki ibid. |
| Altitude band | **450–25 000 m**; low cutoff 400–500 m depending on variant | [T4] both, consistent |
| Fire-control set | SNR-75 "Fan Song", trailer-mounted, **E/F band and G band** | [T4] [Wikipedia, Fan Song](https://en.wikipedia.org/wiki/Fan_Song) |
| Fan Song A tracking range | 120 km, E band | [T4] ibid. |
| Beamwidth | ~7°, Lewis scanners with ~20° scan | [T4] Wikipedia, S-75 |
| Channels | tracks **6 targets**, guides **3 missiles at one target** | [T4] Military Wiki / Wikipedia — the two statements are about different things and are both kept: the *engagement* channel count is **1** |
| Search set | P-12 "Spoon Rest", **275 km** | [T4] Wikipedia, S-75 |
| Missile V-750/V-750V | warhead **195 kg**, prox + contact + command fuzing; launch mass 2 287 / 2 163 kg; diameter 500 mm (booster 654 mm); length 10 726 mm; **Mach 3.5**, ~Mach 3 after burnout | [T4] Wikipedia, S-75 |
| Battery | six launchers per battalion in a hexagon, six reloads on trailers | [T4] ibid. |
| Reaction time | **not sourced** | [TODO] |

**FlightBox row:**

| Field | Value | Provenance |
|---|---|---|
| `StoreKey` | `v750`, `FBSeekerKind::CommandGuided`, `WarheadKg = 195`, `FuzeRadiusM = 12` | warhead [T4]; fuze radius **[SET]** — the tree's air-to-air rounds carry 3.5–10 m for 7–20 kg heads, and a 195 kg head gets the largest gate in the catalogue. The 1/r² law (`weapons.md` §6.1) makes the radius a *did-it-detonate* gate; the **mass** carries the lethality |
| Deck | own slender-body deck sized from ⌀500 mm, 2 163 kg, Mach 3.5 by the recipe of `weapons.md` §10.2 | [DERIVED] |
| `EnvMinM` / `EnvMaxM` | 8 000 / 30 000 | [T4]; the 45 km figure is the Volkhov variant and is **not** taken, because the campaigns' anchors (Vietnam, 1973, 1991) are Dvina-class |
| `EnvAltMinM` / `EnvAltMaxM` | 450 / 25 000 | [T4] |
| `TrackRangeM` | 120 000 | [T4] Fan Song A |
| `SearchRangeM` / `SearchFrameS` | 275 000 / 6.0 | [T4] / **[SET]** at the P-18's sourced 10 rpm, for want of a rate for the P-12 |
| `DopplerNotchMs` / `NotchRejects` | **0 / false** | [SET]. The Fan Song is a conical-scan/Lewis-scanner tracker, not a pulse-Doppler set; declaring a notch would hand it an MTI capability the hardware did not have. **Consequence: chaff has no channel against `sa2`** ([`module.md`](module.md) G4) |
| `Channels` | 1 | [T4] — three rounds, one target |
| `RoundsPerEngagement` | 3 | [T4] "three missiles at the same time, albeit all at a single target" |
| `RoundsDefault` | 6 | [T4] six launchers per battalion |
| `ReactionS` | **30** | **[SET, TODO]** — no source found. Deliberately a free mission lever (`set reaction_s`), because this is the number every anchor argues about and none quantifies |
| `LaunchElevDeg` / `GatherS` | 80 / 3.0 | **[SET]** — the launcher is near-vertical in every photograph of the type; 3 s of gathering for a two-stage round whose booster burns ~4–5 s. Both are settings and both are visible in telemetry |
| `Mobile` | false | it is a battalion with a hexagonal earthwork |

**Binds the shooter:** **yes, to impact.** Command guidance means the Fan Song must track target *and*
round; `FBSeekerHandoverS = -1`, the emission stays `Guidance`, and the moment the track breaks the round
goes `INERTIAL` and stays there.

**When killed:** `Radar` → blind and silent, no engagement possible. `Stores` → tracks and radiates and
never launches. `Structure` → `kill unit` met. **When countered:** by breaking the track (beam manoeuvre
is useless without a notch — the only way is to leave the volume or the range gate), and by nothing else
that exists in the tree.

**The low-altitude escape is real and sourced:** the 450–500 m altitude floor plus the 8 km minimum range
are exactly why an attacker goes low against this system, and both are envelope numbers the launch test
reads.

---

## `sa3` — S-125 Neva/Pechora, the low-altitude complement

| Quantity | Value | Source |
|---|---|---|
| Guidance | **RF CLOS** (command line of sight) | [T4] [Wikipedia, S-125](https://en.wikipedia.org/wiki/S-125_Neva/Pechora) |
| Engagement range | **3.5–35 km** | [T4] ibid. |
| Altitude band | **100 m – 18 km** | [T4] ibid. |
| Fire-control set | SNR-125 "Low Blow", **I/D band**, 40 km (80 km in a secondary mode), 250 kW | [T4] ibid. |
| Search set | P-15 "Flat Face", C band, 250 km, 380 kW | [T4] ibid. |
| Missile V-601/5V27 | warhead **70 kg** frag-HE with **4 500 fragments**; Mach 3–3.5; two-stage solid; 6 090 mm × 375 mm | [T4] ibid. |
| Reaction time | **not stated** | [TODO] |
| Guidance channels | separate target-tracking and missile-guidance antennas; count not established | [TODO] |

**FlightBox row:** `v601` store, `CommandGuided`, `WarheadKg = 70` [T4], `FuzeRadiusM = 10` **[SET]** (the
AIM-120's figure for a comparable-order head). `EnvMinM/EnvMaxM = 3 500 / 35 000`, `EnvAltMinM/MaxM =
100 / 18 000` [T4]. `TrackRangeM = 40 000`, `SearchRangeM = 250 000` [T4]. `SearchFrameS = 6.0` **[SET]**,
as above. `DopplerNotchMs = 0` **[SET]**, same argument as `sa2`. `Channels = 1`,
`RoundsPerEngagement = 2` **[SET]** — the launcher is a twin/quad rail and the doctrine is a salvo;
the sourced count is open [TODO]. `ReactionS = 30` **[SET, TODO]**. `LaunchElevDeg / GatherS = 70 / 2.5`
**[SET]**, a lighter two-stage round than the V-750.

**Binds the shooter:** yes, to impact. **When killed / countered:** as `sa2`.

**Why it is a separate row and not a variant of `sa2`:** the altitude floor. 100 m against 450 m is the
whole difference between "go low and live" and "go low and die", and it is the reason the two systems were
always deployed together.

---

## `sa6` — 2K12 Kub/Kvadrat, the mobile SAM that binds through illumination

The one row with a **[T3]** source, and the one where the sources disagree about the envelope.

| Quantity | [T4] Wikipedia | [T3] Air Power Australia |
|---|---|---|
| Engagement range | **4–24 km** | base **6–22 km**; M3 **4–25 km** |
| Altitude band | **50–14 000 m** | base **100–7 000 m** (12 000 with an asterisk); M3 **20–8 000 m** (12 000*) |
| Reaction time | 22–28 s, variant-dependent | base **26–28 s**; M1/M3 **22–24 s** |
| Simultaneous engagements | 1–2 per battery | **1** across all variants |
| Guidance | command guided with **terminal semi-active radar homing** | **CW semi-active homing** |

Sources: [T4] [Wikipedia, 2K12 Kub](https://en.wikipedia.org/wiki/2K12_Kub) · [T3]
[Air Power Australia, 2K12 Kub/Kvadrat](https://www.ausairpower.net/APA-2K12-Kvadrat.html).

**[DISPUTED], and FlightBox carries the [T3] monograph's base-variant figures**, because they are
variant-resolved and the campaigns that need this system (1973, 1982, 1991, 1999) all fly against
early-to-middle variants. The [T4] figures stay in this table and the mission may clamp with
`set engage_max_m`.

| Quantity | Value | Source |
|---|---|---|
| Radar | 1S91 "Straight Flush", **G/H band**, one vehicle carrying acquisition and CW illuminator | [T4] |
| Acquisition range | ~50 km against a fighter | [T4] |
| Illumination/guidance from | ~28 km | [T4] |
| Missile 3M9 | 599 kg, **59 kg warhead**, Mach 2.8, intercepts up to ~Mach 2 head-on | [T4] |
| Battery | one 1S91 + four 2P25 TELs, **3 rounds per TEL** | [T4] |

**FlightBox row:** `3m9` store, **`FBSeekerKind::SemiActiveRadar` — built, unchanged**;
`WarheadKg = 59` [T4], `FuzeRadiusM = 8` **[SET]** (between the AIM-120's 10 m for a larger head and the
AIM-9's 6.0 m for a smaller one). `EnvMinM/MaxM = 6 000 / 22 000`, `EnvAltMinM/MaxM = 100 / 7 000` [T3].
`TrackRangeM = 28 000` [T4] — **the illumination range, not the acquisition range**, because that is what
the weapon needs. `SearchRangeM = 50 000` [T4], `SearchFrameS = 6.0` **[SET]**. `DopplerNotchMs = 40`,
`NotchRejects = false` — **[SET]**, the tree's generic value: a CW illuminator is by construction a Doppler
device, so a beam manoeuvre must work against it, and the generic constant is used rather than an invented
one. `Channels = 1` [T3]. `ReactionS = 26` [T3]. `RoundsDefault = 3` [T4]. `Mobile = **true**`,
`LaunchElevDeg / GatherS = 45 / 2.0` **[SET]** — a rail-launched round from an inclined TEL.

**Binds the shooter:** **yes, to impact, and this is the row where FlightBox already models it.** The
`SemiActiveRadar` seeker is alive exactly while the illumination is fresh and **never comes back**
(`weapons.md` §10.2, measured: an unbroken chain hits at 0.442 m, a chain broken at 15.5 km misses by
27.04 m). Everything needed for `sa6` on the weapon side is built.

**When killed:** as the others, plus one difference — the radar and the launcher are **different vehicles**
in reality and **one unit** in FlightBox. A mission that wants to model killing the Straight Flush while
the TELs survive declares two units and gives the launcher `rounds 3` with no radar. That is a mission
decision, not a class one.

**When countered:** chaff has a channel here (the notch), and the illumination break is the decisive one.

**Mobility:** `set scoot_s` — [`../../campaigns/w4-allied-force.md`](../../campaigns/w4-allied-force.md)
states explicitly that shoot-and-scoot cycle times are **not sourced** and that its missions must declare
their own `[SET]` timings once `C1` exists. This row therefore ships **no** default scoot time.

---

## `sa8` — 9K33 Osa, the self-contained mobile battery

| Quantity | Value | Source |
|---|---|---|
| Guidance | **RF CLOS** (radio command to line of sight) | [T4] [Wikipedia, 9K33 Osa](https://en.wikipedia.org/wiki/9K33_Osa) |
| Engagement range | **1.5–15 km**, variant-dependent | [T4] ibid. |
| Altitude band | **10–12 000 m** (latest variants) | [T4] ibid. |
| Radar 1S51M3 "Land Roll" | **H band (6–8 GHz)** acquisition, **J band (14.5 GHz)** engagement | [T4] ibid. |
| Detection / acquisition / tracking | 30 km / 35 km / ~20 km | [T4] ibid. |
| Reaction time, detection → launch | **≈26 s** | [T4] ibid. |
| Simultaneous | **two missiles against one target** | [T4] ibid. |
| Optical backup | 9Sh33 electro-optical tracker "for ECM-heavy environments" | [T4] ibid. |
| Missile 9M33 | warhead **15 kg** (M3) / **19 kg** (earlier); launch mass 170/126 kg; ⌀209.6 mm; 3 158 mm; ~Mach 2.9 peak; contact **and proximity** fuzing | [T4] ibid. |
| Vehicle | six rounds in containers (AKM), amphibious 6×6, crew 5 | [T4] ibid. |

**FlightBox row:** `9m33` store, `CommandGuided`, `WarheadKg = 19` [T4] (the earlier variant, matching the
campaign era), `FuzeRadiusM = 6` **[SET]** (the AIM-9's figure for a comparable head).
`EnvMinM/MaxM = 1 500 / 10 000` **[SET within the sourced band]** — the 15 km figure is the late variant;
the campaigns fly against the earlier one, and the file says so instead of averaging.
`EnvAltMinM/MaxM = 25 / 5 000` **[SET within the sourced band]**, same reason; the sourced 10–12 000 m is
the modern figure. `SearchRangeM = 30 000`, `TrackRangeM = 20 000` [T4]. `SearchFrameS = 6.0` **[SET]**.
`DopplerNotchMs = 40`, `NotchRejects = false` **[SET]**, generic. `Channels = 1`, `RoundsPerEngagement = 2`
[T4]. `RoundsDefault = 6` [T4]. `ReactionS = 26` [T4]. `Mobile = true`. `LaunchElevDeg / GatherS =
60 / 1.5` **[SET]**.

**Binds the shooter:** yes, to impact.

**The optical backup channel is NOT modelled** — a `FBSiteOptics` fallback that keeps a command engagement
alive when the radar is jammed would be a countermeasure to a countermeasure that does not exist (`C13`).
Named, not approximated.

**Why this row matters even though it overlaps `sa6`:** it is **one vehicle**. Search, track, guidance and
six rounds in a single unit means one bomb ends the whole engagement capability — the exact opposite of
`sa2`'s six launchers around one van, and a genuinely different SEAD problem.

---

## `zsu23` — ZSU-23-4 Shilka, radar-directed AAA

| Quantity | Value | Source |
|---|---|---|
| Radar RPK-2 "Tobol" ("Gun Dish") | **Ku band**, detection to **20 km** | [T4] [Wikipedia, ZSU-23-4](https://en.wikipedia.org/wiki/ZSU-23-4_Shilka) |
| Radar limitation | "picks up many false returns (ground clutter) under 60 m (200 ft) of altitude" | [T4] ibid. |
| Armament | **four 23 mm** autocannon, **3 400–4 000 rpm** combined | [T4] ibid. |
| Elevation | −4°…+85° | [T4] ibid. |
| Ammunition | **2 000 rounds** (520 per upper, 480 per lower barrel), typically 3:1 HE:AP | [T4] ibid. |
| Effectiveness | "Israeli pilots attempting low-altitude flights to avoid missiles were often shot down by ZSU-23-4s" (Yom Kippur) | [T4] ibid. |
| Muzzle velocity, 23 × 152B | 970 m/s | [T4] [Wikipedia, ZU-23-2](https://en.wikipedia.org/wiki/ZU-23-2) — same round |
| Effective range vs air targets | 2–2.5 km; vertical 1 500–2 000 m | [T4] ibid., same gun family |

**FlightBox row:** `GunKey = azp23` — a new `core/FBGun.h` row beside the M61A1 and the GSh-301, field for
field: calibre 23 mm, **4 barrels**, `RoundsPerMin = 3 400` [T4] (the lower of the sourced pair, stated),
`MuzzleMs = 970` [T4], `Drum = 2 000` [T4], `RoundMassKg` **[TODO]** — the 23 × 152B projectile mass is not
in the read sources and the M61A1's own 0.100 kg is already a declared `[SET]` that every kinetic damage
number hangs on linearly (`weapons.md` §Gaps). `DispersionSigmaRad` **[SET]** at the M61A1's value, because
no dispersion figure was found for either gun and using two different invented ones would be worse.

`SearchRangeM = 20 000`, `TrackRangeM` **[SET] 8 000** — the detection figure is sourced, the tracking one
is not, and 8 km is set as "well outside the weapon's 2.5 km reach so that the gun, never the radar, is the
binding constraint". `EnvMaxM = 2 500`, `EnvAltMaxM = 1 500` [T4]. `SearchElHalfDeg` from the sourced
−4…+85° mount. `Channels = 1`. `ReactionS` **[SET] 8** — a radar-directed gun is a seconds-scale weapon and
no figure was found; it is a mission lever like every other reaction time.

**Binds the shooter:** trivially — a gun tracks what it shoots at, and there is no round in flight to
support. **When killed:** `Radar` failed leaves an optically-directed gun, which FlightBox expresses by the
`zu23` row's mechanism (§below) rather than by a degraded mode — but that is a *different unit*, so a killed
`zsu23` radar simply means no engagement. Named as a simplification.

**When countered:** by altitude, and by nothing else. Its ceiling is 1 500 m and the campaigns' 15 000 ft
floor (W4) is 4 572 m — **this row is the number that makes the floor a decision rather than an arbitrary
altitude**, which is exactly what `../../campaigns/INDEX.md` says the AAA row is for.

**The sourced clutter limit (60 m) is NOT modelled** — it would be terrain/clutter physics the tree does
not have (`C4`). It is quoted here because it says the real system's floor was *higher* than its gun's
reach, and a future round has a documented number to build against.

---

## `zu23` — ZU-23-2, the gun that sees with an eye

| Quantity | Value | Source |
|---|---|---|
| Armament | **two 23 mm**, 2 000 rpm cyclic per cannon, **400 rpm practical** | [T4] [Wikipedia, ZU-23-2](https://en.wikipedia.org/wiki/ZU-23-2) |
| Effective range vs air | **2–2.5 km**; maximum vertical 1 500–2 000 m | [T4] ibid. |
| Muzzle velocity | 970 m/s | [T4] ibid. |
| Sight | **ZAP-23 optical-mechanical**, manually entered target data, limited automatic aiming | [T4] ibid. |
| Feed | 2 × 50-round belts | [T4] ibid. |
| Into action from march | 30 s | [T4] ibid. |

**FlightBox row:** `GunKey = zu23`, 2 barrels, `RoundsPerMin = 800` **[DERIVED]** (2 × 400 practical — the
practical rate is the sourced one for sustained fire and a belt of 50 empties in 7.5 s at cyclic rate).
`Drum = 100` [T4], `ReloadS` **[SET] 10** (a two-man crew changing two belts). `SearchRangeM = 0` — **it
has no radar at all**: acquisition is `FBSiteOptics`, an `FBVisualSystem` derivation.

**This is the row that proves the design.** Its detection range is not a catalogue number at all; it is the
eye's measured behaviour — 3 784 m beam-on, 2 493 m head-on against an F-16, **zero at night**
(`sensors.md` §9 State). So this gun engages a crossing target at the edge of its own gun range and a
head-on target barely at all, and it is blind after dark. Nothing was tuned to produce that.

**When killed / countered:** by altitude, by darkness, by haze and by cloud — the eye's five currencies,
inherited.

---

## `sa7` — 9K32 Strela-2M, the rear-aspect MANPADS

| Quantity | Value | Source |
|---|---|---|
| Seeker | AM reticle, **uncooled PbS**, below 2.8 µm, **1.9° field of view** | [T4] [Wikipedia, 9K32 Strela-2](https://en.wikipedia.org/wiki/9K32_Strela-2) |
| Aspect | **rear-aspect only** against jets — "the seeker can only see infrared energy emitted by very hot surfaces only seen on the inside of the jet nozzle"; a "revenge weapon" | [T4] ibid. |
| Engagement range | **800–4 200 m** (Strela-2M) | [T4] ibid. |
| Altitude | **50–2 300 m** (Strela-2M; 1 500 m for the Strela-2) | [T4] ibid. |
| Speed | 500 m/s (2M) | [T4] ibid. |
| Warhead | **1.15 kg**, 370 g explosive | [T4] ibid. |
| Reaction time | **6–10 s** with the launcher ready, 13 s from the carrying position | [T4] ibid. |
| Countermeasures | "Flares proved to be a highly effective countermeasure against both versions" | [T4] ibid. |

**FlightBox row:** `strela2` store, **`FBSeekerKind::Infrared` — built, unchanged**. `WarheadKg = 1.15`
[T4], `FuzeRadiusM = 2.0` **[SET]** — a 1.15 kg head is a contact weapon; the smallest fuze radius in the
tree today is the R-73's 3.5 m for a head several times larger, so this row gets less. `EnvMinM/MaxM =
800 / 4 200`, `EnvAltMinM/MaxM = 50 / 2 300` [T4]. `ReactionS = 8` [T4] (mid-band of the sourced 6–10 s).
`SearchRangeM = 0` — acquisition is the eye. `RoundsDefault = 2` **[SET]**, a gunner and a reload.
`LaunchElevDeg / GatherS = 30 / 0.6` **[SET]** — a shoulder launch is nearly line of sight, and the
sourced ejection-then-ignition sequence of the type is not quantified.

**The rear-aspect limitation is modelled, and it costs nothing:** `FBIrstSystem`'s aspect law already
gives an afterburning tail aspect 2.25 and a dry head-on aspect 0.16 of the reference intensity
(`sensors.md` §6, measured). The row declares a minimum required intensity that a head-on target cannot
reach — one number in the derivation's own currency, not a new gate.

**Binds the shooter: NO.** Free at launch (`FBSeekerHandoverS = 0`), which is the whole tactical point of a
MANPADS: there is nothing to break, and **no warning of any kind** is produced (nothing radiates, and there
is no MWS — `sensors.md` gap 8).

**Countered by flares**, measured: an IR round against a head-on target dispensing flares is seduced at
`tgtIntensity = 0.16` and misses by 22–26 m (`weapons.md` §State, stage 2c). That behaviour is built.

---

## `sa18` — 9K38 Igla, the all-aspect MANPADS

| Quantity | Value | Source |
|---|---|---|
| Seeker | Igla-1: nitrogen-cooled **InSb** plus an uncooled PbS channel **for detecting flares**. Igla: IR **and UV**, "which decreased its susceptibility to flares" | [T4] [Wikipedia, 9K38 Igla](https://en.wikipedia.org/wiki/9K38_Igla) |
| Engagement range | Igla-1 5.0 km · Igla **5.2 km** · Igla-S 6.0 km | [T4] ibid. |
| Ceiling | **3.5 km** across variants | [T4] ibid. |
| Aspect | "capability to engage straight-approaching fighters (all-aspect capability) **under favourable circumstances**" | [T4] ibid. |
| Speed | 570 m/s peak (~Mach 1.9) | [T4] ibid. |
| Warhead | **1.17 kg**, 390 g explosive | [T4] ibid. |
| Reaction time | not stated | [TODO] |

**FlightBox row:** `igla` store, `Infrared`, `WarheadKg = 1.17` [T4], `FuzeRadiusM = 2.0` **[SET]** as
`sa7`. `EnvMaxM = 5 200`, `EnvAltMaxM = 3 500` [T4]; `EnvMinM = 500` **[SET, TODO]** (no minimum published
in the read source; every MANPADS has one). `ReactionS = 8` **[SET]**, the Strela's sourced figure for want
of one of its own.

**The difference from `sa7` is modelled in exactly one quantity: aspect.** Its required intensity is set at
the dry head-on value, so it can take a frontal shot the Strela cannot. **Its documented flare resistance
is NOT modelled** — FlightBox's flare model is an irradiance inequality with no rejection term, and
inventing one would be inventing the number that decides the shot ([`module.md`](module.md) G8).

**The consequence, stated plainly:** in FlightBox today an Igla is a Strela with a wider aspect window and
a kilometre more reach, and it is **equally** flare-defeatable. That is a known understatement of the real
weapon, in the same class as the R-73's missing thrust vectoring, and it is declared where the row is
rather than discovered in a result.

---

## Disputes left standing

| # | Subject | The two values | FlightBox |
|---|---|---|---|
| D1 | S-75 maximum range | 30 km [T4 Military Wiki] vs 45 km operational [T4 Wikipedia] | takes 30 km (the Dvina-class figure the campaign anchors fly against), carries both |
| D2 | S-75 minimum range | 8 km [T4] vs a 500–1 000 m short-range cutoff [T4] | takes 8 km as `EnvMinM`; the two are probably *different quantities* (the tactical minimum against the guidance cutoff) and the file says so rather than resolving it |
| D3 | 2K12 envelope | 4–24 km / 50–14 000 m [T4] vs 6–22 km / 100–7 000 m base [T3] | takes the [T3] variant-resolved base figures; `set engage_max_m` can clamp |
| D4 | 2K12 reaction time | 22–28 s [T4] vs 26–28 s base / 22–24 s M1-M3 [T3] | takes 26 s [T3] base |
| D5 | 2K12 simultaneous engagements | 1–2 [T4] vs 1 [T3] | takes 1 |
| D6 | ZSU-23-4 rate of fire | 3 400–4 000 rpm [T4] | takes 3 400, the lower bound, stated |
| D7 | Fan Song "tracks 6 targets" vs "guides 3 missiles at one target" | both [T4], from two entries | kept as two different quantities: **track capacity ≠ engagement channels**; FlightBox models the second (`Channels = 1`) |

## What is not sourced at all

| Field | Rows | Status |
|---|---|---|
| Reaction time | `sa2`, `sa3`, `zsu23`, `zu23`, `sa18` | `[SET, TODO]` — and the reason `set reaction_s` exists |
| Antenna revolution rate | every row except `p18` | `[SET]` at the P-18's sourced 10 rpm |
| 23 × 152B projectile mass | `zsu23`, `zu23` | `[TODO]` — it multiplies every kinetic damage number linearly |
| Gun dispersion | `zsu23`, `zu23` | `[SET]` at the M61A1's value |
| Missile aerodynamic decks | all six rounds | `[DERIVED]` per the AIM-120 slender-body recipe; no public aero deck exists for any of them |
| Shoot-and-scoot cycle | `sa6`, `sa8` | `[TODO]`, and the campaign that needs it says so |
| Warhead internals / fragment directivity | all | inherited gap of `weapons.md` §6.1 |

## Sources

| Tier | Document |
|---|---|
| [T3] | [Air Power Australia — 2K12 Kub/Kvadrat Self Propelled Air Defence System](https://www.ausairpower.net/APA-2K12-Kvadrat.html) |
| [T4] | [Wikipedia — S-75 Dvina](https://en.wikipedia.org/wiki/S-75_Dvina) · [Fan Song](https://en.wikipedia.org/wiki/Fan_Song) · [Military Wiki — S-75 Dvina](https://military-history.fandom.com/wiki/S-75_Dvina) |
| [T4] | [Wikipedia — S-125 Neva/Pechora](https://en.wikipedia.org/wiki/S-125_Neva/Pechora) · [2K12 Kub](https://en.wikipedia.org/wiki/2K12_Kub) · [9K33 Osa](https://en.wikipedia.org/wiki/9K33_Osa) |
| [T4] | [Wikipedia — ZSU-23-4 Shilka](https://en.wikipedia.org/wiki/ZSU-23-4_Shilka) · [ZU-23-2](https://en.wikipedia.org/wiki/ZU-23-2) |
| [T4] | [Wikipedia — 9K32 Strela-2](https://en.wikipedia.org/wiki/9K32_Strela-2) · [9K38 Igla](https://en.wikipedia.org/wiki/9K38_Igla) · [P-18 radar](https://en.wikipedia.org/wiki/P-18_radar) |

**Identified and NOT read** (the [T1] material that would move the envelope numbers): the US Army TRADOC
*Worldwide Equipment Guide*, service threat handbooks of the FM 44 series, and the CIA reading room's
Soviet air-defence assessments. Naming them is the honest form of the gap.
