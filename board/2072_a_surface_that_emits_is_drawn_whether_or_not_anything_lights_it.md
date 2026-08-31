Type: defect
State: active
Area: render
Tags: shading, corpora, measured

# A surface that EMITS is drawn, whether or not anything lights it

**Benchmark** — Unreal: `Emissive Color` is an output of the material and reaches the GBuffer's
emissive channel before any light is gathered; an unlit level still shows an emissive mesh. RAGE:
emissive is a term of the shader's own output, not a response to a light. **They agree and the
physics agrees**: emitted radiance leaves a surface whether or not anything arrives at it, so a
scene with no lights renders emission and nothing else.

## Measured, 2026-08-31

`SubjectProxy.cpp:237`

    Lit(proxy, subject, part) = Parts[part].HasNormal && Gathers(proxy) && !Row.Unlit

and `Gathers` is true only when a punctual light or a non-zero indirect radiance stands. So a scene
with NO light takes the unlit path -- and every unlit fragment body in `subject.msl` reads
`SUBJECT_COLOUR_TAP` and nothing else:

    fsTextured:  o.col = in.emitted * SUBJECT_COLOUR_TAP(SUBJECT_UVS(in)).rgb * in.colour.rgb

`in.emitted` is one FLAT float3 per vertex. **There is no path in the unlit shaders that samples the
emissive map at all**, so a surface emitting from a TEXTURE emits a flat colour times the wrong
image when nothing lights the scene.

Proven by construction rather than by reading: `MultiUVTest`, whose oracle is a black field with
three glTF logos, with `unlit` dropped and an environment of `1e-6` -- a radiance six orders below
anything visible, present only to make `Gathers` true:

    unlit, no light           the cube in flat uv0 colours, NO logo    88.41% agreeing
    lit, environment 1e-6     THE ORACLE'S PICTURE, three logos        LOOKED AT

The engine reads `TEXCOORD_1` and samples the emissive map correctly. It just declines to do either
unless something else is lit, and **needing a fake light to see an emitter is the defect.**

## Why the unlit path is RIGHT to ignore it, and this is still a defect

`KHR_materials_unlit` states the shading model is `color = baseColor` and that `emissiveFactor`,
`emissiveTexture`, the normal and occlusion textures and the metallic-roughness pair are all
ignored. So `fsTextured` is CORRECT for a material that declares the extension. The fault is the
routing: a material that does NOT declare unlit is sent down the unlit path anyway, purely because
no light stands in the scene, and there it loses a property the format gives it.

## Three corpus cases are red for this

    EmissiveStrengthTest            0.4444%
    MultiUVTest                    88.4148%
    TextureLinearInterpolationTest 89.0897%

All three are the manifests that state `material.source: "gltf-emissive"` -- the ones whose oracle
is an emitter fed by the emissive image. Nine others state `gltf-base-colour` and are unaffected.

## What will be true

- [ ] a part is sent down the LIT path when its surface emits from a MAP, whatever the scene holds,
      because emission is a term of the surface. With no lights the lit path's gathered terms are
      zero and the emission is what remains, which is the physics
- [ ] `MultiUVTest` draws the three logos with NO environment declared at all
- [ ] Negative control: the same subject with its emissive map removed draws what it drew before,
      so the change reaches emitters and nothing else
- [ ] the three cases are re-scored and what remains apart is named

## What this does NOT cover

The flat `in.emitted` vertex value itself. It is the right answer for a material whose emission is a
CONSTANT, and it stays; this item is about the case where the emission is an IMAGE. Whether a
declared-unlit material should also carry emission is decided by the extension and the answer is no.
