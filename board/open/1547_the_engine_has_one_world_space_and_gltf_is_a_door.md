Type: feature
Area: gltf
Tags: scope

**The engine has ONE world space, and glTF is a door rather than a coordinate system**

**The owner's ruling, and it is the root cause of a whole afternoon of defects:**

> *What is glTF space anyway? Once a glTF is loaded it must be normalised and brought into the engine's
> context correctly. Please hold to how RAGE and Unreal do it.*

**They convert at IMPORT.** Unreal's glTF importer turns Y-up right-handed content into UE's Z-up
left-handed world once, at load; nothing in the runtime knows a glTF convention exists. RAGE
normalises in the asset pipeline for the same reason. **The conversion is a door, not a step in the
frame.**

## What the tree does instead

`EcefFromGltf` appears **twelve times in `src/clients/GltfStudio.cpp`, all on the runtime path**:

| Where | What it converts |
|---|---|
| `:233 :245 :252 :263` | every vertex, normal, tangent and previous position, per upload |
| `:290 :304` | every light's direction and position |
| `:317 :318 :319 :320` | the eye, its forward, its right and its up |
| `:34` | the per-part placements -- **added today, because they were the thirteenth thing to forget** |

**Every new thing that enters a picture must remember to convert, and the compiler cannot help.** That
is the defect this item exists to make unspellable.

## What it cost, measured

Four defects in one session, all the same class:

- the road drew **above the horizon** and first and third person were **near-identical**, because the
  placement was a glTF-space transform multiplied onto ECEF-space vertices
- the fix was the conjugate `P*M*P` -- correct, and a symptom: a correct conversion is still a
  conversion nobody should have to write
- `Ribbon` had to publish an origin so its `float` coordinates would not lose precision at km 250,
  a second frame convention layered on the first
- `kStudioAnchorEcefM` pins every picture to one point on the equator, which is a studio's assumption
  wearing a world's name

## What must be true

- [ ] **`Gltf::Subject` holds engine world space**, converted once where the document is read --
      `Build` and `Assemble` are the door
- [ ] **`EcefFromGltf` has no spelling on the frame path.** All twelve sites disappear; what is left
      is one function at the importer and one at the oracle boundary, where talking to Blender is a
      TOOL's business
- [ ] **A new thing entering a picture cannot get the convention wrong**, because there is only one
- [ ] **The picture bound does not move.** 15 khronos criteria and 21 grown within bound today, and a
      change of basis that is correct reproduces every one of them

## The design, and none of it is invented

**Both conventions, quoted from source rather than recalled.**

| | glTF 2.0, `specification/2.0/Specification.adoc` | Unreal, Epic's *Coordinate System and Spaces* |
|---|---|---|
| handedness | *"glTF uses a right-handed coordinate system"* (`:706`) | *"UE is referred to as a left-handed coordinate system ... and Z-up"* |
| up | *"glTF defines +Y as up"* (`:708`) | +Z |
| forward | front faces +Z; the camera *"looks towards the local -Z axis"* (`:708`, `:2254`) | +X |
| unit | *"The units for all linear distances are meters"* (`:713`) | Unreal Unit |
| the world | -- | *"World space ... is fixed - you cannot transform it"* |

**The lesson is the shape, not the numbers**: one fixed world space per engine, and the importer
converts into it. Unreal's glTF importer does the axis change once at load; nothing downstream knows
a glTF existed.

**This engine's world space is already chosen and already named** -- ECEF metres, camera-relative
`float32` at upload. `kStudioAnchorEcefM`, `PositionEcefM` and `SubjectMesh::Anchor` all say so. So
nothing here is a new choice: the map is the one the tree already applies, `(x, y, z) -> (y, x, -z)`,
and the only question is WHERE.

**Backwards from the frame, which is how the owner asked for it to be thought through:**

| Step | What it must already be |
|---|---|
| the draw | a vertex buffer already in world space -- the shader multiplies and does not convert |
| the upload | must copy, not transform. So `PackVertices` converts **nothing** |
| the subject | already world space, so `Aim`, the lights and the placements convert nothing either |
| the door | **`Subject.cpp:703`** -- the single place `Build` writes a world position, after the node hierarchy is applied -- and the matching writes for normals and tangents, and `Assemble` for generated parts |

**Twelve conversions become one.** What is left outside the door is the **oracle boundary**, where
talking to Blender is a tool's business and belongs in `test/harness/`, not on the frame path.

## Comments

**This is not a tidy-up.** It is the difference between an engine whose world has a coordinate system
and one where every subsystem carries a private opinion about which way is up. The four defects above
are not four mistakes; they are one shape, found four times.

**The risk is real and belongs in its own round.** `Gltf::Subject` is what the oracle comparison reads,
so the conversion must move to the Blender boundary in the same change, with `harness/render/khronos/glTF`
and `harness/render/outshine/grown` as the gate. Doing it while the corpus is pruned to 15 criteria
would be measuring a smaller population and calling it agreement.
