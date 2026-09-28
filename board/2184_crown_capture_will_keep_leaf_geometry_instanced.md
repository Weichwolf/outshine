Type: defect
State: active
Architecture: ready
Priority: P1
Area: generators, render, engine
Parent: 2111
Depends:

# Instanced crown capture has a measured bounded working set

## Befund und vorhandene Fähigkeit

Historischer erster Koerbersee-Bake: 31 Arten, ash zuerst; expandierte Blattvalidierung
mit 8.2 GB physical footprint und 15.5 GB Peak. Das überschreitet das 8-GB-Geräteziel
bereits im Artefaktgenerator. Diese Zahlen bezeichnen den damaligen Pfad, nicht heute.

Der aktuelle Pfad ist bereits instanziert: engine/streaming/ImpostorPreparation.cpp
ruft TreePrototype::InstancedGeometryAt(0), übergibt Rinde mit einer Instanz und das
geteilte Blattmesh mit Placements an Render::ImpostorBaker. Nicht erneut implementieren.
Render akzeptiert native ImpostorCapture; Bounds stammen aus denselben Blatttransforms.
NativeGeometryBakesWithoutEngine und PreparationRetainsGeneratorSurfaces sind vorhandene
Prüfungen, keine aktuelle Großarten-/Speicherabnahme. Alte „63 Tidy-Befunde“ sind historisch.

## Ausführbarer Rest

Owner: flora/TreePrototype, engine/streaming/ImpostorPreparation, render/impostor/ImpostorBaker.
Vor Änderung echte ash-/Koerbersee-Vorbereitung aus demselben Species-/Shape-Input messen:
CPU-Kapazitäten, GPU requested bytes, Prozess-Peak und Phasenzeiten getrennt, Logs in Temp.
Kein Wechsel auf eine leichtere Art oder kleineren Rank, um den Fehler verschwinden zu lassen.

Falls der Peak weiterhin untragbar ist, erste expandierende/allokierende Phase lokalisieren
und vollständig durch gebündelte Instanzen/Readback mit explizitem Cap ersetzen.
Instanz- und Atlasgrenzen vor Allokation prüfen; Überschreitung liefert erwarteten Fehler
und lässt das bestehende Cacheartefakt unverändert. Ein Bake gleichzeitig, Abbruch und
Generationsprüfung vor Cache-Publikation; renderer-eigene Capture-Ressourcen korrekt retire.
Native Nahgeometrie 2111 muss nicht auf den vollständigen Atlas-Messauftrag warten.

## Abnahme

- [ ] Blattpositionen/Normalen/Materialien gegen unabhängig expandierte kleine Referenz;
      Floatgrenzen herleiten. Falsche Blattorientierung und expandierter Großpfad scheitern.
- [ ] Vorhandene unabhängige 3x3-Packing-Prüfung erhält Alpha, Normalraum und MR-Kanäle;
      vertauschte MR-Kanäle verletzen das Oracle. Randfüllung ist keine neue Geometrie.
- [ ] Echte ash-/Koerbersee-Vorbereitung endet innerhalb deklarierter Arbeits-/Speichercaps;
      misslungener/abgebrochener Bake publiziert nichts und ein Retry ist möglich.
- [ ] Atlas- und Welt-PNGs selbst öffnen; Warmrenderkosten getrennt von Artefaktkosten.
- [ ] make format; fokussierte TreePrototype/ImpostorBaker/ImpostorPreparation/VegetationStreaming
      Suites und vollständiges make lint. Device-Peak bleibt unabhängig nachzuweisen.
