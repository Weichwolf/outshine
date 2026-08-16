Type: task
Parent: 0079
Area: gltf
Tags: khronos, oracle, instrument

**`COLOR_0`, and the six cells of which one asset covers one**

**Row 5.** Impact **7 of 146**, tier 1, and **dispatched on a measurement rather than on the rule**:
`board:1187` established that pipelines are built in `Configure`, called **only** from `Renderer::Init`,
so the layout doubling — **8 → 16 layouts, 48 → 96 pipelines** — costs **≈330 ms on a cold `Init` and
nothing this instrument can resolve in a frame**.

## The specification, quoted

> *if a primitive specifies a vertex color using the attribute semantic property `COLOR_0`, then this
> value acts as an **additional linear multiplier to base color***

and **all components MUST be clamped to `[0.0, 1.0]`**. Permitted: **`VEC3` or `VEC4`**, componentType
**float · unsigned byte normalized · unsigned short normalized** — **six cells.**

## `BoxVertexColors` at the pin, checked before the case is designed

| | |
|---|---|
| the accessor | `"componentType": 5126, "type": "VEC3", "count": 24` — **float, VEC3, not normalized** |
| the material | **there is none.** The document declares no materials at all |
| do the colours vary? | **yes — `min [0,0,0]`, `max [1,1,1]` over 24 vertices.** Constant colours would put min equal to max |

**That last line is a correction of the reading that came back with the file**, which inferred *constant
or uniform* from the same bounds. **Min ≠ max is variation, and it is the opposite conclusion** — the
third time this session a fetched summary has drawn a wrong inference from a file it had in front of it,
after the model index returned 148, then 228, then 244.

- [ ] **The asset covers ONE of six cells.** The two normalized integer forms and both `VEC4` forms are
  unproven by it — `board:1179` and `board:1186`'s shape again, and it is named here rather than
  discovered. **`VEC4`'s alpha multiplies base colour's alpha**, which interacts with `alphaMode`, so it
  is not a cosmetic fifth cell
- [ ] **The clamp is a specification requirement on the ASSET, not licence for the engine to skip it.**
  A file out of range is malformed; **whether we clamp, refuse or trust is a decision that must be
  written down**, because all three are defensible and only one can be tested
- [ ] **The multiplier is LINEAR and base colour arrives sRGB-decoded.** A `COLOR_0` applied before
  decode is a plausible picture and a wrong one — the silent-success shape `board:1182` was filed against

## The case tests two things at once, and that is unavoidable rather than sloppy

**`BoxVertexColors` declares no material**, so it also exercises **glTF's default material** —
`baseColorFactor [1,1,1,1]`, `metallicFactor 1.0`, `roughnessFactor 1.0`. **A residual on this case has
two candidate causes** and the case cannot separate them.

- [ ] **State it in the manifest rather than discovering it in the report.** If the default-material path
  is already exercised elsewhere, say where; if it is not, this case proves two rows and only one of them
  is on the board
- [ ] **The mutation that must fail it: ignore `COLOR_0` and render base colour alone.** With no material
  declared that renders a white box, so the mutation is **loud on this asset** — which is why it is the
  right first case despite covering one cell

**Done when** a primitive's `COLOR_0` multiplies base colour linearly after decode, the clamp decision is
recorded and tested, `BoxVertexColors` is in the tree and inside the picture bound, and the five uncovered
cells are named where the row's coverage is published.
