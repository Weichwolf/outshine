Type: task
State: open
Area: door
Tags: gltf, lighting, corpora

# A file that carries its own light is LIT BY IT

**Benchmark** — Unreal: importing a glTF with `KHR_lights_punctual` creates the corresponding
`ULightComponent`s and the scene is lit by them. RAGE: a `#map` carries its lights and the level is
lit by what it declares. **They agree** -- a scene's own lights are part of the scene -- so the
matter is closed and this item is that outshine does not do it.

## What was measured

Two Khronos cases state `scene.light.kind: "gltf"`, meaning the oracle was lit by the punctual
lights the FILE carries. Through `outshine-client`, against Blender's frame:

    PointLightIntensityTest    11.33 per cent of pixels within 8 of 255
    DirectionalLight           85.76 per cent

A scenario cannot say "and use the lights this asset carries". `<lighting>` declares a key by
elevation, bearing and lux, and an environment by colour -- both of which are the SCENARIO's
lights, not the file's. So the two cases are drawn unlit and score what an unlit picture scores.

## What will be true

- [ ] the importer reads `KHR_lights_punctual` and the engine lights the scene with what it read
- [ ] a scenario can REFUSE them as easily as accept them, because a scenario that stands its own
      lighting must be able to say the file's are not wanted
- [ ] the two cases hold at 99.99 per cent of pixels within 8 of 255

## Why this is not an exclusion

It was considered. Blender renders these cases correctly and states how, so the oracle is sound and
the corpus is asking a fair question; what is missing is on this side of the door. An exclusion
here would hide a capability gap behind a line that says the oracle is at fault, which is the
opposite of what `test/khronos/excluded.txt` is for.
