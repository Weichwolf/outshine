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
VersaTiles und reduzierte Kartenkacheln sind am Client-Eingang ausgeschlossen.
Vorhanden: Originalreader, gemeinsame Snapshots mit getrennten Revisions-/Byte-Pins,
begrenzte API-/Cache-Beschaffung auf IO-Worker und produktbezogene Vorladebereitschaft.
Die Runtime lädt bis zu zwei unabhängige Originalregionen gleichzeitig; gemeinsame
Frist und Abbruch räumen laufende Tickets auf. Parsing bleibt auf dem Compute-Worker.
Der registrierte API-Katalog bedient öffentliche geodätische Zelladressen samt
separaten Rohdaten-Cachekeys. Automatische Kameranachfrage und Renderpublikation fehlen.
Begrenzte Zelljobs liefern getrennte geparste Snapshots; der einzelne Bestand wird
ohne zusätzliche Elementkopie übernommen. Zellantworten werden nicht global gemergt.
Der Runtime-Lader übernimmt unveränderte Zell-Snapshots, beschafft nur fehlende Zellen
und veröffentlicht vollständige Nachfrage atomar; Fehler erhalten den gültigen Altstand.
Explizite Zell-/Snapshot-Quoten zählen gepinnte Altstände weiter. Reale RAM-Spitzen sind offen.
Flensburgs Kameraausschnitt lädt über die offizielle API und offline aus denselben Rohbytes.
Native Gebäude erreichen die gemeinsame Queue, Terrain-Stempel und beleuchtete Client-Bilder.
Der regionale Gebäudejob pinnt Originaldaten direkt; ihre eigene Publikationsrevision
invalidiert Kandidaten ohne Verkehrsgraph. Nur deklarierte Routen fordern dessen Aufbau.
Terrainbedarf folgt den Grundrissen, nicht der Quellen-Bbox. Gemeinsamer Compute-Worker
und bestehende Queue halten die vorbereiteten Felder je Quelle und Terrain-Zoom im RAM.
Automatische Nachfrage, native Wasser-/Straßendarstellung und volle Quellenabdeckung fehlen.
Weltweite Abdeckung braucht residente Quellenzellen statt größere regionale Träger.
Das Place-Gate bleibt rot; analytisches Terrain und regionale Sicht sind nur Diagnosen.

## Architektur und Implementierung
- Den unerlaubten Default und dessen impliziten Endpoint entfernen. Vorhandene
  Cache-Dateien und Regression-Bilder erhalten. Ohne erforderliche Originalquelle
  explizit fehlschlagen; keine leere Stadt als vollständige Welt veröffentlichen.
- `world/data` beschafft bounded Regionsdaten von `api.openstreetmap.org/api/0.6`.
  Abfrage: `map?bbox=west,south,east,north`; erforderliche Originalobjekte über
  `way/{id}/full` beziehungsweise `relation/{id}/full` ergänzen und Hülle erneut prüfen.
  Nachfrage, Abdeckung, Revision und Byte-Pins sind explizit. Begrenzte IO-/Parse-Jobs
  teilen gleichartige Anfragen; Cache hält Netzantworten. Kein IO beim Zeichnen.
  `kind=osm` deklariert entweder eine lokale Originaldatei oder exakt den offiziellen
  API-Basisendpoint: mit Bounds als Region, ohne Bounds als Zellkatalog. Ein IO-Worker
  beschafft begrenzt parallele Antworten; Compute-Jobs prüfen Pins und parsen je Zelle.
  Abbruch/Frist erhalten den Altstand; Beschaffungszeit misst verstrichene Zeit.
- Bereitschaft prüft angeforderte Produkte unabhängig von Terrain oder Renderziel.
  Originalquellen und verlangte Verkehrsgraphen haben getrennte Bereitschaft und Revisionen.
  Vorladen wartet auf deren tatsächliche Worker; terminale Quellenfehler verhindern
  Aufnahme und Publikation. Szenen ohne angeforderte Weltprodukte sind sofort bereit.
- Je Quellenzelle hält ein gepinnter `OsmSourceSnapshot` typisierte IDs, Nodes,
  Way-Referenzen, Relationsrollen und alle Tags. Konsumierte Produkte fordern
  ihre transitive Referenzhülle an; fremde unvollständige Fernrelationen bleiben
  erhalten und sperren kein unabhängiges vollständiges Produkt.
- Dataset-Revision und Datei-Prüfsumme getrennt halten. Die Runtime hält Zellbestände
  unabhängig; gemeinsame typisierte Referenzen schließen konsumierte Produkte.
  Begrenzte Diagnosen dürfen Regionen zusammenführen. Konflikte erhalten den Altstand.
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

## Verbindliche Lieferreihenfolge
1. P0: Kamera/Sichtweite fordern native Originalzellen an (2280); Default-OSM
   und Copernicus-Adapter (2331) liefern echte Quellen statt Place-Sonderpfaden.
2. P0: Flensburg integriert native Straßen, Gebäude und Wasser (2145); bestehende
   Straßenprofile/Stempel erhalten. Klassen, Parts und Dächer anschließen (2173).
3. P1: Native Detailzellen, frühe Zusammenfassung und residente Rundum-LOD (2298/2312)
   erfüllen Preload/p99; keine kleinere Fehlerschranke allein aus CPU-Beweisen.
4. Alle acht Places aus denselben Verträgen im vollständigen Gate abnehmen;
   fehlende Abdeckung bleibt rot. Danach Materialien, Details, Licht und Wolken ausbauen.

## Abnahme
Ein echter Client-Place zeigt eine vollständige native Quelle bis ins Bild. Netzwerk-
und Cache-Provenienz nennen offizielle Quelle, Revision und Pins; kein VersaTiles-Zugriff.
Flensburg zeigt Original-Küste statt fehlender Förde und Original-Bauwerksklassen
statt einer aus Kachelhöhe geratenen Kirche. Wien/Feldkirch erhalten ihre gültigen
Original-Höhenintervalle. Straßenfortschritt erhalten, alle Places öffnen und vergleichen.
Die vollständige 240-km-/720p60-Abnahme bleibt verbindlich; ein Quellloader oder
Parser-Test allein schließt diese Lieferung nicht. Format, fokussierte Suiten,
Place-Gate und vollständiger Lint einschließlich clang-tidy/API.
