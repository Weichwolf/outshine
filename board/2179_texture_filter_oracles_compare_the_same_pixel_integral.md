Type: bug
State: active
Architecture: ready
Parent: 2171
Depends:
Priority: P1
Area: test, harness, render
Tags: khronos, measured

# Texture filter oracles compare the declared pixel integral

## Problem

The present Khronos runner rejects the declared `factory` world for
`ABeautifulGame`; its one-sample reference therefore cannot exercise the imported
scene. The four-texels-per-pixel case, which declares a 256-sample pixel integral,
is unreachable for the same reason. Changing a grey value, pin, tolerance or
manifest would hide a translation defect.

The oracle also needs a deterministic first GPU frame. `MipmappedChessRepeatsLinearPixels`
currently proves that this prerequisite is absent; WI 2219 owns the renderer repair.

## Architecture decision

The test harness translates a declarative fixture into the public client/Engine path.
It owns neither a second renderer nor an engine-specific reference image. Fixture data
declares scene source, camera, lights, material closure, colour space, exposure,
sampling population and expected artefact identity. The translator rejects an
unsupported world, closure or colour transform before rendering.

Each reference test has two independent assertions:

1. a CPU analytical integral for signals with a declared finite population;
2. a GPU-rendered, externally pinned image for the same declaration.

The two checks share input data, never a computed expected image. Metallic-roughness
translation remains distinct from a diffuse closure: no retained specular lobe is
allowed when the fixture declares diffuse. Native colour maps are sRGB; data and
normal maps are linear; mip construction happens in linear light before the declared
GPU encoding.

## Implementation order

1. **P1, independent of 2219:** Make the existing factory-world translator complete and
   reject every undeclared conversion explicitly.
2. Run `four-texels-per-pixel` through Make. Its analytical integral must reject a
   base-only chain, gamma-space averaging and a deliberately swapped texel.
3. Run `ABeautifulGame` with its declared point population and independently pinned
   GPU reference. Preserve the current point fixture; do not make it impersonate the
   integrated case.
4. Add linear repeat checks for unlit colour, lit metallic-roughness/normal maps and
   an animation sample. WI 2219's first-frame contract is reused, never warmed up.

## Factory translation decision

render_corpus.py already defines kFactoryWorldRadiance but rejects an explicitly named
factory world. Verify the pinned oracle provenance against that value and Blender's
prep/in_blender_render.py factory branch. Translate explicit factory and absent world
consistently only when the declared oracle contract is identical; never infer lighting
from the produced image. Unknown kinds still fail before client execution. Unit controls
cover none, uniform, factory, absent and unknown worlds without starting Blender.

## Implementation boundary

Use test/khronos declarations and test/scripts/render_corpus.py; rendering
continues through outshine-client/public Engine. Translator and analytical controls can
ship now. Final exact repeat acceptance remains unresolved until WI 2219 is repaired;
this is not a reason to block translation work or silently mark the entire WI complete.
Commands: make format; make corpus-render CASES='ABeautifulGame'; make lint.
Run make corpus-render CASES='SimpleTexture/four-texels-per-pixel'.
Normal runs never regenerate reference pins or invoke Blender.

## Acceptance

- [ ] Both existing fixtures execute through Make without a translation exception.
- [ ] Camera, lighting, closure, colour encoding and sample population are declared
      once and verified by positive and negative controls.
- [ ] Wrong mip construction, wrong colour space, a missing level and a changed
      sample fail their respective oracle.
- [ ] First, subsequent and redeclared static frames agree exactly; reference pins
      and acceptance limits remain unchanged.
