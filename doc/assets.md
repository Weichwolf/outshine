# Assets — built so a better successor can improve them

> Owner, 2026-08-05: *„beachte bitte, dass die assets so gebaut werden, dass neuere versionen von dir
> leicht verbessern können. das ist eine allgemeine vorgabe. neuere versionen von dir werden in allem
> besser und müssen effizient überarbeiten und verbessern können."*

**This is a general directive, not an asset rule.** It happens to bite hardest on assets, because a mesh
is the one artefact in this tree that can be *finished* without being *understood*.

## Spec

### 0. The rule

> **Version the recipe, never the cake.**

A `.glb` is a build output. What is committed is the script that produces it, the dimensions it was
produced from, and the sources those dimensions came from. `sim/assets/models/.gitignore` already
excludes `*.glb` for exactly this reason, and the F-16 round proved it works: `build_f16.py` +
`f16_geometry.py` + `f16.asset.json`, four LOD levels regenerated on demand.

### 1. What makes an asset improvable, concretely

A successor who is better at everything still cannot improve what it cannot read. Five properties, and
each is a thing that goes wrong when it is missing:

| Property | Without it |
|---|---|
| **Parametric source, not a mesh** | improving means re-modelling from scratch; nobody does, so the asset freezes at its first version |
| **Every dimension carries its source** | a successor cannot tell a measured span from a guessed one, so it dares not touch either |
| **Deterministic build** | a rebuild produces a different mesh, and no differential test can say whether a change helped |
| **The critic's open defects survive** | the next round rediscovers the same five flaws instead of starting where the last one stopped |
| **Acceptance recorded with its measurement** | „good enough" degrades into taste, and the bar drifts down or up without anyone deciding |

### 2. What that means per asset

```
sim/assets/models/<name>/
  build_<name>.py      the recipe — headless Blender, deterministic
  <name>_geometry.py   named dimensions, each with [SOURCE] / [DERIVED] / [SET]
  <name>.asset.json    LOD ladder, triangle counts, moving-node names, acceptance record
  DEFECTS.md           what the critic still holds against it, ranked — the next round's start
```

`.glb` files are outputs and stay untracked.

### 3. The two named rules this inherits

- **Every number carries its origin** — measured (with the measurement), derived (with the formula), or
  `[SET]`. A dimension without one is a defect, the same as anywhere else in this tree.
- **Node names are shared with physics.** The body format ([`body-format.md`](body-format.md) §2) puts
  joints at the same names as the visual hinges: `ctl.aileron.l`, `gear.main.r`, `turret.yaw`. Physics
  joint and visual joint are one name or the format is wrong.

### 3.1 Self-checks — invariants, not eyeballing

Cheap, exact, no rendering. Each catches a whole class of defect that a human artist misses by eye.

| Check | Fails when |
|---|---|
| **watertight** — Euler characteristic, zero boundary edges | a hole nobody sees until light leaks through it |
| **winding** — consistent CCW, no inverted faces | a surface that vanishes at a viewing angle |
| **normals** — unit length, orientation matching winding, no NaN | lighting that flips on a LOD swap |
| **no T-junctions**, vertices welded within epsilon | hairline cracks under any subpixel motion |
| **continuous LOD** — adjacent levels share their boundary exactly; geomorph endpoints coincide | cracks between tiles, and popping the eye finds before the metric does |
| **mesh merge** — a merged mesh has the same volume and bounds as its parts | silent geometry loss during simplification |
| determinism | build twice, compare bytes (§4) |

**Fractals test vegetation implicitly.** An L-system or IFS branching structure has a closed-form
definition — branch count, total length, fractal dimension, bounding volume are all known before
rendering. Running the vegetation pipeline on one tests instancing, LOD, impostor transition and wind
against exact expected values, with no artistic judgement anywhere in the loop. See
[`render/visual-target.md`](render/visual-target.md) §1.3.

### 4. Acceptance

| Contract | Anchor |
|---|---|
| Nothing is hand-tweaked | no committed mesh; every `.glb` regenerates from its script |
| A rebuild is identical | build twice, compare bytes |
| Improvement is measurable | `DEFECTS.md` shrinks between rounds, and the critic says which entry closed |
| The bar is written down | `asset.json` records what was accepted and against which reference |

## State

The F-16 follows this and is the only asset that does. Four LOD levels, 107 706 / 41 342 / 14 366 / 9 916
triangles, generated from named dimensions.

## Gaps

- **`DEFECTS.md` does not exist yet**, for the F-16 or anything else — the critic's findings live in
  conversation, which is exactly the place a successor cannot read.
- **Five F-16 defects are open** and unrecorded in the tree: cockpit is boxes, no nose-gear bay, nozzle
  petal count `[SET]`, tailplane hinge 0.62 c `[SET]`, and an L2 dead band where L1 and L2 both resolve
  at 692 m.
- **No rebuild-determinism check.** §4 requires it; nothing runs it.
