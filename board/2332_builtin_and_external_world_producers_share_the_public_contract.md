Type: defect
State: active
Architecture: planned
Priority: P0
Parent: 2131
Depends:
Area: public-api, world, generators, engine
Tags: providers, generators, library, architecture

# Built-in and external world producers share the public contract

## Ergebnis und belegte Lücke
Ein Bibliotheksnutzer kann eigene Provider und Generatoren registrieren und ihre
Produkte durch denselben Runtime-Pfad wie eingebaute Implementierungen verwenden.
Der Client wählt offizielle OSM-Daten, GLO-30 und Open-Meteo; diese Quellenpolitik
schließt externe Implementierungen anderer Bibliotheksnutzer nicht aus.

`include/world/SourceProvider.h` beschreibt Konfiguration, keine Implementierung.
`world/data/Source`, `Transport`, `SourceSet` und `RegisterDeclared` sind privat;
die Factory kennt nur fest eingebaute Klassen. WI 2211 belegt Auswahl, keine Erweiterung.
`Engine::registerGenerator` und `generation/Generate.h` funktionieren für eigene
Geometrieproduzenten. Die Weltplatzierung nutzt zusätzlich private `Making`, `Yield`
und `GeneratorSet`; native Gebäude laufen über `StructureBuildQueue`/`StructureBake`.
Diese Pfade beweisen nicht die Austauschbarkeit der eigentlichen Weltgeneratoren.

## Zuständigkeiten und Umsetzung
- Vor weiterer Providerintegration den vorhandenen Vertrag prüfen und die minimale
  öffentliche Grenze festlegen: Identität, räumlicher Bedarf, Quelldaten/Produkte,
  Fehler, Kosten, Cancellation, Threadzuständigkeit und Besitz. Vorhandene Typen und
  Registrierungen weiterverwenden; keine zweite konkurrierende API erfinden.
- Provider besitzen Beschaffung und Interpretation ihrer Quelle. Engine besitzt
  Scheduling, Rohdaten-Cache, begrenzte Residency und atomare Publikation. HTTP und
  Dateiformate enden am Adapter; Generatoren erhalten native Daten samt Herkunft.
- Eingebaute Provider werden über denselben öffentlichen Vertrag registriert wie
  externe. Konfigurationsvalidierung darf bekannte Client-Quellen beschränken,
  nicht die Erweiterung der Bibliothek. Keine Factory nur mit festem Kind-Switch.
- `generation/Generate.h`, Registry, Weltplatzierung und native Bake-Integration
  erhalten einen zusammenhängenden Erweiterungsvertrag für Bedarf und Produkte.
  Entfernung, Projektionsfehler und Herkunft erreichen tatsächlich die Produzenten;
  ihr Ergebnis erreicht Terrain/Renderer ohne private alternative Erzeugung.
- Die öffentliche API ist Greenfield: Funktionen und Typen nach dem benötigten
  Engine-Vertrag verbessern oder ergänzen, betroffene Aufrufer vollständig migrieren.
  Verträge dokumentieren Besitz, Kosten und Lebensdauer. Renderer- und Formattypen bleiben
  intern; ein fremdes Projekt benötigt weder `src/` noch interne Build-Includes.
- Terrarium- und reduzierte OSM-Provider nach Ersatz entfernen. Weiterverwendete
  Straßen-/Terrainalgorithmen und unabhängige historische Orakel erhalten;
  unbenutzte Wrapper, Fallbacks und Registrierungen verschwinden.

## Abnahme
Ein separates Client-Beispiel mit ausschließlich öffentlichen Includes liefert
einen eigenen Provider und Generator bis ins gerenderte Bild. Eingebaute GLO-/OSM-
Implementierungen laufen durch denselben Vertrag. Quellenidentität, Fehler und
Invalidierung bleiben sichtbar; fehlende Produkte werden keine leere fertige Welt.
Place-Gate und Straßenbilder bleiben erhalten. Keine bloße Registrierung als
Integrationsnachweis. Architektur erst nach konkreter Grenzentscheidung `ready`.
