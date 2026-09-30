Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2280
Depends:
Area: world, data, engine, generators
Tags: osm, official-source, webcam

# Default worlds consume official original OSM, without a map-tile fallback

## Ergebnis und vorhandene Fähigkeit
Der Client lädt Original-Nodes, Ways und Relations der offiziellen OSM-API und
behält sämtliche Tags. Dieselbe Quelle liefert Gebäude, Verkehrsnetze und Wasser.
VersaTiles ist ausdrücklich nicht autorisiert. `fd80cb726` entfernt den Default und
verweigert reduzierte Kartenkacheln am Client-Eingang; die Places haben noch keine Originalquelle.
`OsmXmlReader`, `OsmSourceLoader`, native Verkehrsprodukte, `OsmBuildingFootprints`
und `OriginalStructureInput` sind vorhanden. Ihr vollständiger Client-Anschluss fehlt.
`5644a79c6` trennt den gemeinsamen Datenstand von den jeweiligen Antwort-Prüfsummen;
der Snapshot erhält alle Tags sowie die geprüften Pins der zusammengeführten Quelldateien.
`aa4db4193` beschafft deklarierte Regionen automatisch über den bestehenden Transport
und Rohdaten-Cache; ein eigener IO-Worker hält Compute frei. `1cab1f3fa` bindet
Vorladen und Aufnahme an vollständige Quellenprodukte, auch ohne Terrain.
Der echte Client lädt Flensburgs Kameraausschnitt von der offiziellen API und erneut
offline aus denselben Rohbytes. Automatische Place-Nachfrage, native Gebäude-/
Wasserpublikation und vollständige Sichtabdeckung bleiben offen. Das Place-Gate bleibt
rot; erhaltene Bilder und eine erfolgreiche Quellen-Diagnose ersetzen keine Stadt.

## Architektur und Implementierung
- Den unerlaubten Default und dessen impliziten Endpoint entfernen. Vorhandene
  Cache-Dateien und Regression-Bilder erhalten. Ohne erforderliche Originalquelle
  explizit fehlschlagen; keine leere Stadt als vollständige Welt veröffentlichen.
- `world/data` beschafft bounded Regionsdaten von `api.openstreetmap.org/api/0.6`.
  Abfrage: `map?bbox=west,south,east,north`; erforderliche Originalobjekte über
  `way/{id}/full` beziehungsweise `relation/{id}/full` ergänzen und Hülle erneut prüfen.
  Räumliche Nachfrage, Quellenabdeckung, Revision und Byte-Pins sind explizit.
  IO/Parse laufen in begrenzten Jobs; Cache enthält ausschließlich Netzantworten.
  Gleichartige Anfragen teilen Quelle/Resultat. Kein synchrones IO beim Zeichnen.
  `kind=osm` deklariert entweder eine lokale Originaldatei oder exakt den offiziellen
  API-Basisendpoint samt Bounds. Ein IO-Worker beschafft begrenzte Originalantworten
  über bestehende Transport-/Cache-Verträge; Compute-Jobs prüfen Pins, parsen und mergen.
  Abbruch und Ablauf der gemeinsamen Anfragefrist erhalten den bisherigen Snapshot.
- Bereitschaft prüft angeforderte Produkte unabhängig von Terrain oder Renderziel.
  Vorladen wartet auf deren tatsächliche Worker; terminale Quellenfehler verhindern
  Aufnahme und Publikation. Szenen ohne angeforderte Weltprodukte sind sofort bereit.
- Ein gemeinsamer gepinnter `OsmSourceSnapshot` hält typisierte IDs, Nodes,
  Way-Referenzen, Relationsrollen und alle Tags. Konsumierte Produkte fordern
  ihre transitive Referenzhülle an; fremde unvollständige Fernrelationen bleiben
  erhalten und sperren kein unabhängiges vollständiges Produkt.
- Dataset-Revision und Datei-Prüfsumme getrennt halten: benachbarte unterschiedliche
  Originalantworten müssen unter einer Region konsistent zusammengeführt werden
  können, ohne ihre jeweiligen Byte-Pins zu verlieren. Konflikte erhalten den Altstand.
- Native Gebäudejobs verwenden `OriginalStructureInput`, bestehende DEM-Zertifikate,
  Zell-/LOD-Planung und atomare Publikation. Kein Umweg über reduzierte MVT-Properties.
  Sonderbauwerksklasse, Parts, Höfe, Höhe/Mindesthöhe und Dachangaben erreichen
  Konstruktion und Material. Unbekannte Tags bleiben am Quellobjekt verfügbar;
  fehlende, abgeleitete und widersprüchliche Werte bleiben unterscheidbar.
- Native Verkehrsprodukte erhalten erreichte Profile/Kreuzungen und Terrainanschlüsse.
  Gerichtete Original-Küsten schließen Flensburgs Meeresfläche in vollständig
  deklarierten Regionen. Binnenkörper behalten IDs, Inseln und Pegelherkunft (2145).
  Keine Gebäude im Wasser durch verlorene Semantik oder eine pauschale Wasserplatte.
- Originalquellen ersetzen den bisherigen Laufzeitpfad, nicht ein zusätzliches Overlay.
  Weltinhalte bleiben rundum resident. Umfang oder fehlende Daten verkürzen die
  konfigurierte Sicht nicht; unvollständige Nachfrage bleibt ausdrücklich unvollständig.

## Abnahme
Ein echter Client-Place zeigt eine vollständige native Quelle bis ins Bild. Netzwerk-
und Cache-Provenienz nennen offizielle Quelle, Revision und Pins; kein VersaTiles-Zugriff.
Flensburg zeigt Original-Küste statt fehlender Förde und Original-Bauwerksklassen
statt einer aus Kachelhöhe geratenen Kirche. Wien/Feldkirch erhalten ihre gültigen
Original-Höhenintervalle. Straßenfortschritt erhalten, alle Places öffnen und vergleichen.
Die vollständige 240-km-/720p60-Abnahme bleibt verbindlich; ein Quellloader oder
Parser-Test allein schließt diese Lieferung nicht. Format, fokussierte Suiten,
Place-Gate und vollständiger Lint einschließlich clang-tidy/API.
