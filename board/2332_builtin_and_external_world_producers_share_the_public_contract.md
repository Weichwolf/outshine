Type: defect
State: active
Architecture: ready
Priority: P0
Parent: 2131
Depends:
Area: public-api, world, engine
Tags: providers, library, architecture

# Built-in and external providers share the public runtime contract

## Ergebnis und Iststand
Ein Bibliotheksnutzer kann eigene Provider registrieren und ihre Quellen durch
denselben Runtime-Pfad wie eingebaute Implementierungen verwenden.
Der Client wählt offizielle OSM-Daten, GLO-30 und Open-Meteo; diese Quellenpolitik
schließt externe Implementierungen anderer Bibliotheksnutzer nicht aus.

`Engine::registerProvider`, `world/Provider.h` und die öffentlichen Source-/Transport-
Wertverträge sind integriert. Eingebaute und registrierte Factories erzeugen Quellen
für denselben SourceSet-Pfad; auch Original-OSM verwendet diese Registrierung.
Ein externer Terrainprovider liefert nachweislich Höhensamples und gerenderte Geometrie.
Scheduling und Cache bleiben intern. WI 2211 belegt nur die frühere Quellenauswahl.
Offen: Die Runtime ordnet Original-OSM noch über den Factory-Namen `osm` zu;
bekannte Namen unterliegen teilweise quellenspezifischer Konfigurationsvalidierung.
Eigene Quellennamen müssen dieselben nativen Produkte erreichen können.
Die zusätzliche Generatorlücke besitzt WI 2333, der GLO-30-Adapter WI 2331.

## Entschiedene öffentliche Grenze
`world/data/Source.h` und seine Wert-/Transporttypen sind öffentliche Header.
`world/Provider.h` definiert einen geliehenen
Provider mit `kind()` und `make(SourceProvider, shippedRoot)`: Konfiguration ohne IO
liefert eine eigene Source oder einen Fehler. Engine registriert Provider nach Name;
SourceSet besitzt daraus erzeugte Sources. Eingebaute Factory-Implementierungen
nutzen dieselbe Registry. Begin/Collect laufen auf IO-Arbeitern; Source-Cancellation
beendet auch provider-eigene Arbeit. Namen werden kopiert; geliehene Provider leben
bis zur Engine-Zerstörung. Bestehende Kind-/Formatwerte bleiben Wertverträge.

## Zuständigkeiten und Umsetzung
- Provider besitzen Beschaffung und Interpretation ihrer Quelle. Engine besitzt
  Scheduling, Rohdaten-Cache, begrenzte Residency und atomare Publikation. HTTP und
  Dateiformate enden am Adapter; Generatoren erhalten native Daten samt Herkunft.
- Eingebaute Provider werden über denselben öffentlichen Vertrag registriert wie
  externe. Konfigurationsvalidierung darf bekannte Client-Quellen beschränken,
  nicht die Erweiterung der Bibliothek. Keine Factory nur mit festem Kind-Switch.
- Factory-Name bezeichnet die Implementierung; die native Datenart bestimmt den
  Verbraucher. Diese Zuordnung explizit im öffentlichen Providervertrag festlegen;
  Source-Deklaration dagegen prüfen. Bibliotheksvalidierung prüft gemeinsame
  Invarianten, die Factory ihre Konfiguration, der Client seine Quellenpolitik.
- Die öffentliche API ist Greenfield: Funktionen und Typen nach dem benötigten
  Engine-Vertrag verbessern oder ergänzen, betroffene Aufrufer vollständig migrieren.
  Verträge dokumentieren Besitz, Kosten und Lebensdauer. Renderer- und Formattypen bleiben
  intern; ein fremdes Projekt benötigt weder `src/` noch interne Build-Includes.
- Alte Quellenpfade erst mit ihrem funktionierenden Ersatz entfernen; diese Migration
  besitzt WI 2331. Straßen-/Terrainalgorithmen und historische Orakel erhalten.

## Abnahme
Ein separates Client-Beispiel mit ausschließlich öffentlichen Includes liefert
einen eigenen Provider bis ins gerenderte Bild. Ein eigener Factory-Name erreicht
denselben nativen Verbraucher wie die eingebaute Implementierung.
Quellenidentität, Fehler und
Invalidierung bleiben sichtbar; fehlende Produkte werden keine leere fertige Welt.
Place-Gate und Straßenbilder bleiben erhalten. Keine bloße Registrierung als
Integrationsnachweis. Nativer Weltgenerator-Ausbau bleibt unabhängig in 2333 offen.
