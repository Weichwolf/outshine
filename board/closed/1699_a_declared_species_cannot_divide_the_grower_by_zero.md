Type: bug
Area: generators
Tags: assembly, refusal

**A declared species cannot divide the grower by zero**

`TreeSpecies::Parse` (`src/generators/TreeSpecies.cpp:28-140`) validates three fields — name,
form, crown — and accepts every number unchecked. The grower then divides by them:

- `src/generators/draw/TreeGrower.cpp:283`: `(s - bareSteps) % g.WhorlSpacing` — a species
  file declaring `"whorl_count": 5, "whorl_spacing": 0` is an integer division by zero
  (SIGFPE) at first growth. `WhorlSpacing` defaults to 4 (`TreeSpecies.h:36`) but the reader
  takes 0 and negatives without a word.
- `TreeGrower.cpp:101`: `g.BaseRadius / std::sqrt((float)n)` is guarded for n, but
  `step_len <= 0` collapses every step onto one point and `trunk_steps < 1` grows a stump
  regardless of form — both silently, where the house rule is refusal at assembly.
- `taper >= 1` never reaches `MinRadius`, terminating only on kMaxNodes — a declared typo
  costs the full node budget per tree, silently.

Parse IS the assembly door for species; it already refuses unknown `form`/`crown` strings
loudly. Demanded: numeric domain checks in Parse with the same refusal voice
(`whorl_spacing >= 1` when `whorl_count > 0`, `trunk_steps >= 1`, `step_len > 0`,
`0 < taper < 1`, radii > 0), and a unit case per refusal in a `test/unit/generators/`
twin — the shipped 32 species in `src/assets/world/species/` must all still pass.

---

Closed: TreeSpecies::Parse refuses at assembly what would fault at growth -- whorl_spacing 0
under a whorl_count names both numbers ("divides by it"), heightless trees and orders past
the grower's bound refuse alike. Proving test:
unit/generators/ASpeciesRefusesWhatWouldDivideByZero.cpp.
