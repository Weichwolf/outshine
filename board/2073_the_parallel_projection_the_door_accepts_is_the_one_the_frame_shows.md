Type: defect
State: open
Area: render
Tags: camera, corpora, measured

# The PARALLEL PROJECTION the door accepts is the one the frame shows

**Benchmark** — Unreal: an orthographic `CameraActor` builds its own projection and `OrthoWidth`
is a first-class property the renderer honours. RAGE: the same for its map and blueprint views.
**They agree**, and glTF agrees too -- `camera.type: "orthographic"` with `xmag`/`ymag` is part of
the format this engine claims to read whole. The matter is closed and this item is that the tree
ACCEPTS the declaration and draws something else.

## What was measured, 2026-08-31

`ScenarioRead.cpp:649` reads `orthographic="yes"` with `xMagM`/`yMagM`; `SubjectProxy.cpp:180`
validates that `xMag` equals `yMag * aspect` to `1e-12` and calls `SetOrthoM(2 * YMagM)`;
`SceneRenderer::Framing` passes `OrthoM_` to `MvpCamRel`, which builds a real parallel matrix. The
whole chain exists. **What comes out is broken.**

`NormalTangentTest`, whose manifest declares `projection: "orthographic"`, `yMagM: 1.107692308`,
clip 4..6 -- the same subject, the same camera position, the same scene, TWICE:

| the view declared | what the frame shows | agreeing |
|---|---|---|
| perspective (what the translator wrote before) | a clean 6x5 grid of spheres, the word `Front` upright and legible | 72.0372% |
| **orthographic (what the manifest names)** | **geometry smeared and displaced, `Front` MIRRORED, most of the grid missing** | **81.7989%** |

**The score ROSE while the picture broke**, which is why this item exists and why the orthographic
declaration was taken back out of the translator rather than left in for the number.

`PointLightIntensityTest`, also orthographic, draws a COMPLETELY BLACK frame against an oracle of
six lit boxes -- and scores 57.6729% against 11.3325%, because the oracle is mostly black too. A
metric that rewards drawing nothing is a metric being read without its picture.

## What is NOT the fault

Three things were checked and cleared, so the next reader does not re-check them:

- `OrthoM_` is READ. It reaches `MvpCamRel` through `Framing()` and the first grep for it missed
  `SceneRenderer.cpp`; the setter is not dead
- the magnification agreement is EXACT and the pair now agrees. Nine printed decimals put them
  1 ulp apart at `1e-12` relative and every case was refused with a clear message; at seventeen
  significant digits the door accepts them
- the ortho matrix's reverse-Z is the right way round: `q[10] = 1/(zf-zn)`, `q[14] = zf/(zf-zn)`,
  so a nearer point takes a LARGER depth, which is what `SDL_GPU_COMPAREOP_GREATER` wants

## What is suspect and is not yet measured

`SetProjection`'s orthographic branch returns WITHOUT calling `SetNearM`, so `zn` in `MvpCamRel` is
whatever the previous frame left, while the perspective branch sets it every time. And `zf` is the
literal `60000.0f` regardless of the declared far plane, so a scene declared over 4..6 m is mapped
across sixty kilometres and every depth in it lands within one part in ten thousand of the same
value. Either would explain a smeared frame. **Neither is confirmed, and the mirrored text is
explained by neither** -- that is the first thing to chase.

## What will be true

- [ ] a scene declared orthographic draws the same subjects in the same places as the same scene
      declared perspective, differing only in the projection -- checked by LOOKING, because the
      metric rewarded the broken one
- [ ] the declared near and far planes reach the parallel projection, rather than a stale near and
      a literal 60000
- [ ] `render_corpus.py` declares `orthographic="yes"` for the four manifests that name it, and the
      four are scored through the projection their criterion is derived from
- [ ] Negative control: a subject that is a metre wide at the near plane and a metre wide at the far
      plane covers the SAME pixels under the parallel projection and different ones under the
      perspective one

## What this does NOT cover

The four cases holding. `NormalTangentTest` and `NormalTangentMirrorTest` compare a normal map
against a tangent basis and may disagree for reasons that have nothing to do with the camera; this
item is about them being scored through the projection their manifests name.
