Type: bug
State: active
Architecture: planned
Priority: P0
Parent: 2191
Area: engine, render, flora, test
Tags: streaming, ownership, transaction

# Crown instance updates need frame-atomic resource swaps

## Problem

`VegetationStreaming::Step` accepts a finished atlas and then creates prototypes or replaces instance rows
on the published `RuntimeScene`. `Render::ImpostorInstances::Update` batches its views, but later groups can fail after earlier groups changed. A frame can
therefore contain a mix of old and new crown resources, while the CPU group state has already
advanced. Rebuilding the complete world candidate per foliage update would reupload terrain and
unrelated pieces, violating the streaming budget.
Creating crowns inside a resumable Ground candidate is also invalid: a later ground-revision
change can abandon its resources after `VegetationStreaming` retained their handles.
Frame encoding must continue from the published renderer state while such a candidate is open;
drawing its partial indirect tables caused an invalid Metal indirect-draw access in Malcesine.

## Decision

Give `RuntimeScene` and `SceneRenderer` a bounded resource-update transaction: validate every replacement,
allocate/uploads into inactive GPU resources, submit one ordered swap at a frame boundary, and only
then commit the matching `VegetationStreaming` state. A rejected transaction retains every prior prototype,
instance row and stable handle. Keep atlas IO/preparation outside the render transaction. Retire old
GPU resources after their final submitted frame.

The transaction describes changed resources only. It must not clone native geometry, terrain pages
or unaffected pieces. Resource handles remain stable across the swap and are invalidated only by an
explicit release.
Until that transaction exists, GPU crown creation and updates wait while a Ground candidate is open;
atlas IO/preparation remains independent. This serializes publication without losing CPU work.
Renderer frame execution always binds the published state; candidate mutation remains invisible
until the world transaction publishes.

## Proof

- Reject the second of several crown prototype or instance-row uploads; verify every prior row,
  resident count and rendered frame remains unchanged.
- Retry the same update and verify one frame-boundary publication with no leaked resources.
- Hold different published and candidate geometry across a frame; the image must remain the
  published image. The same oracle must fail when frame execution selects the candidate.
- Update a dense forest while terrain streams; measure CPU/GPU p50/p95/p99, peak resource count and
  upload bytes. Show cost proportional to changed crown resources, not complete world size.

## Re-audit 2026-09-28 und konkrete Integrationsgrenze

Vorhanden: ImpostorInstances::Update bündelt alle Views; SceneResources::SetPieceInstances
validiert Handles/Kapazitäten/Duplikate vor CPU-Mutation. Der aktuelle SubjectDraw-Setter
hat danach keine behandelbare Fehlermöglichkeit. Kein behaupteter partieller View-Fehler.
Zwischen mehreren Groups bleibt die Publikationsgrenze offen. HandTables mutiert Tabellen;
bei Uploadfehler werden Jobs/Args leer und Retry bleibt möglich, kein garantiert alter Frame.
PieceInstanceBatchRejectsPartialUpdates prüft ungültige Eingaben, keinen zweiten GPU-Upload.

Erster Entwurf: VegetationStreaming bereitet geänderte Groups samt Rows als EINEN
begrenzten Vorschlag vor. SceneResources besitzt validierte CPU-Kandidaten; SubjectDraw/
SubjectResidency besitzen inaktive Tabellen/Buffer und deren Submit-/Retirementzustand.
Erst vollständiger Upload/Submit publiziert Handles/Rows/Group-Phasen gemeinsam.
Kein WorldContent-Klon und keine Stage-/Pipeline-Neuerstellung für gleiche PlanSpec.
Alte GPU-Ressourcen bleiben bis letzter Nutzung; Abbruch verwirft nur Kandidaten.

Vor Architecture: ready müssen Table-/Material-/Piece-Kandidaten und Fehlerkanäle
vollständig festgelegt werden. Vorhandenen fail-closed Uploadvertrag erhalten, bis die
stärkere alte-Frame-Verfügbarkeit bewiesen ist; Tests nicht nur auf grün umschreiben.
Kontrollen: zweite Group, zweiter Material-/Buffer-Upload, Submit und laufender Ground-
Kandidat; alte Bild-/Handle-/Residency-Snapshots unabhängig prüfen, Retry und Shutdown.
