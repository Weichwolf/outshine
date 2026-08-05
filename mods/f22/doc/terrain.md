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
   at a computed coordinate whose real elevation is unknown and may be a hilltop, a river or a
   village.
2. **Water features do not survive the anchoring at all** — §6. This is the one place where the
   assignment is not merely unproven but measurably false.
3. **Country assignment per target point is not verified.** The source research places 1.2, 1.3 and 1.4
   in northern Laos, which is consistent with the fiction ("moving into Laos", the base "in Laotian
   territory"). **No border dataset was consulted**, here or in the source. Treat every "this is in
   Laos / this is in Thailand" statement about a target point as TODO.
4. **Elevation range unsourced.** The source states "forested uplands 300–2,000 m". No DEM check was
   performed in either run. The single verified elevation datum inside the box is VTCN at 209 m.

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

## State

**All eight target points of §3 are flown**, in `../src/missions/`. §2.4's arithmetic was re-derived a
third time when the files were written and reproduces to ±0.00001° — e.g. §3's Chiang Rai row
(19.909 N / 99.828 E) comes out as 19.90868 / 99.82791 from the offsets alone.

**The tileserver has still never been asked for these coordinates.** Every sortie runs under
`--elev const` on a **flat 0 m plane**: no `.fbm` declares a `runway`, `../src/data/` carries no baked
DEM, so `fb-gym`'s elevation default is `const` and `FBRunwayPlateauElevation` with no runway answers
0 m everywhere. That is disclosure **D3** of `../src/missions/c01m01-snake-eyes.fbm` and it is the
single largest thing separating these files from the real region.

Three decisions §5–§6 left open are TAKEN in the files, each in the header of the sortie that takes it:

| Left open by | Taken as | Where |
|---|---|---|
| §6, the bridge and the Mekong | keep the measured point, put a fictional bridge on it | `c01m03-luckiest-man-in-laos.fbm` |
| §6, the boats and the Mekong | keep the measured point; there is no ship module, so a stationary `target_soft` | `c01m05-double-down.fbm` |
| §5.3, re-anchoring on VTCT instead of the Chiang Rai centroid | **not** taken; the target stays on the centroid, 7.5 km from the real field | `c01m07-aces-low.fbm` |

## Gaps

- **The box was never fetched from `fb-tiles` and every altitude in the eight `.fbm` files is ASL over
  a plane that does not exist.** The real ground is 300–2 000 m by §5.4.4, from a description; the one
  verified datum inside the box is VTCN at 209 m. Running the campaign with `--elev tiles` is the first
  thing that would change every number in it — and would, on this terrain, put several run-ins
  underground exactly as `mods/f16`'s W2 records for its own 30 m variant.
- **No terrain masking is in play anywhere**, so every detection range and every SAM engagement in the
  eight sorties is an UPPER bound.
- **The north assumption is unproven** and everything in §3 hangs from it. Support is eight bearings
  landing in the right compass octant (mean +9.2°), which is consistent with a rotation of up to about
  ±20° — the octant test simply cannot resolve better. If the map is rotated, every target point swings
  around FOB Tyler by that angle, up to 50 km at mission 1.8's range.
- **The Mekong does not pass through 1.3's or 1.5's target point** — §6, 105 km and 205 km off. Open,
  deliberately unpatched, and the decision is not made.
- **The `.ORF` parse is second-hand.** Re-derivation here proves consistency, not correctness of the
  byte-level read.
- **5 m vs 4.989 m unresolved** (§2.3). 0.27 km at the anchor distance; the coordinates use 4.989 m
  while the source's own conclusion is 5 m.
- **Latitude conversion uses 111.32 km/°, which is the *longitude* constant at the equator.** The
  meridional degree at 19.4 N is 110.70 km `[DERIV]`, so every northward offset is understated by
  0.56 % — `[DERIV]` **0.87 km at mission 1.8's 153.5 km**. Below the 18.5 km box margin, above the
  precision the reconstruction otherwise claims. Not corrected here, because correcting it alone while
  the north assumption stands would be false precision.
- **The voxel heightfield was never decoded.** Not in `RESOURCE.RES`; `.PCX` and `.PAK` were not
  decoded. Without it there is no way to compare game terrain against real DEM at all — the §5.2
  character match is a match of *descriptions*, not of surfaces.
- **`.REF` files uninspected** (49 × 24,576 bytes) — suspected terrain-tile or waypoint data. If they
  hold the heightfield, the previous gap closes.
- **No elevation was checked at any target point.** An objective may land on a 1,500 m ridge.
- **Country borders never consulted**, so §5.4.3 stands open.
- **Whether `fb-tiles` even covers this box adequately is unmeasured** — DEM, OSM and imagery over
  northern Laos at this scale. Unanswered because nothing has fetched it (bullet 1).
