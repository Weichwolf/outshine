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

1. Bake-Publikation vervollständigen: `StructureBakes::{NextLanding,CommitsLanding}`,
   `StructureTilePublication.h`, `Advancing.cpp` und
   `src/world/ground/BuildingField.{h,cpp}` gemeinsam prüfen.
   Aktuell werden Footprints erst nach GPU-Publikation in `CommitsLanding` übernommen.
   Vor Veröffentlichung Footprint-Nachfolger inklusive Kapazität vorbereiten; danach nur
   nichtwerfende Transfers, Rebinding, Queue-Verbrauch und Revisionswechsel. Job-Output
   bleibt bis Commit/Abbruch im Queue-Owner; Landing ist nur geliehen, nie über Commit halten.
   Uploadfehler konsumiert weder Job noch Footprints. Gültiger Retry konsumiert genau einmal.
   Nicht mit einem zweiten Test-Publikationspfad oder bloßen Zählerkopien nachweisen.
2. Stale Ergebnisse abweisen: Bake-/Ground-Anfragen tragen die benötigte Datenrevision
   einschließlich Projektion und Quellidentität. Vor Publikation mit aktuellem Auftrag
   vergleichen. Veralteten fertigen Job freigeben, ohne aktuelle Welt/Revision zu verändern;
   gleiche Tile-ID allein ist keine Identität. A→B→spätes A als deterministischen Test bauen.
   `d557454a4` bindet Structure-Bakes an OSM-Generation, Footprint-Revision, Fokalmaßstab
   und Tile-Spannweite; ein unpassender fertiger Job gibt sein Watermark frei und wird recycelt.
   `StructureBakes/BakeRevisionRejectsChangedInputs.cpp` prüft den vollständigen
   Revisionsvergleich gegen Fokalmaßstab und Footprint-Revision. Der A→B→späte-A-Orakel
   durch den öffentlichen Pfad bleibt offen.
   Fixture: ein lokaler Provider liefert einen festen OSM-Tile mit Gebäudegrundriss und eine
   feste DEM-Seite; A wird vollständig gerendert, B ändert Quelle oder Projektion, dann darf
   ein verspätetes A weder Bild noch Routing, Materialmapping, Albedo oder Revision verändern.
   Beide SDL-Submit-Hooks werden jeweils nach vollständigem Kandidatenaufbau verweigert;
   der gültige B-Retry muss genau einmal landen.
3. Öffentlichen Gesamtpfad testen: kleiner deterministischer OSM-/DEM-Provider, Engine-API,
   zunächst gültige Welt A, dann B mit spätem Klassen-/Geometrie-Submitfehler.
   Materialmapping, Albedo, tatsächliches Routingnetz, GPU-Readback und Bild von A erhalten;
   Retry liefert B. Beide SDL-Submitfunktionen im bestehenden Fault-Injection-Stil erfassen.
4. `Restands`, surface-only redeclare, Kamera-/Animationsersatz und öffentliche
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
