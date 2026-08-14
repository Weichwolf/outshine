Type: bug
Area: gltf
Tags: khronos, instrument

**The extension's published snippet has the wrong rotation sign, and the next data extension reads the same document**

`KHR_texture_transform`'s README prints
`mat3(cos r, sin r, 0, −sin r, cos r, 0, 0, 0, 1)`, which under GLSL's **column-major** constructor is
`[[cos, −sin], [sin, cos]]`. **That sign is wrong against the document's own worked example**, and this
engine deviates from it deliberately.

**Three independent witnesses, and they agree against the snippet:**

| witness | what it says |
|---|---|
| **the worked example three paragraphs below the snippet** | `offset [0,1]`, `rotation π/2`, `scale [0.5,0.5]`, declared to *"utilize only the lower left quadrant, rotated clockwise 90°"*. Under the snippet's sign the unit square leaves the image entirely; under the other it lands exactly on the lower-left quadrant |
| **the prose, read in the frame the extension declares** | rotation is counter-clockwise on the UVs with **(0,0) at the upper left**, which is the opposite screen direction from the naive reading |
| **the asset** | with the snippet's sign our arrow lands on `TextureTransformTest`'s **red** marker, which Khronos defines as *"the rotation was applied in the opposite direction"* |

**At the picture: snippet sign 255 codes over 25 715 px; corrected 9.593 over 19 437 px.** The corrected
value is this case's remaining residual and is attributed elsewhere (`board:1151`); the 255 is what the
snippet produces.

**Why this is filed rather than fixed-and-forgotten.** `CLAUDE.md`'s rule is that a deviation is a defect
**until its reason stands beside it** — and this is a deviation from the reference document the
implementation was written from. The reason must be **citable from the source that deviates**, so a later
round reading the published snippet and "correcting" our sign has the three witnesses in front of it.
**`board:1177` establishes the precedent for every data extension behind it, and they all read this same
document.**

- [ ] **The deviation is cited at the site that makes it**, by `board:` marker, with the worked example as
  the witness — not as a comment saying *the spec is wrong*, which is an assertion, but with the
  arithmetic that shows the unit square leaving the image
- [ ] **Raised upstream, or the decision not to is recorded here.** An error in a ratified extension's
  published snippet is worth one issue, and this repository's own standard is that a finding with three
  witnesses is reportable
- [ ] **The house rule this establishes**: where a specification's **prose** and its **sample code**
  disagree, the prose and the asset decide, and the disagreement is recorded. A snippet is an
  implementation; the normative text is the specification

**Done when** the deviation is cited from the implementing source with its three witnesses, and an
upstream report exists or its absence has a reason.
