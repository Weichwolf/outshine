Type: bug
Area: render
Tags: small-notes

# The shadow basis refuses degeneracy, and the stage constants carry their origin

Four small notes from the render/plan + render/stages pass, bundled like round 22's:

1. `src/render/stages/LightVisibilityStage.cpp:27-41` — `Build` normalises `forward` and
   `right` with no guard: a zero `toSun` or an `up` parallel to the sun divides by zero
   and the whole `LightFromWorld_` matrix goes NaN — a silently black (or fully lit)
   shadow term, the opposite of "a failure is loud". `Declare` (line 14-21) checks only
   `radiusM > 0`; it must also refuse a degenerate basis, with the refusal text naming
   the two vectors.

2. `src/render/plan/RenderCatalogue.h:295` — `kTemporalSettleFrames = 128` carries no
   origin: derived from which blend factor over which convergence criterion, or `[SET]`?
   Same for `kStagedCrossings = 32` (src/render/stages/SubjectResidency.h:61) — the
   actual population is 10 per residency (8 streams + 2 BVH runs); name the derivation.

3. `src/render/stages/TonemapStage.cpp:1-2` — `<cstdio>` stands BEFORE the unit's own
   header, the exact include-order C-ism commit daa3485b just fixed in SourceSet.cpp;
   and `<cstdio>` is not used in this file at all. Own header first, dead include gone.

4. `src/render/stages/MediumTransmittanceStage.cpp:65` (and MediumRadianceStage.cpp:88)
   — `memcmp` over `Medium`/`Standing` structs compares padding bytes; a padding
   difference is only a spurious re-dispatch, but a `static_assert` that the compared
   types have no padding (sizeof == sum of members) beside the memcmp makes the
   settled-check a fact instead of a hope.

---

Closed -- (1) the shadow basis refuses degeneracy at Declare: a zero sun, an up parallel to
it, or an empty radius leave the stage not Standing, proven in
unit/render/stages/AShadowBasisRefusesDegeneracy (negative control red; the stages suite
gained its build set); (2) kTemporalSettleFrames carries its derivation (0.9^128 ~ 1.4e-6,
under one 16-bit code) and kStagedCrossings its population (8 streams + 2 BVH runs, with
headroom); (3) TonemapStage's own header stands first and the dead <cstdio> left; (4) the
settled memcmps are facts: Medium's 80-byte assert already stood, Standing gained explicit
tail pads with a sizeof assert so no compiler-owned byte enters the comparison.
