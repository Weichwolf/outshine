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
Automatische API-Beschaffung und native Gebäude-/Wasserpublikation bleiben offen.
`5fa0fcbf3` besteht fokussierte Quellenprüfungen und den vollständigen Lint samt
clang-tidy/API. Das Place-Gate bleibt rot: Originaldaten sind nicht angebunden;
alte Bilder bleiben erhalten, fehlende Bilder werden nicht als Erfolg gewertet.

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
