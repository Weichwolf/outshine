Type: refactor
State: active
Parent: 2191
Depends: 2222
Area: render, engine, test
Tags: ownership, state, gpu

# Renderer world content publishes as a candidate

## Aktueller Arbeitsumfang

Der vorhandene Kandidat erfüllt den behaupteten Besitzvertrag noch nicht. `SceneRenderer::Candidate_`
ist ein vollständiger `SceneState`; dieser enthält `FrameResources`, Renderplan und `WorldContent`.
Damit baut ein Weltkandidat weiterhin zielgebundene Frame-Ressourcen auf. WI 2224 nutzt die
Transaktion, beweist aber keine Trennung der Besitzer. Kein zweiter Candidate-Owner und keine
neue Renderer-Fassade: der bestehende Typ wird entlang der tatsächlichen Lebensdauern geteilt.

Messung am Floor-Fixture vom 2026-09-21: `SetGeometry` kostet 209.1 ms, davon 207.5 ms
`RuntimeScene::StandsPlan`; Clustering 0.38 ms, Packing 0.15 ms und Mesh-Upload 0.30 ms.
Der abschließende World-Swap kostet 0.31 ms. Nur die Stage-Liste des Plans ändert sich: der
anfangs leere Kandidat besitzt keinen automatisch abgeleiteten Shadow-Pass, Geometrie fügt ihn
hinzu und erzwingt `InitForTarget`. Das widerlegt die bisher behauptete Typtrennung.

## Ursprünglicher Defekt

`RuntimeScene::Open` constructs a CPU-local `RuntimeScene`, then `RuntimeScene::Build` mutates the active
`SceneRenderer`: plan setup, mesh/material/placement uploads, lights, sky, picture region and
overlay. Each individual upload has a replacement failure path, but a later failure leaves an
incomplete new world mixed with the previous frame. The old `RuntimeScene` is still owned, so restoring
its CPU state cannot restore those GPU products.

## Decision

Split `SceneRenderer` into target-owned `FrameResources` and a move-only internal
`WorldContent`. `WorldContent` owns subject and glass residency, material tables, overlay atlas
and quads, persistent light/sky/camera declarations, picture region and every binding whose
lifetime follows a world rather than a target. It has no borrowed pointer into the published
content. Target-dependent bindings stay in `FrameResources` and bind the selected content only
after publication.

`RuntimeScene::Open` obtains an empty content candidate, builds the whole `RuntimeScene` through that candidate,
then publishes it with a nonthrowing move after all uploads and UI composition succeed. The former
content remains drawable until that move. Candidate destruction releases only its own GPU owners.
The existing move-only `FrameResources` transaction from WI 2222 is the local reference: local
RAII ownership, complete candidate construction, `static_assert`ed nonthrowing transfer, then one
publication point. Snapshot/restore and clearing the active renderer during candidate construction
are prohibited.

`Candidate_` darf daher kein vollständiger `SceneState` sein. Reiner Weltinhaltstausch baut nur
`WorldContent` und seine Deklarationen. Bei unverändertem `PlanSpec` verwendet er den bestehenden
Target-Frameplan. Ändert sich der Plan tatsächlich, baut die vorhandene Frame-Transaktion die
neuen `FrameResources`, während A renderbar bleibt; Frame und Welt werden danach gemeinsam
atomar publiziert. Aktive Frame-Ressourcen in den Kandidaten zu verschieben ist verboten.

The candidate remains in a local owner until publication. Only then may `RuntimeScene::Open` hand the
renderer from the previous output owner to that candidate. Building directly into the output owner
would instead detach the newly published `RuntimeScene` and leave later draw calls with a null renderer.

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
- `RuntimeScene` must not retain raw pointers to a candidate after it was rejected or published.

## Proof

- Inject failure separately at mesh upload, material upload, placement upload, light/sky binding,
  overlay atlas upload and overlay quad upload after an old rendered world exists.
- [x] A generated-world GPU submission failure after candidate construction preserves the former
  linear pixels and accepts the immediate declaration retry.
- [x] `RuntimeScene::Open` keeps its newly published owner renderer-bound; an imported-camera scene draws.
- Each rejection preserves old pixels, readable buffers, declaration and revision; the immediate
  retry publishes the new world exactly once.
- Repeated A→B→A declarations have bounded GPU ownership and no stale content binding.
- A negative control that writes the active content before candidate success fails the pixel and
  retry oracle.
- Render a static subject before and after an unrelated target change to prove content rebinding
  preserves pixels. Run relevant suites and `make lint`.

## Strukturaudit: vorhandene Besitzer weiterverwenden
Die RuntimeScene-Migration verschiebt Ressourcen in vorhandenen WorldContent-Besitz. SceneRenderer
verantwortet Submission/Device; kohärente Frame-/World-Parameter ersetzen durchgereichte
Stage-Details pro Aufrufergruppe. Kein zweiter Framegraph oder konkurrierender Commit.
