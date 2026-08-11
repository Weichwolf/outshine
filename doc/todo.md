# Now

| | |
|---|---|
| **Working on** | Buildings, round two — mass articulation, kerb and pavement, doors that face the street, per-building identity |
| **Scope** | `doc/requirements.md`: **1290 features, 183 ticked, 1107 open** |
| **Last accepted** | Buildings round one — roofs, façades, marched-recess openings (`d0b114d`) |

## Next, in order

1. **Vegetation forms.** The grower shapes **one** — a single-stem tree — and all sixteen species ride it.
   Multi-stem shrub, bush, hedge, coppice, pollard, tussock, rosette, fern, reed and the deadwood forms
   are what the picture is missing. `habit` in a species file is prose nothing reads.
2. **The grass stratum**, as a field evaluated at draw time and not as bodies. `render/Sward.h` is the far
   end of it and is built; the near end is not.
3. **Overdraw**, before more vegetation lands. It is the number the strata are judged with and it does not
   exist. Bucketed `atomicAdd`, 16 KB readback, no new pass.
4. **The water level.** 34 of 42 bodies sit under their own ground: the fifth percentile of a ring under
   22 points *is* its minimum. Hydro-flattening is the published answer.
5. **One rank per stand per frame**, and the far rank is one card. The bow-tie crowns are a failure the
   reference cannot have — it never draws a second quad.
6. **Occlusion between 1 m and 20 m.** Contact AO reaches 0.9 m, the third shadow cascade resolves 1.2 m
   per texel, and a tree is the whole span between.
7. **The night.** Nothing emits, sky irradiance is zero, the ground is lit by a constant crutch.

## Open defects

- **WASD and mouse capture do not work in the wasm client.** The registration site reads correct and the
  canvas id matches; the log decides it — `walk pointerlock`, and whether `yawDeg` moves under a held key.
- The demo road reads as a dirt track since the unmapped substrate landed: the ground fragment uses the
  default row as the **runner-up** class where the structure has no second hit.
- `Sim::Features` gained a slice, but a feature inside the tile's 23.3 m buffer still yields twice.
- Nothing evicts against a heap fixed at 296 MiB. Monotone growth is a maximum walk length.
- `ClusterDag.h:75` reads `FB_TAU` from the environment — the picture depends on an undeclared variable.
- The winding is hard-coded at seven sites; it belongs in the draw product beside the cluster list.
- A crossing costs +1.77 ms at p50 against its neighbourhood, 1.03 of it the ring's own snapshot — in no
  column, because `Populate` runs after `Refine` inside one function.
