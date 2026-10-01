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
getrennte Originalprodukte und Terrain-Stempel; der Client deklariert den offiziellen
Katalog und die Engine fordert den vollständigen Positionsradius an. Native Straßen/Wasser
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
  CellsAround plant den vollständigen Radius mit WGS84-Krümmungsschranke, Datumsgrenze und Polen.
  API-Level 9 umfasst 360×180/512² = 0,2472 Grad² je Zelle, unter dem API-Flächenlimit.
  Snapshot-Admittanz: 32×4 MiB = 128 MiB inklusive gepinnter Altstände, keine Vorallokation.
  Das ist eine Quellgrenze, kein bewiesenes App-Budget. Das API-Flächenlimit garantiert nicht
  das Node-Limit: Flensburg scheitert derzeit an einer Antwort mit mehr als 50.000 Nodes.
  Nächster Schritt: Kapazitätsablehnung am HTTP-Adapter typisieren und die betroffene Zelle
  durch vier flächendeckende Kinder ersetzen. Zellzahl, Tiefe, Speicher und Gesamtdauer bleiben
  begrenzt; erst die vollständige Blattmenge publizieren. Unveränderte Blätter wiederverwenden.
  Globale Grobquellen und die vollständige Place-Abnahme bleiben offen.
- GLO-30: `CopernicusRaster`/libtiff liefern native Meter aus Original-COG-Blöcken/Übersichten.
  HTTP-Bereiche, Dateilänge, starke Revisionspins und Receipt vor Cache/Lieferung prüfen.
  PixelIsPoint, Sample-Ursprung, X/Y-Abstände und Raster-NoData aus Metadaten lesen.
  EGM2008-Höhen sind keine Ellipsoidhöhen; DSM-Dächer/Bäume sind kein bewiesener nackter Boden.
- Native 1°-DEM- und geodätische OSM-Quellzellen bleiben unabhängig von Renderkacheln.
  Öffentliche Registrierung gilt für eingebaute wie externe Provider; Fehler kein Leerprodukt.
- Nur Original-Netzwerkbytes persistent cachen. Generator-Artefakt-Lookups/-Writes aus
  Client-Pfaden entfernen; vorhandene Dateien erhalten. RAM-/GPU-Produkte bleiben resident.
  Client, Shots und Prepare teilen dauerhaften SDL-Nutzerspeicher; kein Temp-Default.
  Die vorhandene Kronenvorbereitung übergibt Atlanten direkt im RAM; kein Runtime-Diskcache.
  Explizite Cachepfade bleiben erhalten. Per-Zell-Bytes qualifizieren den Katalog `current`;
  dieser Name behauptet keinen atomaren weltweiten OSM-Zeitstand.
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
