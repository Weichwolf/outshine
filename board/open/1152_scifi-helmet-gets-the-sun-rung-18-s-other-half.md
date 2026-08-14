Type: task
Parent: 0078
Area: corpus
Tags: oracle, khronos, instrument

**`scifi-helmet` gets the sun, which is rung 18's other half**

`board:1146`'s recommendation is taken: `materials/scifi-helmet` declares a delta sun and a metal-rough
oracle instead of `light: none` and an emitter.

**Parented to `board:0078` because rung 18 is this subject and already declares the criterion**:
*one real asset's material stack — coverage · direct radiance*. The case delivers the coverage half today
and has never delivered the second, so this is a declared rung being completed rather than scope being
added. That is also why it is a `task` and not a `feature`: the requirement exists and is written down.

**The declaration, copied from `materials/water-bottle` rather than invented** — same host, same recipe
family, same subject class:

| | |
|---|---|
| `light.kind` | `sun`, `estimator: delta`, `angleRad = 0` — a sun with an angle is an area source and an estimator in the oracle |
| `irradianceWPerM2` | **π**, derived: a Lambertian white facet facing the beam returns `rho*E/pi = 1`, so the picture is scene-referred around unity and no exposure decides legibility |
| beam | off the view axis, so the terminator is in frame and the lobe is not seen head-on |
| `material.kind` | `metal-rough` — the file's own row entire: base colour, metallic-roughness, normal, occlusion |

**WHY A DELTA LIGHT IS THE WHOLE POINT and not a detail.** `water-bottle`'s own note states it: *a uniform
environment delivers the same radiance from every direction, so roughness, metalness and the normal map
all stop changing the picture.* Under `light: none` this asset's four images cannot decide anything, which
is why lowering its oracle to an emitter was correct **given the scene** — the reduction was about the
light and was recorded as though it were about the material.

**What it costs**: one manifest, one case's oracle products re-keyed and re-rendered — the recipe changes,
so the key must miss, and that is `board:1120` working. Renders on this host are ~2 s at one sample.

**What must be true after, or the edit has bought a number instead of a case.**

- [ ] **The shading population is 9, not 8**, and it is **read from the run** — the case's
  `outshine.normal.raw` carries a non-zero shaded count — never from a list. `board:1146` is *done when*
  both published counts carry that population, and this task is what moves it
- [ ] **`occlusionTexture` is stated as unread rather than silently unused.** It is the one thing rung 18
  adds over rung 11, this engine consumes no occlusion map, and a case whose declared novelty reaches
  nothing is a rung that looks covered
- [ ] **The verdict is stated with its cause**, whichever way it goes. A closed self-seeing body under a
  delta light at one sample has the visibility difference `water-bottle` already declares — Cycles traces
  a shadow ray per shading event and neither of our arms computes visibility — so **a residual is
  expected, is not a tolerance, and is judged by eye** under that precedent. What would overturn the
  decision is a measurement showing that difference makes the case undecidable rather than merely
  imperfect

**Done when** the case declares the sun, its oracle is metal-rough, the shading population reads 9 from
the run, and the residual is attributed rather than absorbed.
