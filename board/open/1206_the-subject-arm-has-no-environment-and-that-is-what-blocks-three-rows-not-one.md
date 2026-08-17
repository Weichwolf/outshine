Type: feature
Area: render
Tags: khronos, oracle, instrument, scope

**The subject arm has no environment, and that is what blocks three rows rather than one**

Three rows of `board:0079`'s sequence have now been stopped by the same missing quantity, and each was
diagnosed on its own before the pattern was visible. **It is one piece of work, and its impact is the
sum of theirs.**

| row | impact | why it stops |
|---|---|---|
| occlusion texture | **46** | glTF states it attenuates INDIRECT light and *direct lighting is not affected*. There is no indirect term to attenuate |
| `KHR_materials_ior` | **17** | `IORTestGrid`'s panels are black, metallic 0, **roughness 0** — a mirror whose only visible content is what it reflects |
| `KHR_materials_specular` | **9** | `SpecularTest` is the same construction: 23 black roughness-0 panels varying one factor |

**72 models' worth of blocked impact behind one term**, which is more than any single extension left in
the sequence and more than the two largest of tier 2 together.

## What is missing, measured

**`shadeRow` sums punctual lights and adds emission, and returns.** There is no ambient, no environment,
no irradiance — [MEASURED] in the catalogue, `Stage::Subjects` reads **`{kNoEdge}`**, while `Terrain`,
`Buildings`, `Water` and `Models` each read `IrradianceBuffer`. **The term exists in the plan and the
subject path is the one geometry unit that does not consume it.**

**And the suite cannot declare one either.** The manifest schema's `light` discriminator has exactly
three variants — `none`, `gltf`, `sun` — which is the whole enumeration and not a sample. Across the
corpus: **24 cases declare `none`, 6 `sun`, 2 `gltf`.** There is no spelling for *light this subject the
way its author intended*, and for these three assets that is image-based lighting.

## Why a sun does not stand in, and this is the part that would waste a round

**Under a punctual light a roughness-0 lobe is very nearly a delta**: the highlight is a point, and
`F0` — the number `board:1205` just delivered — decides the brightness of something too small to
compare. **Meanwhile Cycles renders the factory world**, which is a uniform grey environment and IS a
light, so the oracle would show each panel reflecting that grey at exactly the F0 under test while our
side showed black.

**A case authored today would therefore be red for a reason that is neither our arithmetic nor
Blender's**, and the natural reading of it — *our specular is wrong* — is the one that costs the round.
*This is `board:1204`'s silent-failure shape arriving from the other direction: there the oracle dropped
what the file said, here it renders something the engine has no term for.*

## What must become true

- [ ] ~~**The subject arm consumes an irradiance term**, the way the other four geometry units already
  do.~~ **WRONG, AND CORRECTED BEFORE IT COST A ROUND.** [MEASURED] in the catalogue: `Stage::Irradiance`
  reads `SkyViewLut`, `TransmittanceLut`, `LutSampler` and `AtmosphereUniform` — **`IrradianceBuffer` is
  the SKY's irradiance and it pulls the whole atmosphere chain behind it**. Giving `Stage::Subjects` that
  edge would make every corpus case's plan include Transmittance, MultiScatter, SkyView and Irradiance,
  and its picture a function of **our atmosphere model**, which Cycles is not running. The corpus would
  then be comparing our sky against Blender's world — a different disagreement wearing this one's name.
  *The resource's name suggested a general environment and the diagram was read instead of the row.*

- [ ] **What is wanted is a DECLARED UNIFORM ENVIRONMENT**, which is what the oracle already has: Blender's
  world is a Background colour times a strength, and that is a constant radiance from every direction.
  It costs no chain, it is a function of the declaration rather than of a model, and it is exactly the
  quantity `SpecularTest`'s mirrors reflect and glTF's occlusion map attenuates. **The atmosphere's
  irradiance stays what it is** — a world scene's sky — and the two do not become one field
- [x] **A case can already declare an environment, and the oracle already renders it.** The schema's
  `world` discriminator has a `uniform` variant carrying `colourLinear` and `strength`, and the
  preparer drives Blender's Background node from it. **Both halves of the declaration existed before
  this item was filed** — what was missing was only that this engine ignored them.
- [x] **The engine now gathers it.** `SubjectEnvironment` is a declared constant radiance in the light
  uniform — *it IS a light, the one whose solid angle is the whole sphere*, so it needs no binding of
  its own — and `shadeRow` adds `environment · (diffuse + specular)`. **Zero by default**, which is
  what every corpus case that gathers already declares (`world: uniform` at strength 0), so this
  arriving moved no picture: 55 of 55 in the unit tree and `Triangle` green on three arms

- [x] **THE LIT ARM EXISTED AND NOW GATHERS THE ENVIRONMENT.** The previous line said this arm had to be
  built; it did not — `ShadedByLights()` keys on `scene.material.source: gltf`, **not** on the light, so
  the arm where the surface's own row is evaluated has been there all along. What was missing was one
  call. `Studio::Environment` carries the declared radiance and `Show` hands it to the unit.

  **IT IS SUPPLIED IN THAT ARM ALONE, AND THE REASON IS ARITHMETIC.** Every other arm PRE-MULTIPLIES the
  world into a per-part radiance and draws it unlit — `rho·L`, the exact closed form for a Lambert
  surface — so passing the environment there as well would count it twice.

  **Verified over the exact population rather than argued**: all 8 cases declaring `source: gltf` also
  declare `world: uniform`, and every `uniform` in the tree is strength 0 — so the radiance passed is
  zero for the whole existing corpus. **211 tests, 147 PASS, 63 FAIL, and the failing set diffs to
  nothing against the previous run, id by id.**

- [ ] **What remains is a case that declares a NON-ZERO environment**, which is now a manifest and not an
  engine change: `SpecularTest`, `light: none`, `material.source: gltf` with `kind: metal-rough`, and a
  `world: uniform` whose strength is not zero. *Every mechanism it needs is in the tree and measured.*

- [ ] ~~**A LIT-UNDER-ENVIRONMENT MATERIAL ARM DOES NOT EXIST.**~~ Superseded by the line above. [MEASURED] over the runner's whole material vocabulary: `emission`,
  `emission-per-material`, `diffuse`, and the file's own colours. **Every one of them computes a
  RADIANCE in the runner and hands it to an unlit draw.** `diffuse` is literally the closed form
  `rho·L` and refuses a multi-part subject *because the closed form holds only where no surface can
  see another*. So the engine's new environment term reaches no picture through any arm that exists:
  the lit shader runs only under a punctual light, and every case that has one declares the
  environment away at strength 0. **`SpecularTest` needs an arm this runner has never had** — the
  file's metal-rough material, shaded by the ENGINE, under a declared environment

- [x] **`bounces.max` IS 0 IN ALL 56 DECLARED RECIPES AND IT IS NOT A GATE.** [MEASURED] with one
  render of one panel — a black, roughness-0 dielectric under a uniform world of 0.25, at Cycles'
  factory Principled defaults:

  ```
  bounces = 0   centre pixel = 0.009973
  bounces = 4   centre pixel = 0.009973      identical
  ```

  **The oracle renders the world-lit specular reflection at zero bounces**, and raising the budget
  changes nothing for this configuration. *An environment reaching a surface is not an indirect
  bounce, and the measurement says so rather than the argument.*

- [x] **AND THE SAME RENDER VALIDATES THE FORM THIS ENGINE CHOSE, INDEPENDENTLY.** `F0 · L` for a
  dielectric at normal incidence is `0.04 × 0.25 = 0.01` exactly; Cycles returns **0.009973**, apart
  by **0.3 %** at 16 samples. So the specular environment term is `F(nv)·L` on the oracle's own
  evidence — **and Karis's two-term fit, which would have returned 0.0457 × 0.25 / 0.04 ≈ 0.0114 here,
  is 14 % away from a number Cycles has now stated.** *The rejection was made on the arithmetic before
  this render existed; the render is what turns it from a judgement into a measurement.*

- [ ] **The runner's diffuse BAKE is then a separate question with its own blast radius.** `Parity.cpp` carries `kFactoryWorldRadiance = 0.05087608844041824` and multiplies it into
  each part's colour, handing the renderer an EMISSION — a Lambert surface under uniform radiance
  leaves `rho·L`, so the bake is exact for diffuse and silently absent for specular, which is the half
  `SpecularTest` measures. **Replacing the bake with `SetEnvironment` changes how 24 cases are drawn**,
  so it is its own dispatch with its own before-and-after, and not a line added to this one
- [ ] **The occlusion row is re-examined against it.** Its third gate (`board:0079`) is Blender's, and it
  does not open — but the FIRST two are ours, and this is the work that opens them. Whether the row then
  becomes decidable is a question this feature makes askable rather than one it answers

## Comments

**The correction above is the item's own lesson arriving early.** This entry was filed after three rows
stopped on one sentence, and its first line then named a resource by what its name suggested rather than
by what its row says. **One `grep` of the catalogue was the difference between a design and a round spent
discovering that every case's picture had moved.** *`CLAUDE.md`'s rule is that a number carries its
origin; a RESOURCE carries one too, and `IrradianceBuffer`'s is the sky.*

**The engine half cost less than the diagnosis did, and that is the usual shape here.** Three rounds
went into finding that one term blocks three rows; the term itself is a `float4` in a uniform and two
lines in a shader. *What was expensive was knowing which quantity, and the round that nearly spent
itself on the wrong one — `IrradianceBuffer` — was stopped by a `grep` of the catalogue.*

**The specular form is exact where the corpus measures and over-estimates elsewhere, stated rather than
discovered.** Under a constant environment a mirror returns `F(nv)·L`, and `SpecularTest` and
`IORTestGrid` are roughness-0 panels, so Schlick's Fresnel at the view angle is the exact answer there.
**Karis's two-term environment-BRDF fit was tried and rejected with its number**: at `roughness = 0`,
`nv = 1` it gives `F0·0.9941 + 0.00588`, which for a dielectric's 0.04 is **0.0457 — 14 % high, on
exactly the panels the term exists to make decidable**, and it would have read as this engine's `F0`
being wrong. What is owed is the other end: a rough surface's directional albedo is below `F(nv)`
because a GGX lobe loses energy to masking, so a rough dielectric under an environment is drawn too
bright. No case measures that today; the first one that does pays for the split-sum.

**The order this was found in is the lesson.** Occlusion looked like a row about a texture; specular
looked like a row about a factor; ior looked like a row about a number. Each was measured on its own and
each stopped on the same sentence — *there is nothing for it to modulate*. **Three rounds of diagnosis
produced one item**, and the sequence in `board:0079` orders by *models blocked solely by that feature*,
which by construction cannot see a term that blocks three features at once.
