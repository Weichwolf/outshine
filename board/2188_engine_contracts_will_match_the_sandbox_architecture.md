Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: public-api, engine, world, generators, render
Tags: extension, ownership, native-model

# The public engine supports built-in and external world producers

## Ergebnis und vorhandene Fähigkeit
Bibliotheksnutzer liefern eigene Provider, Generatoren und deklarative Szenarien über
nutzbare öffentliche Verträge. Eingebaute Produzenten verwenden exakt denselben Weg.
Providerregistrierung und native API bestehen. `generation/Generate.h::Request` trägt
Ort, Extent, Seed, Ground und Coarseness, aber keinen Entfernungs-/Projektions-/Fehlervertrag.
Private Generator-/Importsonderpfade und vollständige externe Runtime-Abnahme sind offen.

## Besitzer und nächste Lieferung
`include/` besitzt minimale native Verträge; Engine koordiniert, `world/data` adaptiert
Quellenformate, Generatoren bleiben eigene Bibliothek. Zuerst den öffentlichen Generator-Request um
Raumreferenz, Entfernung/Projektion und erlaubten Fehler ergänzen; das native Produkt
liefert Bounds/Fehler oder eine explizit unbekannte Schranke. Eingebaute/externe Erzeuger
und Aufrufer gemeinsam migrieren. Wetter erhält einen öffentlichen Orts-/UTC-Snapshot
mit Einheiten, Gültigkeitsfenster und Herkunft. Danach externe Produzenten bis zur Runtime ersetzen.
Greenfield-API verbessern und alle Aufrufer migrieren; Deklaration ist kein Runtime-Nachweis.

## Gemeinsame Engine-Verträge
- Ein natives Geometrie-/Material-/Instanzmodell für Importer und Generatoren. Adapter
  beenden Formattypen; Handles, Borrowed-Spans, Ressourcen und Kollisionsprodukte besitzen
  erklärte Lebensdauer/Kosten. Keine privaten builtin-Umwege oder Eingriffe in den Host.
- Weltpositionen Double, GPU kamera-relative Floats; rechtshändig Y-up, CCW, ENU,
  Einheitsnormalen. Geometrie, Licht, Schatten und Kontakt teilen Frame-Ursprung/Höhendatum.
- Generatoranforderungen gelten nicht nur für die Erde: Raumreferenz, Entfernung und
  Fehler/Budget explizit übergeben; keine versteckten OSM-/DEM-Annahmen für Bibliotheksnutzer.
- Source-/Producer-Version, Parameter/Seed und konsumierte Abhängigkeiten bestimmen
  Produktidentität. Fehler, Abbruch, Kosten und letzte Nutzung an Grenzen explizit führen.
- `reaches`-Tiers, Groundless- und glTF-Szenarien erhalten. Szenarien vollständig
  roundtrippen; Imports/Plugins liefern native Produzenten, keine Place-Sonderklasse.
- Frame-/Ressourcenwechsel und CPU/GPU-ABI verbindlich prüfen; keine Host-Allokator-
  Ersetzung, versteckten Globals oder synchronen Produceraufrufe im Renderer.

## Abnahme
Ein Bibliotheksnutzer ersetzt Provider und Generator, baut/zeichnet native Produkte
und erhält Fehler/Abbruch ohne private Includes oder Sondercode. Builtins gehen denselben
Weg. Öffentliche Header, Runtime und Dokumentation beschreiben denselben Vertrag.
