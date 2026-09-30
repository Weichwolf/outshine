Type: defect
State: active
Architecture: ready
Priority: P0
Parent: 2131
Depends:
Area: public-api, world, engine
Tags: providers, library, architecture

# Built-in and external providers share the public runtime contract

## Ergebnis und belegte Lücke
Ein Bibliotheksnutzer kann eigene Provider registrieren und ihre Quellen durch
denselben Runtime-Pfad wie eingebaute Implementierungen verwenden.
Der Client wählt offizielle OSM-Daten, GLO-30 und Open-Meteo; diese Quellenpolitik
schließt externe Implementierungen anderer Bibliotheksnutzer nicht aus.

`include/world/SourceProvider.h` beschreibt Konfiguration, keine Implementierung.
`world/data/Source`, `Transport`, `SourceSet` und `RegisterDeclared` sind privat;
die Factory kennt nur fest eingebaute Klassen. WI 2211 belegt Auswahl, keine Erweiterung.
Die zusätzliche Generatorlücke besitzt WI 2333.

## Entschiedene öffentliche Grenze
`world/data/Source.h` und seine Wert-/Transporttypen werden öffentliche Header;
Scheduling und Cache bleiben intern. `world/Provider.h` definiert einen geliehenen
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
- Die öffentliche API ist Greenfield: Funktionen und Typen nach dem benötigten
  Engine-Vertrag verbessern oder ergänzen, betroffene Aufrufer vollständig migrieren.
  Verträge dokumentieren Besitz, Kosten und Lebensdauer. Renderer- und Formattypen bleiben
  intern; ein fremdes Projekt benötigt weder `src/` noch interne Build-Includes.
- Terrarium- und reduzierte OSM-Provider nach Ersatz entfernen. Weiterverwendete
  Straßen-/Terrainalgorithmen und unabhängige historische Orakel erhalten;
  unbenutzte Wrapper, Fallbacks und Registrierungen verschwinden.

## Abnahme
Ein separates Client-Beispiel mit ausschließlich öffentlichen Includes liefert
einen eigenen Provider bis ins gerenderte Bild. Eingebaute GLO-/OSM-
Implementierungen laufen durch denselben Vertrag. Quellenidentität, Fehler und
Invalidierung bleiben sichtbar; fehlende Produkte werden keine leere fertige Welt.
Place-Gate und Straßenbilder bleiben erhalten. Keine bloße Registrierung als
Integrationsnachweis. Nativer Weltgenerator-Ausbau bleibt unabhängig in 2333 offen.
