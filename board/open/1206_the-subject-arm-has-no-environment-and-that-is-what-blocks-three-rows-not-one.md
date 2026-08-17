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

- [ ] **The subject arm consumes an irradiance term**, the way the other four geometry units already do.
  The plan already produces it; what is missing is the edge and the shading that reads it
- [ ] **A case can declare an environment**, as a fourth `light` variant or as a `world` that is a light
  rather than a backdrop — and the declaration says what the environment IS, so the picture stays a
  function of the declaration and not of Blender's default grey
- [ ] **The oracle's side is the same environment and is stated**, not the factory world by accident.
  Today `world: factory` is declared *configured by not being configured*, which is honest for a scene
  where nothing gathers and becomes a hidden light the moment something does
- [ ] **The occlusion row is re-examined against it.** Its third gate (`board:0079`) is Blender's, and it
  does not open — but the FIRST two are ours, and this is the work that opens them. Whether the row then
  becomes decidable is a question this feature makes askable rather than one it answers

## Comments

**The order this was found in is the lesson.** Occlusion looked like a row about a texture; specular
looked like a row about a factor; ior looked like a row about a number. Each was measured on its own and
each stopped on the same sentence — *there is nothing for it to modulate*. **Three rounds of diagnosis
produced one item**, and the sequence in `board:0079` orders by *models blocked solely by that feature*,
which by construction cannot see a term that blocks three features at once.
