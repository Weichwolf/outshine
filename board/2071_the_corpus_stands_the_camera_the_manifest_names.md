Type: defect
State: active
Area: test
Tags: corpora, translator, measured

# The corpus stands the camera the manifest NAMES, and refuses a source it cannot honour

**Benchmark** — Unreal: `FAutomationScreenshotOptions` takes the camera from the level's own
`CameraActor` and an automation shot that silently substituted a default view would be a broken
test rather than a failing one. RAGE: a `rag` capture replays the recorded camera. **They agree**:
the harness reproduces the oracle's framing or it refuses, and it never picks its own.

## What was measured, 2026-08-31

`test/scripts/render_corpus.py` reads `scene.camera` from the manifest. Across 185 manifests:

| `camera.source` | manifests | the translator |
|---|---|---|
| `manifest` | 147 | reads `positionM` / `lookAtM` / `yfovRad` from the manifest |
| `derived` | 34 | reads `provenance.report.render[].provenance.camera.derivedFrom` |
| **`gltf`** | **4** | **reads nothing, and falls back to `[0,0,3]`, `yfov` 0.5 rad** |

Both of the four that are scored are RED, and this is why:

    APART MultiUVTest         0.0000%  worst pixel 255  921600 px
    APART DirectionalLight   85.7584%  worst pixel 112  131251 px

**`MultiUVTest` at 0.0000 % is every pixel of the frame.** Looked at, because a number is not a
picture: the reference is a black field with three glTF logos on three faces of a cube seen from a
corner; ours is a FLAT PALE YELLOW field with no subject in it at all.

The asset carries its camera in the file -- node `Camera` at `[7.481, 5.344, 6.508]` with a
rotation, a child `Correction_Camera` holding camera 0 at `yfov` 0.5034 -- and the manifest says so
in words: *"The file carries a camera node, so both sides read it from there: our reader through
`Gltf::DeclaredPlacement` and Blender's importer through the node it imported. It is the asset's
own framing, which puts three of the cube's faces in view and therefore three independent readings
of the same material in one picture."*

The translator writes `<view>` at `[0,0,3]` instead, so the case that exists to see three faces at
once sees none.

## This is the SECOND instance of one fault

`board:2053`'s withdrawal found the first: 34 manifests state `camera.source: "derived"` and the
translator never read `derivedFrom`, falling back to the same `[0,0,3]`. That was repaired and the
34 animation cases now hold -- measured today, 32 HELD and 3 APART. **The repair fixed the value it
was chasing and left the fall-through that produced it.** A fourth `source` tomorrow would do this
again silently, which is the actual defect.

## What will be true

- [x] `camera.source: "gltf"` stands the camera the FILE carries -- `gltf_camera()` composes the
      holder node's world transform down its parent chain, which this asset needs because its
      camera is a CHILD of a rotated parent, and reads `yfov` from the perspective block
- [x] a `camera.source` the translator does not know is REFUSED with its name, never defaulted
- [x] `MultiUVTest` shows the three faces the manifest describes -- LOOKED AT: the cube stands on a
      black field seen from a corner, three faces in view, at `[7.481, 5.344, 6.508]` and 28.84
      degrees, which is the file's own camera
- [ ] Negative control: a manifest whose `source` is misspelt goes RED naming the spelling, and the
      same manifest spelt right goes green -- the refusal is written and NOT yet exercised

## Measured after

    MultiUVTest        0.0000% -> 88.4148%
    DirectionalLight  85.7584% -> 86.9599%

The whole corpus is 123 held, 53 apart both before and after, and a line-by-line diff of the two
runs moves exactly those two rows and nothing else. Neither case HOLDS yet, which this item said it
would not cover -- and looking at `MultiUVTest` says why, below.

## WHAT THE RIGHT CAMERA THEN SHOWED, and it is a second field the translator does not read

The picture is the cube at the manifest's framing wearing FLAT COLOUR FACES and no glTF logo. The
colours are `uv0.png`, the base-colour image. The oracle is black with three green logos, which is
`uv1.png` through the EMISSIVE socket -- and the manifest states exactly that:
`material.source: "gltf-emissive"`.

`wears()` branches on `material.kind` and never reads `material.source`. `kind: "emission"` covers
two different oracles:

| `material.source` | the oracle's emitter is fed by | the row we write |
|---|---|---|
| `gltf-base-colour` | the base-colour image | `r=1 g=1 b=1` -- right |
| **`gltf-emissive`** | the **emissive** image | `r=1 g=1 b=1` -- the wrong texture |

Nine manifests say `gltf-base-colour` and three say `gltf-emissive`. **All three of the latter are
APART**, and they are the three whose oracle reads the socket we do not hand over:

    EmissiveStrengthTest            0.4444%
    MultiUVTest                    88.4148%   (after the camera)
    TextureLinearInterpolationTest 89.0897%

That is the same shape as the camera: a field the manifest STATES, the translator does not read, and
a default that is silently plausible. It is the third instance in this one file and it is the next
box.

- [ ] `material.source: "gltf-emissive"` hands the emissive image rather than the base colour, and
      the three cases are scored on the socket their oracle actually reads

## What this does NOT cover

Whether `MultiUVTest` then HOLDS. Its whole point is that reading `TEXCOORD_0` twice is a silent
success, so a right camera is what makes the case able to judge -- and it may then judge us wrong.
That is the case doing its job and it is a separate finding if it happens.
