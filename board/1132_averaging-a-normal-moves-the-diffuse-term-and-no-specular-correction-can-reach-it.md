Type: bug
State: open
Area: render
Tags: oracle, khronos, instrument

**Averaging a normal moves the diffuse term, and no specular correction can reach it**

Toksvig's factor is built, derived and tested (`board:1130`), and enabling the mip chain with it in place
still fails by a wide margin. **It closes 3.2 % of the distance to the oracle.**

| `row1_pair2_geometry_matches_normalmap_p95_relative` | |
|---|---|
| unfiltered | 0.13879225 |
| filtered, no Toksvig | 0.27571386 |
| **filtered, with Toksvig** | **0.26971** |
| the oracle, same cell | **0.08692** |

**The reason is that Toksvig is a specular correction and the dominant error is not specular.** A
filtered normal is a different DIRECTION, so `N·L` changes and the Lambertian term changes with it —
first order, present at every roughness, and untouched by anything that widens a lobe. Widening the
lobe is the right repair for the specular half and it is worth its 3.2 %; it was never going to be the
whole of it.

**The evidence that this is direction and not lobe:** `pair1` — the cells where our unfiltered agreement
was BEST, `0.03156..0.05676`, better than the oracle's own `0.05677..0.07886` — goes to `0.18338..0.25048`
under filtering. A cell with little specular disagreement to begin with cannot be ruined by a specular
correction being too small.

| cells, filtered with Toksvig | ours | the oracle |
|---|---|---|
| `normal-tangent` pair1 × 5 | 0.18338 … 0.25048 | 0.05677 … 0.07886 |
| `normal-tangent` pair2 × 5 | 0.21178 … 0.26971 | 0.05547 … 0.08692 |
| `normal-tangent-mirror` left × 5 | 0.17024 … 0.21937 | 0.04859 … 0.07369 |

`picture_max_delta_code` stays at **255** on both cases.

**And one cell class inverted, which is a finding of its own.** The third pair of every row is a metal
whose base colour is black; both sides were exactly 0 and the manifest declares the check vacuous. Under
Toksvig they read **1.00000** — the relative form is `|a-b| / max(|a|,|b|)`, so any infinitesimal on one
side alone reads as total disagreement. **A vacuous check is not a harmless one**: it holds 0 until
something perturbs it and then reports the maximum, which is indistinguishable in a log from the worst
real failure this suite can produce.

**What would actually reach it** — named, not chosen, and each needs its own measurement:

| | |
|---|---|
| **shade-then-average** | what Cycles does: many rays per pixel, each against a normal at its own differential, averaging RADIANCE. Correct by construction and the reason the oracle does not suffer this at all |
| **a narrower footprint** | our LOD comes from screen-space quad derivatives, which spike 712× at UV island boundaries; ray differentials do not. A correct filter over a wrong footprint is still wrong |
| **an anisotropic filter** | a box over an isotropic footprint over-blurs whenever the true footprint is elongated, which on a tilted quad is most of it |

## Comments

**2026-08-14** — Filed from `board:1130`'s Toksvig round. The term is kept and shipped, because it is
derived, correct within its domain and provably inert at `l = 1`; what is NOT shipped is `max_lod`,
which still fails the acceptance stated before the round — ours in `0.04859..0.08692` and the picture
bound at 20 of 34 or better. Ours is 2 to 3× outside that band.
