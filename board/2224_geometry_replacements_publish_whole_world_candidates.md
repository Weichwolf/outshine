Type: bug
State: active
Parent: 2191
Depends:
Architecture: ready
Area: engine, render, test
Tags: geometry, ownership, state, gpu

# World replacements publish one coherent native revision

## Architekturvertrag

Review 21342822f: `GroundStack::Restand` ruft bei neuer Vektorgeneration
`Footprints_.ResetDerived()` auf und ersetzt Ways_/WaterBodies_ sofort. Damit sind
veröffentlichte CPU-Ableitungen vor Candidate-Publish geleert; PNG-Erhaltung allein
prüft das nicht. Invalidation muss die nächste abgeleitete Generation markieren;
der bisherige publizierte Owner bleibt bis zum gemeinsamen Commit nutzbar.
Regression: publiziere A, liefere neue OSM-Kacheln, lehne B spät ab; prüfe alte
Footprints/Netze samt Abfragen sowie Bild/Audio und danach gültigen B-Retry.
Nebenbefund: `Advancing.cpp` subtrahiert IngestedTiles über diesen Reset unsigned;
Cost.StreamedTiles ist derzeit ungenutzt. Entfernen oder echte Arbeitsereignisse
zählen; keine bloße Nullklammer als Ersatz für den Publikationsvertrag.

`Core::RuntimeScene` besitzt native Weltinputs; `Render::WorldContent` besitzt daraus erzeugte
GPU-Produkte. `Surrounds` besitzt Streamingzustand, logisches Netz und Ressourcenhalter.
Ein vorbereiteter Nachfolger veröffentlicht diese Produkte gemeinsam auf dem Engine-Thread.
Provider-Anfragen und Vorbereitungscaches dürfen fortschreiten; veröffentlichte Geometrie,
Materialzuordnung, Lichtparameter, Netz, Audio-Occlusion und Revision bei Ablehnung nicht.
Keine Whole-World-Mutation mit anschließendem Snapshot-Rollback. Unabhängige gestreamte
Tiles bereiten neue Handles vor, geben sie bei Fehler frei und tauschen erst bei Erfolg.

## Vorhandene Grundlage

590696be3 invalidiert gecachte Sichtbarkeit bei Publish trotz gleicher Owner-Adresse
und lokaler Generation. Wiederholte Weltwechsel und statische Wiederverwendung geprüft.

`WorldCandidate.h` kapselt Prepare/Publish/Abandon und die schmalen Kandidatenoperationen für
`RuntimeScene` und Renderer. Es gibt keinen `RuntimeScene&`-Fluchtweg mehr: Grounding,
Material-/Geometrieaufbau und Diagnose laufen nur über benannte Kandidatenoperationen. Bei Fehler
zerstört RAII ausschließlich den Kandidaten. Abgelehnte verschachtelte Vorbereitung darf den
äußeren Kandidaten nicht verwerfen. `GroundWorldCandidate.h` ergänzt Sheets,
Terrainpositionen/Indizes, Netz, Materialslots und Revision; Revision wird zuletzt gesetzt.
`Surrounds::BindSceneResources` bindet Pieces, Sheets und Crowns ohne Allokation neu.
Ground-Klassen-GPU-Puffer gehören zum WorldContent, ihre CPU-Inputs zu Live.
Piece/Page-Identität verwendet native generational Handles und wiederverwendbare Slots.
Kandidaten kopieren Identitäten/Freilisten, nur Live übersetzt GPU-Adressen. Native
GroundTile-Daten enthalten keine als Float kodierten Handles; der Upload prüft exakte
GPU-Adressdarstellung. HeightSheets bereitet den Seitenersatz vor der alten Freigabe vor.

## Nächste Schritte in Reihenfolge

Die öffentliche Geometrieersetzung hat bereits die richtige Grenze: sie baut die
Audio-Occlusion vor dem Kandidaten, `RuntimeScene::ReplacesGeometry` bereitet alle fehlbaren
GPU-Produkte vor, und erst `PublishesPreparedWorld` tauscht den Owner. Danach sind
`Surrounds::BindSceneResources` und der Occlusion-Move nichtwerfende Übergaben. Ein
fehlgeschlagener Submit lässt daher Welt, Audio und Bild bei A; ein Retry darf B
publizieren. Das ist kein Blocker für Struktur-Bake-Shutdown oder -Budgetierung.

1. `GroundBuildProducts` besitzt `BuildingField` und `TilePieces` bis zum atomaren
   `GroundWorldCandidate::Publish`. Danach veröffentlicht der Live-Stream genau ein
   unabhängiges Struktur-Tile pro Frame direkt über stabile Handles; Wall und Roof
   werden gemeinsam ersetzt, während `RuntimeScene` und alle anderen Tiles stehen bleiben.
   `StructureBuildQueue` erhält den Footprint-Owner ausdrücklich. Ein unanchored Field
   lehnt Bake-Aufnahme ab; `BakeRevisionRejectsChangedInputs` prüft die Revision.
   Noch offen: A→B→spätes-A über die öffentliche API, GPU-Submit-Fehler während
   Ground-Bake-Publikation und erneuter Bindungsnachweis nach Kandidatenwechsel.
2. Stale Ergebnisse abweisen: Bake-/Ground-Anfragen tragen die benötigte Datenrevision
   einschließlich Projektion und Quellidentität. Vor Publikation mit aktuellem Auftrag
   vergleichen. Veralteten fertigen Job freigeben, ohne aktuelle Welt/Revision zu verändern;
   gleiche Tile-ID allein ist keine Identität.
3. Öffentlichen Gesamtpfad testen: kleiner deterministischer OSM-/DEM-Provider, Engine-API,
   zunächst gültige Welt A, dann B mit spätem Klassen-/Geometrie-Submitfehler.
   Materialmapping, Albedo, tatsächliches Routingnetz, GPU-Readback und Bild von A erhalten;
   Retry liefert B. Beide SDL-Submitfunktionen im bestehenden Fault-Injection-Stil erfassen.
4. Öffentliche Geometrie-A→B→spätes-A samt GPU-Submit-Fehler als einzelnes
   `test/outshine/include/Outshine/`-Oracle ergänzen. Es misst Pixel und einen
   verdeckten Audio-Strahl von A, erzwingt die späte Ablehnung von B und verlangt
   unmittelbar danach den erfolgreichen B-Retry. Dies belegt den bestehenden
   Übergang; keinen zweiten Transaktionsrahmen einführen.
5. `Restands`, surface-only redeclare, Kamera-/Animationsersatz und öffentliche
   Geometrie-/Audio-Occlusion im Übergangsinventar von WI 2191 prüfen. Pro Übergang ein
   vollständiger Änderungsschritt; kein allgemeines Transaktionsframework auf Vorrat.

## Abnahme und vorhandene Nachweise

- `StructureTilePublication/TileChangesPublishAtomically.cpp`: leerer Ersatz entfernt
  Gebäude; unvollständige Dreiecke und Dachfehler erhalten alle alten Tiles und Payloads.
  Direkte Einzel-Tile-Publikation ersetzt keinen `RuntimeScene`-Owner. Rosenheim bleibt
  8e6642f9; Live-Transfer fällt von 52.62 auf 2.38 ms und Frames über 16.67 ms von 24
  auf 9. Whole-world Ground-Revisionswechsel bleiben Kandidatenoperationen.

- `test/outshine/src/engine/GroundWorldCandidate/LateFailurePreservesPublishedGround.cpp`:
  späte Klassen-/Geometriefehler erhalten CPU-/GPU-Welt und Revision; Retry funktioniert.
- `test/outshine/include/Outshine/GeneratorProductsComposeAtomically.cpp`: echte
  `SDL_SubmitGPUCommandBuffer`-Ablehnungen bei B und spätem A erhalten jeweils das zuvor
  vollständig gerenderte Weltbild; beide unmittelbaren Retries publizieren genau einmal. Der
  Test benutzt nur die öffentliche Engine-API und dieselbe Submit-Grenze wie die Generatorprüfung.
  Ein positionaler Oszillator hinter A misst dabei den echten Occlusion-Snapshot: Ablehnung hält
  Pegel und Pixel von A beziehungsweise B; erfolgreicher Commit wechselt den Snapshot atomar.
- `Live/GroundClassificationBelongsToItsWorld.cpp`: Klassifikationspuffer gehören dem
  richtigen Owner; verworfene Kandidaten erhalten alte Inhalte.
- `Live/GroundResourcesSurviveWorldPublication.cpp`: Höhenhalter über zwei Ersatzwelten.
  Piece-/Crown-Rebinding zusätzlich mit tatsächlicher Folgeoperation belegen.
- `Live/ReleasedResourcesDropCpuPayloads.cpp`: CPU-Nutzdaten werden freigegeben;
  Slotreuse stabilisiert Metadaten bei fester Spitzenbelegung. Piece-/Page-Kandidaten,
  stale Handles, GPU-Fehler, Retry und Generationsüberlauf sind separat geprüft.
  `HeightSheets/FailedPageReplacementKeepsPreviousPage.cpp` prüft Erhaltung beim
  GPU-Seitenfehler; alter Freigabe-vor-Upload-Pfad verletzt zwei Checks.
  GPU-Floatadressgrenzen sind analytisch geprüft. Absolutes Speicherbudget bleibt WI 2228.
- Tests liegen unter `test/outshine/src/engine/<Komponente>/`; öffentliche Übergänge
  unter `test/outshine/include/Outshine/`. Produktionsoperation aufrufen, nicht nachbauen.
  Der RuntimeScene-Piece-Test ersetzt Geometrie über `ReplacesGeometry`, nie über die direkte
  Kandidatenaufbauoperation auf einer veröffentlichten Szene.
- Ground-Transaktion: Graz ohne Vegetation unverändert, 0/921600 abweichende Pixel;
  Referenz `build/shots/reference/ground-world-transaction/`. Keine visuelle Qualitätsabnahme.
- Pro Schritt `make format`, betroffene `make suite SUITE=...`, `make lint`.
  Bildwirksame Änderungen zusätzlich über Client rendern, PNG öffnen und vergleichen.
