Type: bug
State: active
Parent: 2191
Depends:
Architecture: ready
Area: engine, render, test
Tags: geometry, ownership, state, gpu

# World replacements publish one coherent native revision

## Architekturvertrag

`Core::Live` besitzt native Weltinputs; `Render::WorldContent` besitzt daraus erzeugte
GPU-Produkte. `Surrounds` besitzt Streamingzustand, logisches Netz und Ressourcenhalter.
Ein vorbereiteter Nachfolger veröffentlicht diese Produkte gemeinsam auf dem Engine-Thread.
Provider-Anfragen und Vorbereitungscaches dürfen fortschreiten; veröffentlichte Geometrie,
Materialzuordnung, Lichtparameter, Netz, Audio-Occlusion und Revision bei Ablehnung nicht.
Keine Mutation des aktiven Owners mit anschließendem Snapshot-Rollback.

## Vorhandene Grundlage

`WorldCandidate.h` kapselt Prepare/Publish/Abandon für `Live` und Renderer. Bei Fehler
zerstört RAII ausschließlich den Kandidaten. Abgelehnte verschachtelte Vorbereitung darf
den äußeren Kandidaten nicht verwerfen. `GroundWorldCandidate.h` ergänzt Sheets,
Terrainpositionen/Indizes, Netz, Materialslots und Revision; Revision wird zuletzt gesetzt.
`Surrounds::BindLiveResources` bindet Pieces, Sheets und Crowns ohne Allokation neu.
Ground-Klassen-GPU-Puffer gehören zum WorldContent, ihre CPU-Inputs zu Live.
Piece/Page-Identität verwendet native generational Handles und wiederverwendbare Slots.
Kandidaten kopieren Identitäten/Freilisten, nur Live übersetzt GPU-Adressen. Native
GroundTile-Daten enthalten keine als Float kodierten Handles; der Upload prüft exakte
GPU-Adressdarstellung. HeightSheets bereitet den Seitenersatz vor der alten Freigabe vor.

## Nächste Schritte in Reihenfolge

Die öffentliche Geometrieersetzung hat bereits die richtige Grenze: sie baut die
Audio-Occlusion vor dem Kandidaten, `Live::ReplacesGeometry` bereitet alle fehlbaren
GPU-Produkte vor, und erst `PublishesPreparedWorld` tauscht den Owner. Danach sind
`Surrounds::BindLiveResources` und der Occlusion-Move nichtwerfende Übergaben. Ein
fehlgeschlagener Submit lässt daher Welt, Audio und Bild bei A; ein Retry darf B
publizieren. Das ist kein Blocker für Struktur-Bake-Shutdown oder -Budgetierung.

1. Die Bake-Übergabe ist jetzt eigentümerscharf: `GroundBuildProducts` besitzt
   `BuildingField` und `TilePieces` bis `GroundWorldCandidate::Publish`.
   `StructureBakes` erhält den Footprint-Owner ausdrücklich; vor der ersten
   Ground-Publikation arbeitet `State::Bakes` gegen diesen Kandidaten. Nichtwerfende
   Transfers veröffentlichen Footprints, Pieces, `Live` und GPU-Welt gemeinsam.
   Ein unanchored Field lehnt Bake-Aufnahme ab. `BakeRevisionRejectsChangedInputs`
   prüft den Revisionsvertrag. Ein eingecheckter vollständiger Floor-Contact-Pfad fehlt;
   er darf nicht als Nachweis behauptet werden. Noch offen: A→B→spätes-A über die öffentliche API,
   GPU-Submit-Fehler während Bake-Publikation und erneuter Bindungsnachweis nach
   Kandidatenwechsel.
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
  Gebäude, unvollständige Dreiecke werden abgelehnt; Dachfehler, Retry, verschachtelte
  Kandidaten und Rebinding geprüft. Fix 947d891d9; drei gezielte Suiten und Lint grün.
  Graz zweimal geöffnet: zweiter Lauf pixelgleich zum Ausgangsbild, zwischen Läufen
  68/921600 Pixel am linken Hang verschieden. Reproduzierbarkeitsbefund separat WI 2230.

- `test/outshine/src/engine/GroundWorldCandidate/LateFailurePreservesPublishedGround.cpp`:
  späte Klassen-/Geometriefehler erhalten CPU-/GPU-Welt und Revision; Retry funktioniert.
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
- Ground-Transaktion: Graz ohne Vegetation unverändert, 0/921600 abweichende Pixel;
  Referenz `build/shots/reference/ground-world-transaction/`. Keine visuelle Qualitätsabnahme.
- Pro Schritt `make format`, betroffene `make suite SUITE=...`, `make lint`.
  Bildwirksame Änderungen zusätzlich über Client rendern, PNG öffnen und vergleichen.
