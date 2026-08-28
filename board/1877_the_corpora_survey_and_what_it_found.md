Type: task
State: open
Area: test
Tags: corpora, invariants

# The corpora survey names three, and each names a gap it closes

**Benchmark** — Neither engine ships a corpora survey; both rely on internal QA and platform certification. **The choice is mine** — vendor corpora are the only oracles here whose truth does not depend on this design, so which capability each one asserts has to be written down or the coverage claim is a feeling.

`test/CORPORA.md` is the survey: for every capability TARGET demands, which ESTABLISHED corpus
asserts that problem class, and is it reachable. Two structural findings came out of it that the
brief did not ask for and that decide how the list is read:

**Effort has two halves.** FETCH is a pinned URL and a hash — cheap for almost everything worth
having. REACH is priced by board:1879: `test/` may only go through `include/`, and the door is
two headers. A corpus whose fetch is trivial and whose reach is not is not a cheap row.

**Four grades separate what a corpus HOLDS**: SPEC (a standards body states the answer), TRUTH (a
measurement or computation carried to more digits than we hold), SNAPSHOT (another
implementation, frozen — agreement, never correctness), INPUT (nothing is supplied; it proves
survival only). Most of what graphics calls a test corpus is INPUT.

## The three, in order

| | corpus | what it closes |
|---|---|---|
| 1 | glTF-Validator, 372 golden report pairs, Apache-2.0 | glTF is the only content surface. The 151 render cases prove what we do with a CORRECT asset and nothing proves what we do with a broken one — and refusal-with-a-reason is first class here (`std::expected`). Rides on `prepare.py` with no new job kind |
| 2 | GeographicLib GeodTest + TMcoords, CC0, DOI-pinned | *One world space*. `src/base/geo/Geodesy.h` and `Mercator.h` carry Munich–Hamburg, every tile key and every building placement, and NOTHING proves one function of it. 50-digit reference, so the corpus cannot be the thing that is wrong |
| 3 | Kider's measured sky via `ebruneton/clear-sky-models`, BSD | Three medium stages are green in CURRENT *and* TARGET with no oracle at all — Cycles models no atmosphere. See the box below |

JTS TestBuilder lost third place to a measurement, not a preference: 129 files of SPEC-grade
expected WKT, dual EPL/EDL — and `grep` over `src/ground` and `src/generators` finds no buffer,
no overlay, no union, no point-in-polygon. A corpus for operations the engine does not perform
proves nothing.

## The magic number the survey walked into — the review's verdict

```
src/render/stages/ParticipatingMedium.h:13-30
  float BottomRadiusKm = 6360.0f;
  float TopRadiusKm = 6460.0f;
  float RayleighScatteringPerKm[3] = {0.005802f, 0.013558f, 0.033100f};
  float MieScatteringPerKm = 0.003996f;
  float OzoneAbsorptionPerKm[3] = {0.000650f, 0.001881f, 0.000085f};
  float GroundAlbedo[3] = {0.10f, 0.13f, 0.07f};
```

The survey named one line; the block is fourteen numbers and **two separate defects, of which
the second is the worse one.**

**One: they are a SNAPSHOT wearing no label.** These are Bruneton's precomputed-atmospheric-
scattering constants to the digit — Rayleigh 5.802/13.558/33.100 e-6 m^-1, Mie 3.996e-6, ozone
0.650/1.881/0.085e-6. So an origin exists and the tree does not carry it. Under the survey's own
grades that makes the sky SNAPSHOT-grade: agreement with one implementation, never correctness.
Corpus 3 is what turns it into TRUTH, and that is the argument for its rank — not that the
numbers are wrong.

**Two: `BottomRadiusKm = 6360.0f` is EARTH'S RADIUS inside a render stage.** CLAUDE.md's layer
table forbids any of `src/` to spell a planet's name or numbers, and a scale height of 8 km, an
ozone layer centred at 25 km and a ground albedo of 0.10/0.13/0.07 are Earth's atmosphere and
Earth's ground, declared in a struct the renderer owns. This is board:1611's audit at a site that
item does not name, and it is filed there rather than here.

What the block gets RIGHT and should not lose in the repair: `struct alignas(16) Medium`, five
float4 rows, `static_assert(sizeof(Medium) == 80)` and `static_assert(alignof(Medium) == 16)`
with the padding named. That is the house style working.

## What will be true

- [ ] The three are fetched, prepared and run, each as a SCENARIO with its oracle.
- [ ] Every one of the fourteen numbers cites where it comes from, or is derived from a corpus
      that does — and the atmosphere is DECLARED per sphere rather than defaulted in a stage
      (board:1611).

## Comments

- 2026-08-25 — `test/CORPORA.md` landed at `bf41233e` citing this number before the item existed;
  the item is written to the survey rather than the other way round.

## The survey has fallen behind the tree it surveys (measured 2026-08-25, c0de1b18)

`test/CORPORA.md:38` states *Measured 2026-08-25 by `find test/render -name manifest.json`* and
`test/render/` does not exist -- board:1895 moved the corpora to `test/<vendor>/<suite>/` and the
survey's own method line still points at the old path.

Worse, the table *What runs today* names four corpora and 1160 cases, and the tree now runs
seven more suites it does not mention: `outshine/door` (4 cases), `outshine/
physics` (2), `outshine/fuzz` (2), all written this hour. CLAUDE.md: *a case whose grade
the survey does not name is a case nobody has priced.* Their grades are not obvious and that is
exactly why they must be written down -- `ScoreTheWrenchAForceBuilds` is TRUTH (statics), while
`ScoreWhatASubjectSwapRebuilds` asserts a COST BOUND against our own counters and is neither
SPEC nor TRUTH nor SNAPSHOT. If the four grades do not have a place for a cost bound, the survey
gains a fifth and says what it proves.

- [ ] Every suite `test/run.sh` runs has a row in the survey with its grade, including ours.
- [ ] The survey's method line names a path that exists, and a claim walks it.

## The sky's sixteen constants, given their origin

CORPORA.md's own argument for the Bruneton corpus says citing the reference implementation
discharges half the debt before a byte is fetched. `src/render/stages/ParticipatingMedium.h`
carries sixteen floats on the frame path and `grep board/` found none of them. Here they are, and
where each comes from.

**Source**: E. Bruneton, *Precomputed Atmospheric Scattering* (2008), and the 2017 reference
implementation `ebruneton/precomputed_atmospheric_scattering`, whose demo states these values in
per-METRE units at the RGB wavelengths lambda = 680 / 550 / 440 nm. This tree carries them per
KILOMETRE, so every scattering figure below is the reference value x 1e6 / 1e3, i.e. x 1e3.

| the field | the value | origin |
|---|---|---|
| `BottomRadiusKm` | 6360 | reference — the Earth radius the model uses, not WGS84's |
| `TopRadiusKm` | 6460 | reference — 100 km of atmosphere above it |
| `RayleighScaleHeightKm` | 8.0 | reference — the exponential density scale |
| `MieScaleHeightKm` | 1.2 | reference |
| `RayleighScatteringPerKm` | 0.005802, 0.013558, 0.033100 | reference `5.802e-6, 13.558e-6, 33.1e-6` per m at 680/550/440 nm |
| `MieScatteringPerKm` | 0.003996 | reference `3.996e-6` per m |
| `MieExtinctionPerKm` | 0.004440 | reference `4.440e-6` per m — extinction exceeds scattering, the difference is absorption |
| `OzoneAbsorptionPerKm` | 0.000650, 0.001881, 0.000085 | reference `0.650e-6, 1.881e-6, 0.085e-6` per m |
| `OzoneCentreKm` / `OzoneHalfWidthKm` | 25 / 15 | reference — the tent profile peaks at 25 km |
| `MiePhaseG` | 0.8 | reference — Cornette-Shanks asymmetry |
| `GroundAlbedo` | 0.10, 0.13, 0.07 | **NOT the reference.** Bruneton's demo uses a flat 0.1. These three are this tree's own and carry no derivation — the one number here that the corpus must adjudicate rather than confirm |

**So fifteen of sixteen are cited and one is exposed.** That is the honest split, and it is why the
corpus is worth fetching rather than optional: `GroundAlbedo` is a free parameter that changes
every sky the engine draws, and Kider's measured full-day irradiance is what can say whether it is
right.

- [ ] the sixteen carry their origin where CLAUDE.md puts it -- the item and the commit, since
      `src/` carries no comments. **Done by the table above**; what remains is the guard: a claim
      that refuses a NEW undeclared constant in `Medium` rather than trusting the next reader.
