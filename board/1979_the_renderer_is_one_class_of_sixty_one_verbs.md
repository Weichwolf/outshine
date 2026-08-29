Type: issue
State: open
Parent: 1953
Area: render

# The renderer is one class of sixty-one verbs

**Benchmark** — Unreal: `FScene` survives frames, `FSceneRenderer` lives for one, RDG passes declare their resources. RAGE: the draw list is separate from the device. **Taking Unreal** — three lifetimes, three types.

`STATE.md`, Carpet: **`src/render/SceneRenderer.h` carries 61 `[[nodiscard]]`** — the widest public
surface in the tree, ahead of the glTF document (51) and the glTF subject (46).

**Unreal splits exactly this into three things with three lifetimes.** `FScene` holds what
persists across frames — primitives, lights, GPU-resident instance data. `FSceneRenderer` exists
for the duration of ONE frame and owns the passes. The passes themselves are RDG nodes declaring
resources and dependencies. RAGE separates the draw list from the device the same way.

Here all three are one object, so nothing in the type says what survives a frame and what does
not, and a caller that wants to add a pass reaches the same door as one that wants to hand over a
mesh.

The evidence that this hurts is already on the board: board:1826's remaining half is that the
renderer's conventions are learned by getting a call ORDER right rather than by being refused, and
a 61-verb door is where that comes from — order is the only structure a flat surface has.

- [ ] what survives a frame and what does not are different types
- [ ] a pass declares its resources; adding one does not widen the renderer's door
- [ ] `STATE.md`'s Carpet no longer names `SceneRenderer.h` first
