# The standing goal

Build the engine open-ended against one reference scene until this is revoked. The roadmap says what
comes next; this says how it is built.

## The scene

`mods/demo/scene.json` — **position, direction, time, wind, cloud cover. Nothing else.** Everything
visible is produced by the engine from DEM, OSM and the classification chain. **A mod never declares a
tree or a turbine.** It is a measuring bench, not a game.

The standpoint is mine and it is a measuring position. It moves when it fails to show the layer under
judgement, never in the middle of a running measurement.

## The target

**Days Gone and Horizon Forbidden West.** Not for their look but for what they hold on PS4-class
hardware: a full field of view, GPU-driven placement, lighting decoupled from geometry. Outshine does
that from tile-server data alone. For the look, Witcher 3 / Fallout 4 / GTA 5 — they reach their impact
with **less** detail than a photograph, carried by light and silhouette.

**16.67 ms at 1280×720 — 720p60.** Whatever holds 60 holds 30 with headroom, and the headroom is the
point.

## The order of work

**Geometry → LOD → lighting → colour.** Detail comes from geometry, not from procedural noise on a flat
surface.

Layers: **terrain + buildings → trees → perennials → turbine → grass (last, or never).** Grass is
stopped — it held everything up, and the aggregate machinery it needed belongs to trees, where there is
a measured precedent.

Each layer runs **build → critics accept → optimise**, and optimisation happens inside the layer, never
saved up. A layer is not finished because it works; it is finished when the critics have nothing left.

## Rules

**What is at a place is a property of the place, never of the viewer.** The classification may not
change as the camera moves. A filter decides what is *drawn*, never what is *there*. This outranks
every beautification.

**Classes interpolate as weights, never as an index.** The mean of "meadow" and "asphalt" is not a
class; a coverage fraction is a quantity and filters correctly. Transition widths belong to the ordered
class pair — a built edge is a component, a grown edge is a zone — and they are baked into the weights,
not interpolated in the shader.

**Nothing below tree size moves.** Grass, shrubs and ground cover are static; Fallout 4 was and it
looked good. Swaying branches are nice-to-have. Whatever *does* move needs a published anchor — tip
speed ratio, sway frequency — measured in the simulation state and in the rendered image.

**Every number carries its origin:** derived with the formula, measured with the measurement, or `[SET]`
and named as such. A target that turns out to be arithmetically unreachable is **disproved, not voted
away**, and the measured failure goes in `## Gaps`.

**One version.** No quality levels during fundamental development. If a switch offers two behaviours,
one of them is wrong.

**Delete what you supersede, in the same round.** Git holds the history. Leaving a dead path alive is
worse than deleting one line too many, because a dead path still fires. Escape hatches are dead paths;
diagnostics (`FB_FIELD_DUMP`, class visualisation, `FB_JITTER`) are not.

**Epoch and decay are discrete** — three each, a selection, not a blend. Not built yet; the two indices
are threaded everywhere a material sits and read nowhere.

## Measurement

**Commit after every accepted step.** "Git will bring it back" only holds once it is in. The tree went
**30 h 30 min** uncommitted through the `FB`→`outshine` cut, the deletion of the combat layer, the ground
shader, TAA, wind, LOD and the tree generator — every deletion argued with "git has it" stood on an
assumption that was false, and 466 measured lines were lost proving it.

**Every measurement pins its binary** — copy, hash, render from the copy. A figure taken from
`build/gpu_walk` directly is not a measurement.

**Performance is a distribution over a moving camera, never a mean and never a minimum.** Several
hundred frames, with movement at walking, sprint and well above, plus fast panning. Report p50, p95, p99
and the trend. Stutter is a p99 event.

**Measure rather than reason.** Owner, 2026-08-07: *„viel und ausgiebig messen und benchen ist eh gut.
rechenzeit ist billig, du bist teuer."* Do not argue about what a number probably is — run it. Do not
take one sample where hundreds are free. Do not carry a stale figure forward; re-measure. Every wrong
call this session came from an argument standing in for a measurement.

**A logged number is only evidence if it can move with the thing under test.**

**Development is strictly serial. One agent in the tree at a time.** Owner, 2026-08-07: *„in zukunft
nicht parallel laufen lassen. development immer strikt seriell."* File-level separation stops agents
overwriting each other; it does **not** stop interference, because the working tree and the compiler are
shared — a build during someone else's edit sees a broken intermediate that belongs to nobody, and a
measurement during someone else's build is worthless. That already cost three hours of critic work when
`build/gpu_walk` was rewritten under a running critic and three "non-deterministic states" turned out to
be three binaries.

**No critic renders while a developer holds the same file.**

**Attack the frame, not only the numbers.** A constant that defects cluster around is a suspect.

## Judgement

A plant or a building is rendered **alone first** on `gpu_walk --rig` and enters the scene only after
`botanist` / `architect` / `art-director` accept it there. The complete image is judged by `sim-critic`
alone. Quality against the reference is decided by the Blender comparison, not by an opinion.

The owner comments as it goes; those comments outrank this file.
