Type: refactor
State: active
Parent: 2191
Depends: 2222
Area: render, engine, test
Tags: ownership, state, gpu

# Renderer world content publishes as a candidate

## Aktueller Arbeitsumfang

SceneState/WorldContent-Kandidaten sind implementiert; WI 2224 nutzt sie bereits.
Die folgende Problembeschreibung begründet den Vertrag, nicht einen erneuten Umbau.
Nächste Arbeit ist die noch offene Fehler-/Rebinding-Matrix unter Proof, über bestehende
Produktionsoperationen. Kein zweiter Candidate-Owner und keine neue Renderer-Fassade.

## Ursprünglicher Defekt

`Live::Open` constructs a CPU-local `Live`, then `Live::Build` mutates the active
`SceneRenderer`: plan setup, mesh/material/placement uploads, lights, sky, picture region and
overlay. Each individual upload has a replacement failure path, but a later failure leaves an
incomplete new world mixed with the previous frame. The old `Live` is still owned, so restoring
its CPU state cannot restore those GPU products.

## Decision

Split `SceneRenderer` into target-owned `FrameResources` and a move-only internal
`WorldContent`. `WorldContent` owns subject and glass residency, material tables, overlay atlas
and quads, persistent light/sky/camera declarations, picture region and every binding whose
lifetime follows a world rather than a target. It has no borrowed pointer into the published
content. Target-dependent bindings stay in `FrameResources` and bind the selected content only
after publication.

`Live::Open` obtains an empty content candidate, builds the whole `Live` through that candidate,
then publishes it with a nonthrowing move after all uploads and UI composition succeed. The former
content remains drawable until that move. Candidate destruction releases only its own GPU owners.
The existing move-only `FrameResources` transaction from WI 2222 is the local reference: local
RAII ownership, complete candidate construction, `static_assert`ed nonthrowing transfer, then one
publication point. Snapshot/restore and clearing the active renderer during candidate construction
are prohibited.

The candidate remains in a local owner until publication. Only then may `Live::Open` hand the
renderer from the previous output owner to that candidate. Building directly into the output owner
would instead detach the newly published `Live` and leave later draw calls with a null renderer.

`LightVisibilityStage` and `SubjectCullStage` may retain a `SubjectDraw` address while pipelines
remain frame-owned. Publication therefore rebinds those addresses without allocation; the generator
composition oracle exposed the stale-candidate-pointer failure before this contract was added.

## Boundaries

- Device, window claim, frame attachments and GPU fences remain renderer/platform state.
- Generated mesh, imported mesh, materials, placements, overlays, environment and camera belong
  to `WorldContent`; ground streaming stays independently owned until its declaration transition
  receives the same transaction.
- A target change must rebind the published content to its new frame bindings without copying or
  re-uploading world data.
- `Live` must not retain raw pointers to a candidate after it was rejected or published.

## Proof

- Inject failure separately at mesh upload, material upload, placement upload, light/sky binding,
  overlay atlas upload and overlay quad upload after an old rendered world exists.
- [x] A generated-world GPU submission failure after candidate construction preserves the former
  linear pixels and accepts the immediate declaration retry.
- [x] `Live::Open` keeps its newly published owner renderer-bound; an imported-camera scene draws.
- Each rejection preserves old pixels, readable buffers, declaration and revision; the immediate
  retry publishes the new world exactly once.
- Repeated A→B→A declarations have bounded GPU ownership and no stale content binding.
- A negative control that writes the active content before candidate success fails the pixel and
  retry oracle.
- Render a static subject before and after an unrelated target change to prove content rebinding
  preserves pixels. Run relevant suites and `make lint`.
