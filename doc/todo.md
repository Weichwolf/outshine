# Now

| | |
|---|---|
| **Working on** | Buildings, round two — mass articulation, kerb and pavement, doors that face the street, per-building identity |
| **Scope** | `doc/requirements.md`: **1290 features, 183 ticked, 1107 open** |
| **Last accepted** | Buildings round one — roofs, façades, marched-recess openings (`d0b114d`) |

## Next, in order

1. **`scenarios/` replaces `mods/`, and the URL names the scenario.** The documents have said
   `scenarios/` since `035a657`; the code still says `mods/`, `clients/Mod.{h,cpp}` and `Scene`, and
   there is no `Scenario` type anywhere in the tree. The accepted interface is the shape: one
   `Scenario` whose axis is **camera × clock** rather than a hierarchy (slideshow = several `Fixed` +
   rate 0 · film = one `Keyframed` + timeline · interactive = `Driven` + rate 1), `ScenarioSource` as
   a pull seam that assumes no document, `JsonScenarioSource` as the one implementation, `Link()` as
   a second pass after which a dangling reference is unspellable, and a decided error table with no
   partial scenario and no fallback.

   **In this round:** the rename, the `Scenario` type, the two loader files, `GET /<scenario>` serving
   a client already loaded, and the deletion of `OUTSHINE_MOD`/`OUTSHINE_SCENE` (`clients/SimHost.cpp`,
   `sim/up.sh`) plus the `window.FB_SCENE` injection in `tools/browser_run.cjs` — a tool navigates.
   **Not in this round:** `Director`, `Voice`, `Perception`/`Intent`/`System`, characters and
   relationships. They are the same design and a later step.

   **Cost of not having this, measured:** a still-frame round left `OUTSHINE_SCENE=frame` in the
   running container at 12:59; every session after it silently got a non-interactive scene, where the
   input path is never registered, and three rounds went to a browser that was never at fault. The
   scene was settable in three places and declared in none.
2. **The telemetry carries the camera.** Eye position and look direction per row. A run whose subject
   is motion cannot answer "did anything move" from its own record — that is what made the above take
   three rounds instead of one grep.
3. **Vegetation forms.** The grower shapes **one** — a single-stem tree — and all sixteen species ride it.
   Multi-stem shrub, bush, hedge, coppice, pollard, tussock, rosette, fern, reed and the deadwood forms
   are what the picture is missing. `habit` in a species file is prose nothing reads.
4. **The grass stratum**, as a field evaluated at draw time and not as bodies. `render/Sward.h` is the far
   end of it and is built; the near end is not.
5. **Overdraw**, before more vegetation lands. It is the number the strata are judged with and it does not
   exist. Bucketed `atomicAdd`, 16 KB readback, no new pass.
6. **The water level.** 34 of 42 bodies sit under their own ground: the fifth percentile of a ring under
   22 points *is* its minimum. Hydro-flattening is the published answer.
7. **One rank per stand per frame**, and the far rank is one card. The bow-tie crowns are a failure the
   reference cannot have — it never draws a second quad.
8. **Occlusion between 1 m and 20 m.** Contact AO reaches 0.9 m, the third shadow cascade resolves 1.2 m
   per texel, and a tree is the whole span between.
9. **The night.** Nothing emits, sky irradiance is zero, the ground is lit by a constant crutch.

## Open defects

- The demo road reads as a dirt track since the unmapped substrate landed: the ground fragment uses the
  default row as the **runner-up** class where the structure has no second hit.
- `Sim::Features` gained a slice, but a feature inside the tile's 23.3 m buffer still yields twice.
- Nothing evicts against a heap fixed at 296 MiB. Monotone growth is a maximum walk length.
- `ClusterDag.h:75` reads `FB_TAU` from the environment — the picture depends on an undeclared variable.
- The winding is hard-coded at seven sites; it belongs in the draw product beside the cluster list.
- A crossing costs +1.77 ms at p50 against its neighbourhood, 1.03 of it the ring's own snapshot — in no
  column, because `Populate` runs after `Refine` inside one function.
