Type: feature
State: open
Progress: gpu-driven
Area: render
Tags: benchmark, target

# The picture REFLECTS, and a mirror is not a hole

**Benchmark** — Unreal: Lumen traces reflections (software or hardware), with screen-space
reflection as the fallback and reflection CAPTURES — cubemap probes placed in the level — behind
that. Pre-Lumen it was SSR plus captures. RAGE: screen-space reflection over the deferred buffers,
a planar reflection for water, and per-region cubemap probes for what leaves the screen. **Both
agree on the ladder: screen space first, a probe for what the screen does not hold**, and both
answered it on hardware far below a ray-tracing budget. **Taking that ladder.** Lumen is out on
five GPU cores at 720p60 and neither engine needed it to look right.

## Why this is filed at all

`CLAUDE.md` now states what the engine is laid out for: high geometry, many lights and shadows,
realistic atmosphere, parameterised materials, **reflections and mirroring**. Four of the five have
something in the tree. This one has **nothing**:

    grep -i 'reflect|SSR|probe|cubemap' over the render catalogue and the stages
      -> MetalRoughBrdf's Fresnel term, and nothing else

There is no reflection stage, no probe resource, no capture, no screen-space pass. `board/` had no
item for it either -- the word appears eight times across the board and every one is "reflects the
causal chain" or similar. So the gap was in the plan as well as in the code.

**What EXISTS and is close**: `subjectsTransmissive` and `compositeTransmission` already partition
a pass over one residency (board:1574), so the machinery for a second pass that reads the first
pass's colour is built and proven. A screen-space reflection is that shape.

- [ ] a screen-space reflection stage stands in the catalogue and executes, reading last frame's
      or this frame's colour and depth
- [ ] a material's roughness drives it: a mirror reflects sharply, a rough surface does not, and
      the falloff is the BRDF's own rather than a second invented one
- [ ] what leaves the screen is answered by a probe rather than by black
- [ ] the cost is measured at 720p on this device, before and after, through `apps/bench`

**The measurement that would show I am wrong**: if a screen-space pass at 720p costs more of the
frame than the atmosphere does, the ladder's first rung is the wrong one on this hardware and the
probe becomes the primary rather than the fallback. `apps/bench`'s stage table already prints what
each stage costs, so the comparison is one run.


## THE WATER IS DRAWN NOW, AND THIS ITEM'S SUBJECT IS WHAT IT DOES NOT DO

A water surface is geometry at last: a flat lid over its own ring at the level its shore gives it,
231 surfaces and 5 975 triangles at Venice, and the Giudecca reads as water where the lagoon was
terrain-green in every frame before. The history recorded the gap twice -- "WaterStage: Wasser ist
Geometrie -- gezeichnet, Lage noch nicht belegt" and "WaterField: der Pegel kommt vom Ufer, nicht vom
Hang -- noch zeichnet ihn nichts" -- and it is now drawn, so this item is not about existence any
more.

What it still has NO part of, stated so the next reader is not misled by a blue surface: no planar
reflection, no refraction through it, no wave normal, no motion. It is a lid, and it is the geometry
those will need.

MEASURED and honest about its edge: only the water inside the loaded vector ring is laid, so
Venice's far lagoon is still terrain-green. That is the vector ring's reach, not this item's.
