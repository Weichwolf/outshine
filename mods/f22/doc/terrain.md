# F-22 Lightning II — Campaign One: terrain, scale, anchoring, bounding boxes

> **Source document:** research distillation
> `scratchpad/novalogic/f22.md`, §1 (l. 42–81), §4 (l. 200–300), §10 (l. 531–549).
> All arithmetic **re-derived in this run** and reproducing to ±0.001° — see §2.4.
> §5 ("why this real region") is **not in the source document**; it is written here because
> [`doc/mods.md`](../../../doc/mods.md) §3 requires it and it has no other home.
> Form per `doc/mods.md` §3. A mod has no `test/`.

**Read this section first — it decides how everything below may be used.**

| | What it covers | Confidence |
|---|---|---|
| **Measured** | the campaign's **internal geometry** — where every object sits relative to FOB Tyler, in metres, to 5 m | **high**, 0.22 % residual scale error |
| **Reconstructed** | the **anchoring of that geometry on the real Earth** — one real anchor point plus the *assumption* that game +Z is geographic north | **medium**, and the north assumption is **not proven** |
| **Assigned** | the match of individual game terrain features to real ones (a bridge, a river, an airfield site) | **low**, and for water features it is **measurably wrong** — §6 |

NovaLogic's voxel terrain is invented. It has **no projection and no datum**, and the heightfield is not
in `RESOURCE.RES` at all. Nothing below turns the game's terrain into real terrain; it places the
game's *layout* onto a real region whose *character* matches.

## Spec

### 1. Scale — measured, not assumed

The `.ORF` object records carry three `int32` as (X, Y, Z) = (east, altitude, north) in game units.
Ground objects carry Y = −2; flying groups carry their altitude (2,500 / 5,000 / 10,000 / 20,000).

Calibration: for all eight missions the base→objective distance was taken in game units and set
against the briefed distance.

```
Σ units      = 167,051                 [ORF, source research]
Σ briefed NM =     450                 = 50+45+35+60+40+65+70+85   [TXT]

167,051 / 450        = 371.22 units/NM
1852 / 371.22        =   4.9889 m per unit
nearest round value  =   5 m  →  1852/5 = 370.4 units/NM
residual             = (371.22 − 370.4)/370.4 = +0.222 %
```

**One unit = 5 m.** The hit on a round number at 0.22 % residual is the confirmation, not the fit
itself. Cross-check on airfield geometry: the base spans **356 units = 1,780 m ≈ 5,840 ft** of runway
`[ORF]` — a plausible military field, and 220 m short of the real Nan Nakhon runway (§5.3).

The remaining scatter sits in the briefings, which round to 5 NM, not in the data.

**Unresolved inconsistency in the source, carried forward deliberately:** the source concludes "one
unit = 5 m" but then does all of its coordinate arithmetic with **4.989 m**. Both are recorded below;
§2.3 states what the difference costs.

### 2. Anchoring on the real Earth — reconstruction, two assumptions

#### 2.1 What the reconstruction needs

1. **A real anchor point.** Mission 1.7 contains the airfield at **Chiang Rai** as an object group, and
   the briefing names the place. That is the game naming a real location — the only such handle in the
   campaign.
2. **The assumption that game +Z points at geographic north.** Supported by all eight measured bearings
   landing in the briefing's stated compass octant (mean offset +9.2°). **Supported, not proven.**

#### 2.2 FOB Tyler, determined by measurement

The base sits at the same game coordinates (≈ 28,958 / 18,665 units) in all eight missions `[ORF]`, so
the Chiang Rai → FOB Tyler vector is directly measurable:

```
measured [ORF]:  26,448 units, back-bearing 155.5°
                 26,448 × 4.989 m = 131.95 km

Chiang Rai    = 19.90944 N / 99.82750 E              [Wikipedia]
Δlat = 131.95 · cos 155.5° / 111.32              = −1.0786°
Δlon = 131.95 · sin 155.5° / (111.32 · cos 19.37°) = +0.5210°
                              └ mean latitude of the two points
```

```
FOB TYLER = 18.8308 N / 100.3486 E
```

**This resolves a contradiction in the briefings rather than choosing between them.** One briefing says
the base is *"south of Chiang Rai"* (180°); another says *"Chiang Rai 70 NM NW of our base"*
(back-bearing 135°). The measurement gives **155.5°** — between the two. **Both prose statements are
imprecise and neither was usable.** An earlier reconstruction that took the 180° statement literally
(18.745 N / 99.828 E) is thereby superseded and is not carried here.

#### 2.3 What the 5 m / 4.989 m split costs

| Unit length | Chiang Rai → Tyler | FOB Tyler |
|---|---|---|
| 4.989 m (used by the source, used here) | 131.95 km | 18.8309 N / 100.3485 E |
| 5.000 m (the source's own conclusion) | 132.24 km | 18.8285 N / 100.3497 E |

`[DERIV]` Difference **≈ 0.27 km**, growing linearly with distance from the anchor. Against a bounding
box carrying a 10 NM (18.5 km) margin this is irrelevant; against "put the runway *here*" it is not.
**The coordinates in §3 use 4.989 m.** If a rebuild prefers the round 5 m — defensible, since the game
almost certainly used it — every coordinate shifts by 0.22 % away from Chiang Rai and must be
recomputed, not nudged.

#### 2.4 Verification performed in this run

Recomputed from the eight measured offsets alone: all eight distances, all eight bearings, all eight
target points and all eight bounding boxes reproduce the source's table to **±0.1 NM, ±0.1°, ±0.001°**.
The campaign box edges reproduce as 258.9 × 218.6 km against the source's "about 258 × 219 km".
One 0.001° discrepancy exists between the source's mission-1.6 `min_lon` (99.125) and its campaign
`min_lon` (99.124) — 100 m, a rounding artefact. **The lower value is used.**

This tests **internal consistency only.** It does not test that the `.ORF` bytes were parsed correctly,
because the archive was not fetched in this run.

### 3. Bounding box per mission

Method: target point = FOB Tyler + measured offset; box encloses **base and target plus a 10 NM
margin**, so it carries takeoff, ingress and target. Latitude conversion 111.32 km/°, longitude
111.32 · cos(mean latitude) km/°.

| # | Name | Offset `[ORF]` | Target point | min_lat | min_lon | max_lat | max_lon |
|---|---|---|---|---|---|---|---|
| 1.1 | Snake Eyes | 25.9 km W · 97.3 km N | 19.705 N, 100.102 E | 18.664 | 99.926 | 19.871 | 100.525 |
| 1.2 | Party Crashing | 53.6 km E · 65.3 km N | 19.417 N, 100.859 E | 18.664 | 100.172 | 19.584 | 101.035 |
| 1.3 | Luckiest Man in Laos | 58.0 km E · 33.1 km N | 19.128 N, 100.900 E | 18.664 | 100.172 | 19.295 | 101.076 |
| 1.4 | Silkworm Jungle | 71.4 km E · 76.6 km N | 19.519 N, 101.028 E | 18.664 | 100.172 | 19.685 | 101.204 |
| 1.5 | Double Down | 30.3 km E · 68.5 km S | 18.216 N, 100.636 E | 18.050 | 100.172 | 18.997 | 100.812 |
| 1.6 | Four of a Kind | 110.6 km W · 48.8 km S | 18.393 N, 99.300 E | 18.226 | 99.124 | 18.997 | 100.525 |
| 1.7 | Aces Low | 54.7 km W · 120.1 km N | **19.909 N, 99.828 E** (= Chiang Rai, the anchor) | 18.664 | 99.651 | 20.076 | 100.525 |
| 1.8 | Black Mariah | 48.7 km E · 153.5 km N | 20.210 N, 100.813 E | 18.664 | 100.172 | 20.376 | 100.989 |

FOB Tyler is inside every box by construction: **18.8308 N / 100.3486 E**.

### 4. Campaign box

Union of all eight:

| | Value |
|---|---|
| min_lat | **18.050** |
| min_lon | **99.124** |
| max_lat | **20.376** |
| max_lon | **101.204** |

`[DERIV]` Edges **258.9 km** N–S (2.326° × 111.32) × **218.6 km** E–W (2.080° × 111.32 · cos 19.21°).
Contains Chiang Rai, Chiang Mai, the Golden Triangle tri-point and the northern Laotian share.

### 5. Why this real region — and where the assignment is weak

`doc/mods.md` §3 requires this and it is not in the source document. It is the scenario's intent and
belongs nowhere else.

#### 5.1 The strongest reason: the fiction names the place itself

This is not a resemblance argument. The game states the geography in three independent places:

| Statement | Where |
|---|---|
| "In the verdant hills bordering **Laos, Myanmar** (formerly Burma) **and Thailand** lies the **Golden Triangle**" | `[MAN]`, campaign one prose, verbatim |
| "Chang's power now stretches throughout **northern Myanmar and Thailand**, as well as parts of **Laos**" | `[MAN]`, verbatim |
| **Chiang Rai** exists as a named, placed airfield object group | `[ORF]` mission 1.7 |
| "a bridge over the **Mekong River**" · "a southern stretch of the **Mekong River**" | `[ORF]` 1.3, `[TXT]` 1.5 |

So the region is not selected by us. **The only reconstruction step is the anchor and the north
assumption** (§2.1); the region itself is the game's own claim. That is the strongest form this
argument can take, and it is why an anchored rebuild is defensible at all.

#### 5.2 The terrain character matches

| Source says | Real region inside the box |
|---|---|
| "Southeast Asian **jungle**" `[TXT 1.1]` | dense monsoon broadleaf forest |
| "**verdant hills**", "lush, **hilly** terrain" `[MAN]` | forested ranges with high relief energy, cut by the Mekong, Ruak and Kok valleys |
| no coastline anywhere in eight missions | the box is entirely inland |
| eight targets at 35–87 NM from one base | one province-scale theatre; northern Thailand + northern Laos supplies exactly that without crossing a coast, a desert or a second climate |

The **scale** match matters as much as the type. The campaign never needs a carrier, a shoreline, open
sea or an urban centre. A 259 × 219 km inland box of forested uplands with one airfield at each end is
precisely what the eight missions consume.

The manual's own campaign summary confirms the intent was terrain contrast between campaigns — *"each
featuring a different type of predominant terrain, from tiny islands to the snow-capped mountain peaks
of Siberia"* `[MAN]`. Campaign one's share of that contrast is the green hill country, and that is what
the real box delivers.

#### 5.3 FOB Tyler on real ground

The reconstructed base at 18.8308 N / 100.3486 E falls in the Nan uplands of northern Thailand. There
is no real airfield at that point.

| | Value | Provenance |
|---|---|---|
| Nearest real airfield: **Nan Nakhon (VTCN/NNT)** | 18.80778 N / 100.78333 E, elev 209 m, runway **2,000 m** | Wikipedia |
| `[DERIV]` distance from the reconstructed FOB | **45.9 km**, nearly due east (Δlat 0.023° = 2.6 km) | — |
| Game's measured runway length | **1,780 m** `[ORF]` | §1 |

The latitude agreement to 2.6 km and the runway-length agreement to 220 m are a **coincidence, not
corroboration** — nothing in the game refers to Nan. But it is a useful coincidence: a real field of
almost the right size exists at almost the right latitude.

**Position taken: keep the computed point, do not snap the base to VTCN.** The entire internal geometry
is measured *relative to FOB Tyler*; moving the base 46 km east while leaving the eight target vectors
attached to it drags every objective 46 km east too, which walks 1.7's target off Chiang Rai — the one
real place the game actually names. The reconstruction has exactly one anchor and it is Chiang Rai; the
base is derived from it and must stay derived. Fidelity to the anchor beats convenience at the base.

Cross-check on that anchor `[DERIV]`: the game's Chiang Rai object group sits **7.5 km** from the real
Chiang Rai International Airport (VTCT, 19.95230 N / 99.88290 E) — because the anchor was taken as the
*city* centroid, not the field. A rebuild that wants the 1.7 attack to run against a real runway should
consider re-anchoring on VTCT instead, which shifts everything 7.5 km NE and is a **decision, not a
correction**; it is not made here.

#### 5.4 Where the assignment is weak — say it, do not smooth it

1. **The heightfield is invented and was never recovered.** Real DEM under this box gives terrain of
   the right *kind*, never the same hills. No ridge, valley or lake will line up. Every objective sits
   at a computed coordinate whose real elevation is now MEASURED (§8) but is still a coordinate the
   game never placed against real ground — FOB Tyler lands on a 741.9 m hilltop, and that is an
   accident of the reconstruction, not a claim about the game.
2. **Water features do not survive the anchoring at all** — §6. This is the one place where the
   assignment is not merely unproven but measurably false.
3. **Country assignment per target point is not verified.** The source research places 1.2, 1.3 and 1.4
   in northern Laos, which is consistent with the fiction ("moving into Laos", the base "in Laotian
   territory"). **No border dataset was consulted**, here or in the source. Treat every "this is in
   Laos / this is in Thailand" statement about a target point as TODO.
4. **Elevation range was unsourced and is now measured.** The source states "forested uplands
   300–2,000 m" from a description. `[MEAS]` the baked raster (§8) gives **97–2 547 m** over the box
   and **271–1 485 m** under the eight sorties' tracks, so the description was low at both ends. The
   independent datum still holds: the raster answers **208.34 m** at VTCN against a published 209 m.

### 6. The river problem — two missions, not one

The source research names this defect for mission 1.3 and stops there. **It applies to 1.5 as well, and
worse.**

`[DERIV]` distances from each target point to the two documented points on the real Mekong inside or
near the box — the Golden Triangle tri-point at Sop Ruak (20.35556 N / 100.08139 E) and Pak Beng
(19.85 N / 101.55 E):

| Mission | Target point | Fiction says | → Sop Ruak | → Pak Beng |
|---|---|---|---|---|
| **1.3** | 19.128 N / 100.900 E | "a **bridge over the Mekong River**" | 161 km | **105 km** |
| **1.5** | 18.216 N / 100.636 E | "Nanuchka Missile Boats patrolling a **southern stretch of the Mekong**" | 244 km | **205 km** |

Both are anchored far from any reach of the real river. Mission 1.5's target sits in the Nan/Phrae
uplands where there is no navigable water of any kind — and the manual's own ship entry `[MAN p.97*]`
is a **Nanuchka-III class corvette with two triple SS-N-9 launchers**, which is a seagoing warship. The
fiction was already implausible before anyone anchored it.

**Not patched, and not to be patched.** The game terrain is invented; the game's river is wherever
NovaLogic's voxels put it, and no rotation, translation or rescale of one anchor point brings two
targets 105 km and 205 km onto the same real watercourse. Forcing it would trade a *known* mismatch for
a *hidden* one.

What a rebuild must therefore decide, explicitly, per mission:

| Option | Cost |
|---|---|
| keep the computed point, put a fictional bridge / fictional water there | the objective no longer looks like the Mekong from the cockpit |
| move 1.3 and 1.5 onto the real Mekong | breaks the measured geometry — the only thing here with high confidence |
| drop the water framing, keep the target | loses the mission's identity ("Luckiest Man in Laos" is *about* a bridge) |

No option is chosen here. **It is a decision, it is recorded as one, and it must not be taken by
accident inside a `.fbm`.**

### 7. Real places named or implied

| Place | Role | Coordinate | Provenance |
|---|---|---|---|
| Chiang Rai | **anchor point**; enemy airfield in 1.7 | 19.90944 N / 99.82750 E | Wikipedia |
| Chiang Rai Intl. (VTCT) | real field at that place | 19.95230 N / 99.88290 E | Wikipedia |
| Chiang Mai | threatened district | 18.79528 N / 98.99861 E | Wikipedia |
| Chiang Mai Intl. (VTCC), RTAF Wing 41 | real field | 18.76667 N / 98.96250 E | Wikipedia |
| Nan Nakhon (VTCN) | nearest real field to the reconstructed FOB, §5.3 | 18.80778 N / 100.78333 E, 209 m, rwy 2,000 m | Wikipedia |
| Golden Triangle tri-point, Sop Ruak | names the campaign `[MAN]` | 20.35556 N / 100.08139 E | Wikipedia |
| Pak Beng | documented Mekong point inside the box's east flank, §6 | 19.85 N / 101.55 E | latitude.to |
| Kunming, PRC | stated origin of the Silkworms in 1.4 | 25.03889 N / 102.71833 E | latitudelongitude.org — **far outside the box**, narrative only |
| Bangkok | Chang's political objective | **not obtained** | — |
| FOB Tyler · Objective Madison · Objective Talbot | fictional; located only by §2–§3 | — | — |


### 8. The baked DEM — what the campaign is actually flown over

The eight sorties run on `../src/data/mekong-dem-90m.bin`, an offline raster of this box.
`mod.json` names it (`"dem"`), so `fb-gym`'s elevation default over this mod is `baked`. The file is a
BUILD OUTPUT and is not committed (`doc/assets.md` §0); the recipe is
`sim/tools/bake_dem.py --region mekong`.

| Decision | Value | Why |
|---|---|---|
| Box | **17.90–21.70 N / 98.85–102.35 E** | `[DERIV]` §4's campaign box does NOT enclose the sorties: their CAP spawn points sit outside it, up to 21.506 N / 102.156 E. The DEM box is the union of §4 and every lat/lon the eight files name, plus ~0.15° of margin, rounded outward to 0.05°. Outside a baked box `FBBakedDemElevation` answers 0 m, and a cliff at the boundary is worse than a wider file |
| Source zoom | **13** = 18.0 m/px at 19.8 N | `[SET]` the same zoom `tiles/src/elev.c`'s `FB_DEM_Z` samples. The baked surface is therefore the SAME surface `--elev tiles` would answer, and the only difference left to measure is the output grid |
| Output spacing | **90 m** (4 076 × 4 675, 38.11 MB) | `[SET]` the f16 fixture's spacing, so the two theatres' ground is comparable. `[MEAS]` its cost against the 18 m source is **rms 3.96 m** over 400 interior points (bias +0.28, max 15.74) — an order of magnitude below the terrain features that decide anything here |
| Edge blend | **none** | the f16 fixture ramps its outer 15 km to 0 m because it is an ISLAND. This ground continues past its own box, and a ramp would invent a cliff exactly where 1.7's and 1.8's tracks cross the north edge |
| Degree lengths | WGS84 at 19.80 N: 110 702 m/° lat, 104 786 m/° lon | `[DERIV]` so `90 m` is metres. §4's own 111.32 km/° for latitude is the equator's LONGITUDE degree and is 0.56 % wrong here — corrected for the raster, deliberately NOT for §3's coordinates, which would be false precision under an unproven north (see Gaps) |

**What the ground can and cannot do to a sortie.** Exactly one sensor in the tree samples terrain:
`sensors/FBGroundMap`, the fire-control radar's ground-MAPPING mode, which does compute a local grazing
angle and geometric shadowing behind every crest. It writes a PICTURE — `FBGroundMapBlock`, read only
by `systems/FBDisplaySystem` and `render/stages/FBGroundMapStage` — and never a detection, a track or a
launch gate. **No air-to-air radar mode, no IRST, no visual channel and no RWR carries a terrain term
at all**, `sensors/FBDatalinkSystem.h` says so in its own comment, and no sortie in this campaign
selects `fcr_mode gm` anyway. Terrain reaches these eight files through exactly four doors:

1. **CFIT** of aircraft, missiles and bombs (`core/FBFlightMonitor`, ground penetration).
2. **The launch altitude of a ground site** — a battery on 395 m of hill flies a different intercept.
3. **AGL-driven pilot behaviour**, and the spawn validator's "explicit altitude below ground" FAIL.
4. **The datalink's radio horizon**, which is computed over AGL and not ASL — `[MEAS]` door four is
   theoretical here: **zero** `horizon` events in either run, flat or real.

Every detection range in this campaign is therefore STILL an upper bound. The reason changed: it used
to be a missing asset, and it is now a named engine gap.

## State

**All eight target points of §3 are flown on the real ground of §8**, in `../src/missions/`. §2.4's
arithmetic was re-derived a third time when the files were written and reproduces to ±0.00001° — e.g.
§3's Chiang Rai row (19.909 N / 99.828 E) comes out as 19.90868 / 99.82791 from the offsets alone.

### The flat plane against the real ground, all eight

Exit codes only; the reading rule of each file, not the code, is the verdict. Column **flat** is the
old `--elev const` 0 m plane, **real** is the same files unchanged on the DEM, **repaired** is after
the altitude corrections T2 that real ground forced on 1.3 and 1.5. Determinism checked at
`--threads 1/2/4`, byte-identical.

| # | Sortie | flat | real | repaired | Where the difference comes from |
|---|---|---|---|---|---|
| 1.1 | Snake Eyes | 3 | 3 | 3 | **nothing.** Every launch, DETONATION and miss distance identical to the digit; only the shot-down MiG's wreck lands 1.9 s earlier (428 m of ground) |
| 1.2 | Party Crashing | 3 | 3 | 3 | SAM slant range 29 946 → 29 885 m (site on 692 m). Same six launches, same seconds, house still destroyed. CCRP plane 0 → 347 m against a 319 m target: absorbed |
| 1.3 | Luckiest Man | 1 | **2** | 1 | 900 m run-in flies into a 967 m shoulder → CFIT at t = 63.1 s. Repaired to 1 900/2 050 m: one span now falls (flat: neither), the leader is lost instead of the wingman |
| 1.4 | Silkworm Jungle | 0 | 0 | 0 | **nothing** at 8 000 m; only the escort's wreck lands 2.7 s earlier |
| 1.5 | Double Down | 1 | **2** | 1 | 900 m spawn is 44 m over the hillside → STRUCTURE_CONTACT at t = 7.4 s. Repaired to 1 850/1 500 m: both boats destroyed again, escort fight unchanged to the digit (t = 91.7 s, missM 4.5826) |
| 1.6 | Four of a Kind | 3 | 3 | 3 | **byte-identical**, all nine actors, whole telemetry |
| 1.7 | Aces Low | **1** | **3** | 3 | the SA-2 launcher stands on 395 m. Same six rounds, same seconds, launch ranges 48–86 m shorter — and the lethal round moves from #4 at 8.20 m to #5 at 11.80 m, which does NOT kill. `s7lead` lives, `s7two` destroys one parked EF2000 |
| 1.8 | Black Mariah | **1** | **3** | 3 | same battery effect: all six v-750 miss, the sweep survives, the run reaches the release — and then BOTH mk84 fall 215 m short because the CCRP plane is the release point's 511.9 m and the target sits at 834.4 m |

Three sorties changed verdict and none of them changed because a sensor saw less. **Terrain masked
nothing, anywhere, in any of the eight** (§8, and 1.1/1.4/1.6 are the byte-level proof).

### What the flat plane had been hiding

| Finding | Measurement |
|---|---|
| **The 1.7 headline was a flat-plane artefact.** "The SA-2 kills the attack 12.1 km before the ramp" does not survive real ground | 8.20 m → 11.80 m of miss distance. The margin is 3.6 m: the flat-plane kill was already on a knife edge |
| **The CCRP impact plane is the RELEASE POINT's ground, not the target's.** `FBF16Module.cpp` hands `SetSteerpoint(..., GroundAslM)` — this tick's sample under the aircraft — into `FBF16FireControl::SolveGroundAttack`. Over a 0 m plane the two are equal by construction, so the error was exactly zero and invisible | 1.8: plane 511.9 m vs target 834.4 m = +322.5 m error → **215 m short**, target survives, while the computer reported `aimMissM` 65.3 m. 1.2: 28 m error, absorbed |
| **Two of eight low-level profiles were unflyable** over the ground they were written for | 1.3 CFIT at t = 63.1 s, 1.5 at t = 7.4 s |
| **FOB Tyler is a hilltop** | 741.9 m. Nothing in the reconstruction chose that; it is what §2.2's arithmetic lands on |
| **The "300–2 000 m" description was low at both ends** | 97–2 547 m in the box |

Three decisions §5–§6 left open are TAKEN in the files, each in the header of the sortie that takes it:

| Left open by | Taken as | Where |
|---|---|---|
| §6, the bridge and the Mekong | keep the measured point, put a fictional bridge on it | `c01m03-luckiest-man-in-laos.fbm` |
| §6, the boats and the Mekong | keep the measured point; there is no ship module, so a stationary `target_soft` | `c01m05-double-down.fbm` |
| §5.3, re-anchoring on VTCT instead of the Chiang Rai centroid | **not** taken; the target stays on the centroid, 7.5 km from the real field | `c01m07-aces-low.fbm` |

One decision §8 forced and the files record: **1.3's and 1.5's run-in altitudes are re-derived, their
geometry is not.** The `.ORF` offsets are the one high-confidence thing here and no lat/lon moved.

## Gaps

- **Terrain masks NO DETECTION — not radar, not IRST, not the eye, not the RWR.** Not a mod gap: the
  only terrain-aware sensor in the tree, `sensors/FBGroundMap`, writes a display raster and gates
  nothing, and no other sensor sees an `FBElevationProvider` at all. Every detection range and every
  SAM engagement in the eight sorties is therefore STILL an upper bound, exactly as it was over the
  flat plane, and the eight files say so in D3. Measured: 1.6 is byte-identical over the two grounds.
- **The CCRP impact plane is the release point's ground, not the target's** (State, and
  `c01m08-black-mariah.fbm`'s header). Cost measured once: 215 m short on a 322 m plane error. Fixing
  it needs the module to sample terrain AT the steerpoint, which no module can do today.
- **The `.fbm` altitudes are the campaign's weakest remaining numbers.** 1.3's and 1.5's are now
  derived from the DEM (their own 900 m of clearance, per leg, +-1 km corridor); the other six are
  still the flat-plane figures, kept because the ground does not break them — but nobody has checked
  them against a *tactical* intent, only against the dirt.
- **The north assumption is unproven** and everything in §3 hangs from it. Support is eight bearings
  landing in the right compass octant (mean +9.2°), which is consistent with a rotation of up to about
  ±20°. If the map is rotated, every target point swings around FOB Tyler by that angle — up to 50 km
  at mission 1.8's range, and now over ground that is 97–2 547 m rather than flat, so a rotation would
  also re-decide every altitude in §8's repair.
- **The Mekong does not pass through 1.3's or 1.5's target point** — §6, 105 km and 205 km off. Real
  DEM makes this worse rather than better: 1.5's "southern stretch of the Mekong" is now measurably
  492 m of upland with no water on it, and its Vipers attack boats from 1 350 m.
- **The `.ORF` parse is second-hand.** Re-derivation here proves consistency, not correctness of the
  byte-level read.
- **5 m vs 4.989 m unresolved** (§2.3). 0.27 km at the anchor distance; the coordinates use 4.989 m
  while the source's own conclusion is 5 m.
- **§3's latitude conversion uses 111.32 km/°, which is the *longitude* constant at the equator.** The
  meridional degree at 19.4 N is 110.70 km `[DERIV]`, so every northward offset is understated by
  0.56 % — `[DERIV]` **0.87 km at mission 1.8's 153.5 km**. §8's raster uses the correct value; §3's
  coordinates deliberately do not, because correcting them alone while the north assumption stands
  would be false precision.
- **The voxel heightfield was never decoded**, so the §5.2 character match is still a match of
  *descriptions* against a real surface, not of two surfaces. `.REF` files (49 × 24,576 bytes) remain
  uninspected.
- **Country borders never consulted**, so §5.4.3 stands open.
- **Nine actors leave the DEM box before the run ends and the ground under them reads 0 m there.**
  `[MEAS]` `s7lead/s7two/s7esc` at 24.6–24.8 N / 97.5 E, `s8ef` at 22.67 N, `s6esc1/s6esc2/s6mig2` at
  17.5–17.7 N / 97.6–98.1 E, plus two rounds in flight — all of them past their last waypoint, all of
  them at 3 000–6 250 m ASL, so nothing any verdict reads depends on it. Widening the box further
  would only chase a pilot that has run out of plan; the honest fix is a plan that ends somewhere.
- **Nothing has been flown under `--elev tiles`.** The baked raster is the same z13 surface the live
  server samples (§8) and agrees with `/elev` to rms 3.96 m, but a `tiles` run is not replayable
  across time and was therefore never made the reference.
