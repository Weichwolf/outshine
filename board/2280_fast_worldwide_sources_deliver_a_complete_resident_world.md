Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends:
Area: generators, data, engine, client
Tags: sources, loading, residency

# Fast worldwide providers deliver a complete resident world

## Ergebnis und Ist
Wien und alle Places laden die volle benötigte Welt schnell und reproduzierbar.
Präferierte Anbieter stehen in AGENTS; die Anbieteranalyse begründet Austauschbarkeit
über Adapter statt blindem Mischen. XML/MVT/GLO-30, paralleles IO, verifizierter Quellcache
und native Gebäude-/Terrainprodukte bestehen. WorldSourcePolicy erzwingt noch Original-
OSM und verwirft Vektoren; normalisierte neue Quellen und aktuelle vollständige Bilder fehlen.

## Architekturentscheidung und Besitzer
2188 besitzt öffentliche Erweiterungs-/Produktverträge. `generators/osm` besitzt MVT/XML,
Schemaadapter und OSM-Erzeugung; Höhen-/Wettererweiterungen ihre Provider. Gemeinsame
HTTP-/Cache-/Jobs bleiben quellunabhängig. Keine zweite Importqueue oder Generatorroute.

| Formatprovider | Austauschbare API-Anschlüsse | Notwendige Anpassung |
|---|---|---|
| MVT-Vektoren | OpenFreeMap; alternativ VersaTiles/MapTiler/Stadia/Mapbox/ArcGIS | Schema/Layer/IDs/Buffer normalisieren, Rechte und Auth separat |
| Terrarium-Höhen | Mapterhorn; alternativ VersaTiles-Elevation/AWS Mapzen | WebP/PNG, 512/256 Pixel, Zoom/Datum/Quelle |
| PMTiles-Container | Mapterhorn Primary und dokumentierte Mirrors | Gepinnter Archivindex, Range/Revision; nur gleiche Stände teilen Bytes |
| Wetter-Snapshot | Open-Meteo free/paid; andere Wetter-APIs über Schemaadapter | Ort/UTC/Einheiten/Felder/Gültigkeit, keine erfundenen Wolkenschichten |

Endpunkte/Auth/XYZ-Reihenfolge/Limits sind Konfiguration. Formatgleichheit ist keine
Datasetgleichheit; aktueller Mapterhorn-Mirror ist nicht revisionsgleich. Ein Hauptanbieter
je Datenart; Wechsel atomar über abhängige Produkte. Kein beliebiges Tile-Mischbild.

## Nächste Lieferung und offene Quellenfragen
1. OpenFreeMaps Tile-Nutzung und großflächigen Offline-/Cache-Erwerb gegen die AGB abgrenzen;
   veröffentlichte Downloads sind ein eigener Lieferweg. Kein SLA aus Stichproben behaupten.
   Mapterhorn-Attribution/Quellenauflösung erhalten. Open-Meteo-Free ist nichtkommerziell;
   kommerzielles Archiv benötigt Professional+, dessen Endpoint ungemessen ist.
2. Anbieter-Konfiguration und Schemaadapter über bestehende Registrierung bis Wien liefern;
   Client-Originalzwang ersetzen. Vorhandene Originalreader bleiben Bibliotheksfähigkeit,
   kein Client-Fallback auf Editing-API. Diese API ist für unseren Bulk-Bedarf ungeeignet.
3. Vollständiges 240-km-/Höhenbedarfsmanifest aus 2336 vorbereiten; nur benötigte Hierarchiestufen
   erwerben. Datenumfang/Erwerb separat von Warmaufbau/Rendern prüfen; keine Radiuskürzung.
4. Central Park/Tokyo als dichte Erwerbsbenchmarks, anschließend alle acht Pflicht-Places.
   Straßen-/Wasser-/Gebäudequalität beim Quellenwechsel erhalten; echte Bilder vergleichen.

## Umsetzung und Invarianten
- MVT-MultiPolygone/Höfe/Parts korrekt dekodieren; Featurezahl ist keine Gebäudezahl.
  Straßenklassen/Ebenen, Höhen und Gewässer normalisieren, soweit geliefert. Fehlende
  Informationen explizit; aus Liniennähe weder Kreuzung noch Brücke erfinden.
- Native Höhenmeter mit Rastermaß/Zoom/Datum/NoData liefern. Terrarium nach verlustfreiem
  RGB-Decode; Terrain-RGB/COG nutzen eigene Decoder. Globales DSM ist kein nackter Boden.
- HTTP-Endstatus und Payload prüfen. Auth terminal; temporäre Fehler begrenzt mit Backoff/
  Retry-After und Ursprungssperre. Abbruch/Frist/Rückstau; Ausfall ≠ gültiges Leerprodukt.
- Netzwerkbytes mit Anbieter/Dataset/Adresse/Version/Digest cachen; bestehende Bestände
  erhalten. Begrenztes paralleles IO, Decode/Build auf gemeinsamem Worker. Native Inputs
  früh verdichten, Quellarchive freigeben; Grenzen für Quellen/Scratch/RAM/GPU getrennt.
- Client/Prepare/Shots teilen persistenten SDL-Nutzerspeicher, Registry und Weltbedarf.
  Quellcache-Vorbereitung getrennt vom frischen Warmprozess; keine generierten Diskprodukte.
  Tile-Ränder/Overlaps besitzen konsistente IDs/Ownership; Publikation bleibt atomar.

## Abnahme
Vollständiger warmer Place erfüllt AGENTS-Bild-/Zeitbudget ohne Netzwerk, Löcher oder
Qualitätsverlust. Kaltstartbytes/Acquire/Decode/Build/RAM/GPU getrennt belegen. Anbieter-
Stichproben beweisen weder 240-km-Gesamtvolumen noch Weltaufbau oder A18-Pro-Laufzeit.
Straßen/Wasser/Gebäude folgen 2281/2145/2173, Distanzhierarchie 2336; übriger Sandbox-Ausbau
blockiert keine Quellenvorbereitung. Offene Liefer-/Nutzungsverträge halten `planned`.
