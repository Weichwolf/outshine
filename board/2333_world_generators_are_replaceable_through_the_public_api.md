Type: defect
State: open
Architecture: planned
Priority: P1
Parent: 2101
Depends:
Area: public-api, generators, engine
Tags: library, buildings, roads, architecture

# World generators are replaceable through the public API

## Ergebnis und belegte Lücke
Ein Bibliotheksnutzer ersetzt Gebäude-/Straßen-/Wassergeneratoren über öffentliche
Verträge; die Engine integriert Bedarf, Produkte und Fehler wie bei eingebauten
Implementierungen. Eine eigene Demo-Geometrie allein erfüllt diesen Vertrag nicht.

`Engine::registerGenerator` und `generation/Generate.h` funktionieren für eigene
Geometrieproduzenten. Weltplatzierung nutzt zusätzlich private `Making`, `Yield`
und `GeneratorSet`; native Gebäude laufen über `StructureBuildQueue`/`StructureBake`.
Das belegt keine Austauschbarkeit der eigentlichen Weltgeneratoren. WI 2101 bleibt
für native Konstruktion zuständig, 2332 für Provider; kein zweiter Runtime-Pfad.

## Entscheidung und Umsetzung
- Vor Umbau den minimalen öffentlichen Vertrag für geodetischen Bedarf, native
  Features samt IDs/Tags/Herkunft, Entfernung, Projektionsfehler und Produkte festlegen.
  Bestehende `generation/Generate.h`, Registry und native Produkte weiterentwickeln.
- Generator besitzt Konstruktion; Engine besitzt Scheduling, Residency, Upload und
  atomare Publikation. Logisches Straßennetz bleibt unabhängig von Render-LOD.
- Eingebaute Generatoren benutzen dieselben öffentlichen Verträge und Registrierung
  wie externe. Private alternative Erzeugung und unbenutzte Wrapper entfernen.
- Besitz, Threads, Abbruch, Kosten und Produktlebensdauer dokumentieren. Fremde
  Projekte benötigen keine internen Includes oder Renderer-/Formatkenntnisse.
- CPU-Zertifikate und bestehende Terrain-Deformation erhalten. Kein engerer LOD-Fehler
  ohne Runtime-Nachweis. Vegetation nutzt später denselben Bedarf-/Produktvertrag.

## Abnahme
Ein Client nur mit öffentlichen Includes ersetzt einen echten Weltgenerator und
zeigt dessen Produkt im Bild. Entfernung/Fehler erreichen ihn; Quellenänderungen
invalidieren abhängige Produkte. Fehler erhalten die letzte vollständige Publikation.
Straßenqualität, vollständige Places und Framebudget bleiben verbindlich.
