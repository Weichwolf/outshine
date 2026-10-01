Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: world, data, engine, client
Tags: original-sources, loading, residency

# Original sources deliver a complete resident world

## Ergebnis und vorhandene Fähigkeit
Alle Places laden offizielle OSM-Originaldaten und Copernicus GLO-30 über dieselben
öffentlichen Providerverträge. Tags, Provenienz und vollständige Weltabdeckung bleiben
bis zur atomaren Publikation erhalten. Originalreader, paralleles IO, Quellcache,
getrennte Zell-Snapshots und native GLO-30-Samples existieren. Gebäude publizieren
getrennte Originalprodukte und Terrain-Stempel; Kameranachfrage, native Straßen/Wasser
und vollständige Place-Abnahme sind offen. Keine reduzierte Kartenquelle als Ersatz.

## Nächste Lieferung und Besitzer
`RuntimeWorldPreparation` fordert positionsgebundene Originalzellen an und übergibt
vollständige Nachfrage an `OsmSourceLoader`; `Laying` koordiniert Terrain und Produkte.
`world/data`, SourceSet und ContentStore besitzen Originalbytes/Receipt. Bestehende
Tasks/Fetching nutzen begrenztes paralleles IO und genau einen gemeinsamen Compute-
Worker; verbleibende private ClassBuilder-/VectorStreetGraphWorker-Threads migrieren.
Render/Audio bleiben getrennt. Keine zweite Importqueue oder globaler Objektmerge.

## Quellen- und Produktvertrag
- OSM: offizieller API-Katalog über GeoCellId, `map?bbox=west,south,east,north` und
  erforderliche Referenzhüllen. Begrenzte Originaldateien/-regionen bleiben Diagnosen.
  Dataset/Revision, typisierte IDs, Rollen und sämtliche Tags erhalten; widersprüchliche
  Überlappungen verwerfen, gleiche Objekte und konsumierte Relationswege eindeutig besitzen.
- GLO-30: `CopernicusRaster`/libtiff liefern native Meter aus Original-COG-Blöcken/Übersichten.
  HTTP-Bereiche, Dateilänge, starke Revisionspins und Receipt vor Cache/Lieferung prüfen.
  PixelIsPoint, Sample-Ursprung, X/Y-Abstände und Raster-NoData aus Metadaten lesen.
  EGM2008-Höhen sind keine Ellipsoidhöhen; DSM-Dächer/Bäume sind kein bewiesener nackter Boden.
- Native 1°-DEM- und geodätische OSM-Quellzellen bleiben unabhängig von Renderkacheln.
  Öffentliche Registrierung gilt für eingebaute wie externe Provider; Fehler kein Leerprodukt.
- Nur Original-Netzwerkbytes persistent cachen. Generator-Artefakt-Lookups/-Writes aus
  Client-Pfaden entfernen; vorhandene Dateien erhalten. RAM-/GPU-Produkte bleiben resident.
  Quellen, Parse/Build-Scratch, gepinnte Altstände und GPU-Produkte getrennt begrenzen.
- Unveränderte Zellen/Produkte übernehmen, veraltete Jobs abbrechen, geänderte Produkte
  gezielt ersetzen. Kandidaten veröffentlichen geschlossen; Fehler erhalten gültigen Altstand.

## Weitere Integration und Abnahme
Straßen/Wasser konsumieren dieselben Originalbestände (2281/2145); Klassifikation 2173.
2336 besitzt Bedarf/LOD über Boden, Flug und Orbit. Die vorhandene Quell-/Produkt-API
ermöglicht diese Kinder bereits; deren vollständige Fertigstellung blockiert diesen Anschluss nicht.
Ein frischer warmer Prozess liefert belegte Quellcachehits ohne Remote-Starts und ohne
Generator-Diskcache. Ziel: vollständiger warmer Aufbau in ein bis zwei Sekunden; das
verbindliche Place-Gate aus AGENTS bleibt maßgeblich. Keine fehlenden Ferngebäude,
verkürzte Abdeckung oder verlorenen Tags als Optimierung. Alle acht Places bleiben offen,
bis native Quelle, vollständige Runtime und tatsächliches Bild dieselbe Welt belegen.
