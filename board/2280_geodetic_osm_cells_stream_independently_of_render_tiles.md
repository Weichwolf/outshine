Type: feature
State: active
Architecture: ready
Parent: 2173
Depends:
Priority: P0
Area: world, data, navigation, engine, streaming
Tags: osm, worldwide, source-cells, residency

# Original OSM supplies coherent roads and buildings independently of render tiles

## Ergebnis und Iststand
Originalobjekte liefern native Verkehrs- und Gebäudeprodukte bis ins Client-Bild.
Quellenresidenz bleibt unabhängig von Render-LOD, Frustum und Kacheleviktion.
Vorhanden: regionale Originalbeschaffung, gemeinsame Snapshots und Transportprodukte;
native Gebäude mit gepinnten Koordinaten, Terrain-Zertifikaten und Stempeln im Client.
Der gemeinsame Compute-Worker plant DEM-Felder aus Gebäudegrundrissen statt Quellen-Bboxes;
unveränderte Nachfrage verwendet den vorbereiteten RAM-Bestand ohne erneute Geometriesuche.
Öffentliche `GeoCellId`-Adressen und der registrierte Original-API-Katalog liefern
begrenzte Zellantworten mit getrennten Raw-Cachekeys; alte Quelladressen bleiben stabil.
`ReadOsmApiCells` beschafft begrenzte Zelljobs; `ParseCell` hält Herkunft und Elemente
je Zelle getrennt. Zellantworten dürfen nicht zum regionalen Snapshot verschmelzen.
`OsmSourceLoader::RequestCells` hält vollständige Zellmengen resident und übernimmt auch
fertige unveröffentlichte Snapshots; bestehende IO-/Compute-Phasen publizieren Ersatz atomar.
Regionale Diagnosen sind sichtbar; Kameranachfrage, native Zellprodukte und vollständige
Places fehlen. Der regionale Snapshot ersetzt keine weltweite Residenz.
Weltweite Zellnachfrage bleibt Teil dieser Lieferung; größere Chunk-Limits ersetzen sie nicht.

## Weltbedarf von Boden bis Orbit
- Am Boden bleibt der vollständige 240-km-Umkreis um die Position resident; Blickrichtung
  begrenzt nur Zeichenarbeit. Höhe über Gelände erweitert die Abdeckung bis zum
  konservativen Ellipsoid-Horizont einschließlich sichtbarer Gelände-/Bauwerkshöhen.
- Aus dem Orbit steht zuerst eine vollständige grobe Erdansicht bereit. Geodätische
  Hierarchie und Bildschirmfehler fordern anschließend auflösbare Originaldetails an.
  Globale Vollauflösung ist keine Ladebedingung; unverfügbare Grobdaten bleiben offen.
- Globale Grobdarstellung und lokale Detailprodukte stammen aus denselben erlaubten
  Quellen. Erdansicht, hohe Aussicht und Boden-Place messen eigene Preload-/Bytebudgets.
- Boden, Flug in mehreren Kilometern Höhe und Orbit verwenden dieselbe Hierarchie.
  Übergänge bleiben geschlossen und stetig; sichtbarer Fehler steuert Verfeinerung.
  Subpixel-Relief entfällt zugunsten des Ellipsoids, ohne sichtbare Küsten zu verlieren.
- Nahtloser Zoom reicht vom Planeten bis zum einzelnen Grashalm; dieselbe Welt,
  kamera-relative Präzision und konsistente Eltern-/Kindabdeckung tragen alle Maßstäbe.
  Vegetation folgt zuletzt; Architektur und Generatorvertrag müssen sie ermöglichen.
Fahrabnahme und weltweiter Router blockieren den visuellen Meilenstein nicht.

## Besitzer und Quellenvertrag
- `world/data` besitzt unveränderliche Quellregionen und typisierte OSM-IDs.
  Der öffentliche `GeoCellId`-Vertrag gilt unabhängig von Render-/DEM-Adressierung.
- `OsmApiSource` adressiert (Dataset, Revision, GeoCellId) über den öffentlichen
  Provider-/Source-Vertrag. Ein offizieller API-Provider ohne Bounds bezeichnet den
  Katalog; jede Zelladresse bestimmt ihre Bbox und ihren eigenen Rohdaten-Cachekey.
  Es gibt keinen zusätzlichen Netzwerk-Katalog und kein generiertes Manifest.
  API-Zellen beginnen bei Level 9: 360 * 180 / 4^9 = 0.2471923828125 Quadratgrad
  unterschreitet das 0.25-Quadratgrad-Limit. Dichtere Antworten verlangen Verfeinerung.
  Globale Payload-Pins sind für Kataloge ungültig; Zellantworten behalten eigene Pins.
  Begrenzte Originaldateien/API-Regionen bleiben explizite Eingaben. IO/Parse laufen
  außerhalb der Simulation; die Kamera fordert Quellenzellen unabhängig vom Render-LOD an.
- Gleiche überlappende Objekte deduplizieren nach typisierter ID; Konflikte verwerfen
  den Ersatz. Geteilte Nodes verbinden Netze; gleiche Koordinaten beweisen keine Identität.
  Konsumierte Produkte verlangen vollständige Referenzhüllen und fehlende Nachbarzellen.
  Fremde unvollständige Fernrelationen bleiben erhalten und blockieren keine Gebäude.
  Vollständig deklarierte lokale Chunk-Sets behalten ihre strikte Prüfung.
- `world/navigation` leitet Verkehrsnetze ab; Routen pinnen benötigte Graphzellen.
  `world/ground` liefert native Gebäudeinputs an bestehende Generatoren.
  `engine/streaming` besitzt Nachfrage, begrenzte Jobs, Abbruch und Retention.
  Kein globaler Objektmerge und kein Graph aus Straßen-Rendergeometrie.

## Konkreter Runtime-Anschluss
- `OsmSourceLoader` besitzt Zellzustände nach Dataset, Revision und Adresse und nutzt
  seine bestehenden IO-/Compute-Phasen. Der regionale `Current()`-Snapshot bleibt
  expliziten Diagnosen vorbehalten; keine zusätzliche IO-/Gebäudequeue.
  Verkehrs- und Gebäudejobs pinnen ihre benötigten Zell-Snapshots.
  `GroundInputsReady` wartet auf verlangte Produkte, nicht pauschal auf alle Graphen.
- `OriginalStructureInput` liefert `RawTile` an `StructureBuildQueue`.
  Terrain-Anfragen folgen dessen Geometrie statt reduzierten `OsmField`-Features.
  Quellenidentität, DEM-Zertifikat und Geometriebesitzer qualifizieren Jobs und Publikation.
  Keine zweite Gebäudequeue, kein MVT-Zwischenformat, kein persistenter Produkt-Cache.
  Ein Kandidat hält getrennte Gebäudeprodukte je Original-Snapshot. Compute prüft
  gemeinsame Dataset-/Revisionsidentität, widersprüchliche Duplikate und Relation-Member;
  gleiche typisierte IDs erhalten genau einen Besitzer. Fehler erhalten den Altstand.
- `BuildingField::AcceptedInput` pinnt Geometrie und Quelle je Produkt; Indizes gelten
  nur in dessen Points/Rings. `BuildingStampJob` und `Laying` verwenden diese Besitzer.
  OSM verformt DEM über dieselben Terrain-Stempel wie vorhandene Straßen und Gebäude.
  Regionale Gebäudekandidaten pinnen die Originalquelle direkt. Ihre Publikationsrevision
  erneuert Kandidaten unabhängig vom nur für deklarierte Routen benötigten Verkehrsgraphen.
  Ersatz publiziert atomar; Fehler erhalten gültigen Altstand und Straßenqualität.
- Original-IDs erschließen Tags für `StructurePlan`/`BuildingMesh` gemäß 2173;
  Klassen, Dächer, Material und Parts bleiben erhalten. Schornsteine sind keine Wohnhäuser.
  `OsmBuildingHeights` normalisiert Höhen/Geschosse an der Grenze; fehlende, gültige und
  fehlerhafte Angaben bleiben getrennt. Kein Sentinel wird zur gemessenen Höhe.
- Außenring, Höfe und erhöhte Parts bleiben bei jedem LOD erhalten. Kein Stamp, Sockel
  oder Pflaster unter erhöhten Parts; Zusammenfassung füllt weder Hof noch Durchfahrt.
  Earcut trianguliert perforierte Dächer/Böden vor Dachfaltung; Massing bleibt ungeteilt.
  Mesh und Stempel teilen Punkte und Revision; Innenringe brauchen eindeutige Außenbesitzer.

## Nachfrage und Speicher
Alle Azimute erhalten entfernungsangemessene residente Produkte bis zur konfigurierten
Sichtweite. Frustum begrenzt nur Zeichnen. Bewegung fragt neue Regionen mit Hysterese an;
Überlast verschiebt Arbeit ohne Frame-Blockade. Kohärente Quellenstände publizieren atomar.
SSD hält nur Rohbytes; RAM hält Indizes, Staging und CPU-Produkte. GPU-Produkte sind
vor Drehung zeichenbereit. Budgets nach AGENTS; aktive Regionen werden nicht ständig verdrängt.
Der Lader verlangt Zell-/Snapshot-Grenzen und zählt auch weiter gepinnte Altstände.
Die Snapshot-Charge zählt Strukturen und reservierte Kapazitäten konservativ; sie ersetzt
keine Messung von RSS, Allocator-Reserve oder transienten Parse-/Publikationsspitzen.

## Widerlegbare Abnahme
Zwei benachbarte Originalregionen liefern im Client Gebäude und verbundene Straßen.
Geteilte Nodes verbinden über die Grenze; Quellenkonflikte erhalten den Altstand.
Ein getaggter Schornstein hat keine Wohnfenster, ein Hof bleibt offen, ein erhöhtes Part
behält Durchfahrt und unverformten Boden. Bodenberührende Gebäude stempeln das Terrain.
Originalprodukte ersetzen Kachelprodukte ohne Dopplung. IDs, Tags und Rollen bleiben verfügbar.
Volle Drehung und Rückkehr über die Grenze zeigen vollständige unveränderte Welt ohne Frame-IO.
Kalt/warm: Quellenbytes, komplettes Preload, Residenz und p99 messen. Parser allein genügt nicht.
Format, fokussierte Quellen-/Generator-/Terrainprüfungen, vollständiger Lint und Place-Bilder
nach AGENTS; regionale Diagnosen ersetzen kein echtes Place-Gate. Quellenanschluss: 2330.
