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
Öffentliche `GeoCellId`-Adressen und der registrierte Original-API-Katalog liefern
begrenzte Zellantworten mit getrennten Raw-Cachekeys; alte Quelladressen bleiben stabil.
Regionale Diagnosen sind sichtbar; automatische Nachfrage, residente Zellprodukte
und vollständige Places fehlen. Der regionale Snapshot ersetzt keine weltweite Residenz.
Weltweite Zellnachfrage bleibt Teil dieser Lieferung; größere Chunk-Limits ersetzen sie nicht.
Fahrabnahme und weltweiter Router blockieren den visuellen Meilenstein nicht.

## Besitzer und Quellenvertrag
- `world/data` besitzt unveränderliche Quellregionen und typisierte OSM-IDs.
  `GeoCellId(level,x,y)` teilt Länge [-180,180) und Breite [-90,90], level <= 24,
  Achsen < 2^level; x umläuft, y nicht. Bounds sind halboffen außer am Nordpol.
  Diese Quellenzellen sind weder Metergrid noch Mercator-Render-/DEM-Kacheln.
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
- Engine und `OsmTransportLoader::RequestSource` teilen den gepinnten Snapshot.
  `GroundInputsReady` wartet auf verlangte Produkte, nicht pauschal auf alle Graphen.
- `OriginalStructureInput` liefert `RawTile` an `StructureBuildQueue`.
  Terrain-Anfragen folgen dessen Geometrie statt reduzierten `OsmField`-Features.
  Quellenidentität, DEM-Zertifikat und Geometriebesitzer qualifizieren Jobs und Publikation.
  Keine zweite Gebäudequeue, kein MVT-Zwischenformat, kein persistenter Produkt-Cache.
- `BuildingField::AcceptedInput` pinnt Geometrie und Quelle je Produkt; Indizes gelten
  nur in dessen Points/Rings. `BuildingStampJob` und `Laying` verwenden diese Besitzer.
  OSM verformt DEM über dieselben Terrain-Stempel wie vorhandene Straßen und Gebäude.
  Ersatz publiziert atomar; Fehler erhalten gültigen Altstand und Straßenqualität.
- Gepinnte Original-IDs erschließen sämtliche Tags für `StructurePlan`/`BuildingMesh`.
  Klassen, Dachformen und Material bleiben erhalten; `PitchedShare` ersetzt keinen Dachtag.
  Schornsteine erhalten keine Wohnfassade. Parts behalten Höhenintervalle und Besitzer.
- `OsmBuildingHeights` normalisiert Höhe/min_height und levels/min_level an der Grenze.
  Fehlende, gültige und fehlerhafte Angaben bleiben getrennt; Originalstrings bleiben erhalten.
  Widersprüche löst explizite Generatorpolitik; kein Sentinel wird zur gemessenen Höhe.
- Außenring, Höfe und erhöhte Parts bleiben bei jedem LOD erhalten. Kein Stamp, Sockel
  oder Pflaster unter erhöhten Parts; Zusammenfassung füllt weder Hof noch Durchfahrt.
  Earcut trianguliert perforierte Dächer/Böden vor Dachfaltung; Massing bleibt ungeteilt.
  Mesh und Stempel teilen Punkte und Revision; Innenringe brauchen eindeutige Außenbesitzer.

## Nachfrage und Speicher
Alle Azimute erhalten entfernungsangemessene residente Produkte bis zur konfigurierten
Sichtweite. Frustum begrenzt nur Zeichnen. Bewegung fragt neue Regionen mit Hysterese an;
Überlast verschiebt Arbeit ohne Frame-Blockade. Kohärente Quellenstände publizieren atomar.
SSD hält nur empfangene Rohbytes. RAM hält begrenzte Indizes, Staging und CPU-Produkte;
GPU-Produkte sind vor Drehung zeichenbereit. Budgets folgen Bytes, Wiederverwendung,
IO-Latenz und Durchsatz; ständig verdrängte aktive Regionen sind keine gültige Residenz.

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
