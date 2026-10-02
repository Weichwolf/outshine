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
Nach 2188 übergibt Engine allgemeinen Weltbedarf an registrierte Generatoren. Die OSM-
Erweiterung unter `generators/osm` besitzt Provider, Adapter, Zellplanung und Erzeugung.
`SourceAcquisition`, `TransportPreparation`, Provider und API-/Zellerwerber liegen bereits
dort. OSM-Gebäudeparser und Strukturadapter liegen in `generators/osm/buildings`; native
StructureFootprints/Höhenintervalle enthalten keine Tags oder Quellformate; der Gebäudebake
übernimmt native Ringe/Höhen über seinen eigenen StructureInput; konsumierte Objekt-IDs prüft SourceObjects ohne OSM-Typen im Bake. XML-Decoder/Validierung bleiben in
world/data, Engine-Aufrufer sind noch spezialisiert.
Allgemeine SourceSet-/ContentStore-Dienste halten Netzwerkbytes/Receipt. Bestehende
Tasks/Fetching nutzen begrenztes paralleles IO und genau einen gemeinsamen Compute-
Worker; StreetGraphPreparation und ClassificationBuild nutzen ihn ohne eigene Compute-Threads.
Render/Audio bleiben getrennt. Keine zweite Importqueue oder globaler Objektmerge.
Der serielle Read-/Decode-Zyklus wird durch eine begrenzte Pipeline ersetzt: ein IO-
Besitzer betreibt bis zu acht unabhängige Quellenanfragen; fertige Zellen gehen einzeln
über höchstens zwei wartende XML-Produkte an den gemeinsamen Compute-Worker. Keine
Thread-Sicherheitsannahme über externe Provider/Transports. Ein langsamer Request hält
fertige Nachbarn nicht zurück. Revision/Abbruch gelten für IO, Übergabe und Decode;
atomare Publikation und vorhandene Snapshot-Admittanz bleiben verbindlich.
`CellAcquisition` und `OsmCellPipeline` setzen diesen Erwerb bereits um; IO und Decode nutzen explizit geliehene Queues. Der Client
begrenzt jeden OSM-Response auf 4 MiB. Vollständige warme Place-Abdeckung ist noch offen.

## Quellen- und Produktvertrag
- OSM: offizieller API-Katalog über GeoCellId, `map?bbox=west,south,east,north` und
  erforderliche Referenzhüllen. Begrenzte Originaldateien/-regionen bleiben Diagnosen.
  Dataset/Revision, typisierte IDs, Rollen und sämtliche Tags erhalten; widersprüchliche
  Überlappungen verwerfen, gleiche Objekte und konsumierte Relationswege eindeutig besitzen.
  CellsAround plant den vollständigen Radius mit WGS84-Krümmungsschranke, Datumsgrenze und Polen.
  API-Level 9 umfasst 360×180/512² = 0,2472 Grad² je Zelle, unter dem API-Flächenlimit.
  Snapshot-Admittanz: 32×4 MiB = 128 MiB inklusive gepinnter Altstände, keine Vorallokation.
  Das ist eine Quellgrenze, kein bewiesenes App-Budget. Das API-Flächenlimit garantiert nicht
  das Node-Limit von 50.000; überlastete Zellen liefern kein gültiges Leerprodukt.
  Der HTTP-Adapter typisiert Kapazitätsablehnungen; OsmCellRefinement ersetzt betroffene Zellen
  durch vier flächendeckende Kinder. Zellzahl, Tiefe, Speicher und Gesamtdauer bleiben begrenzt.
  IO bevorzugt bereits verfeinerte Blätter vor weiteren groben Kapazitätsproben. Begrenzte
  Vorbereitungen hinterlassen vollständige Original-Teilregionen; die globale Publikation
  bleibt atomar. Diese Reihenfolge senkt weder die Gesamtanfragen noch beweist sie das Ladebudget.
  Erst die vollständige Blattmenge publizieren; unveränderte Pläne und Blätter wiederverwenden.
  Quellzellen nicht allein am Sichtkreis wegschneiden: nodebasierte API-Abfragen können
  über äußere Nodes auch Geometrie innerhalb des Radius liefern. Objektabschluss zuerst beweisen.
  Globale Grobquellen und die vollständige Place-Abnahme bleiben offen.
- GLO-30: `CopernicusRaster`/libtiff liefern native Meter aus Original-COG-Blöcken/Übersichten.
  HTTP-Bereiche, Dateilänge, starke Revisionspins und Receipt vor Cache/Lieferung prüfen.
  Vor der Dekodierung fehlende Abschnitte des vollständigen komprimierten Originalblocks
  laden; libtiffs interne Lesestücke sind keine vollständigen Blöcke. Metadatenbereiche
  wiederverwenden, überlappende Requests vermeiden, Byte-/Abschnittsgrenzen erhalten.
  PixelIsPoint, Sample-Ursprung, X/Y-Abstände und Raster-NoData aus Metadaten lesen.
  EGM2008-Höhen sind keine Ellipsoidhöhen; DSM-Dächer/Bäume sind kein bewiesener nackter Boden.
- Native 1°-DEM- und geodätische OSM-Quellzellen bleiben unabhängig von Renderkacheln.
  Öffentliche Registrierung gilt für eingebaute wie externe Provider; Fehler kein Leerprodukt.
- Nur Original-Netzwerkbytes persistent cachen. Generator-Artefakt-Lookups/-Writes aus
  Client-Pfaden entfernen; vorhandene Dateien erhalten. RAM-/GPU-Produkte bleiben resident.
  Client, Shots und Prepare teilen dauerhaften SDL-Nutzerspeicher; kein Temp-Default.
  ContentStore/SourceSet speichern adressqualifizierte Netzwerk-Receipts mit Payload-Digest.
  Neue Prozesse rekonstruieren vorhandene Blattpartitionen aus verifizierten Originalbytes.
  Auch Teilbestände vermeiden erneute Elternproben; alle fehlenden Quadranten bleiben Pflicht.
  Vorhandene Teilbytes beweisen keine vollständige Abdeckung.
  Fehlende oder beschädigte Bytes sind keine Abdeckung. Keine Generatorprodukte persistieren.
  `engine/EnginePreload.cpp` übergibt das öffentliche Preload-Budget auch als Quellenfrist,
  einschließlich laufender IO. Terrain-Sampling erhält ursprüngliche Fehleradresse,
  Quelle und Fehlergrund bis zur Runtime; eine Ablehnung liefert kein partielles Mesh.
  `FetchFailure` hält tatsächliche Quelle, Endstatus und Scheduler-Retryzahl. Der OSM-
  Reader reicht HTTP-Status und Transportgrund bis zum Client; andere Quelladapter
  müssen diese Metadaten noch übernehmen. Eine pauschale Ablehnung verliert keine Ursache.
  Internet-Erwerb erhält seine eigene Frist und darf länger dauern. Erst mit vollständigem
  Quellcache gilt das Zehn-Sekunden-Gate; ein frischer Offline-Prozess prüft den warmen Aufbau.
  Als Nächstes 2188s Produkt-/Pinvertrag und 2336s Detailbedarf vor Terrainarbeit anschließen:
  native Zellen ingestieren, ohne sämtliche Roh-Snapshots bis zur fertigen Fernwelt zu halten.
  SourceAcquisition trennt Cache-Vorbereitung von residenten Eingaben: dekodierte Zellen
  nach Validierung freigeben, vollständige qualifizierte Byte-Abdeckung zuletzt auf IO prüfen.
  Cache-Bereitschaft liefert keine Weltprodukte; der Prepare-Client muss diesen Pfad noch anschließen.
  Originalcache vollständig vorbereiten und tatsächlichen Place-Aufbau messen; Erwerb allein
  beweist weder das Ladebudget noch Bildqualität. Keine weitere reine Wartezeit-Optimierung.
  Geschlossene dekodierte Teilregionen geben native Erzeugung frei, während weitere Quellen
  laden. Decode und Erzeugung teilen den Compute-Worker; Produkte ersetzen unnötige Archivpins.
  Vollständigkeit folgt dem gesamten Weltbedarf, nicht dem Ende eines einzelnen IO-Auftrags.
  Die vorhandene Kronenvorbereitung übergibt Atlanten direkt im RAM; kein Runtime-Diskcache.
  Explizite Cachepfade bleiben erhalten. Per-Zell-Bytes qualifizieren den Katalog `current`;
  dieser Name behauptet keinen atomaren weltweiten OSM-Zeitstand.
  Quellen, Parse/Build-Scratch, gepinnte Altstände und GPU-Produkte getrennt begrenzen.
- Unveränderte Zellen/Produkte übernehmen, veraltete Jobs abbrechen, geänderte Produkte
  gezielt ersetzen. Kandidaten veröffentlichen geschlossen; Fehler erhalten gültigen Altstand.

## Weitere Integration und Abnahme
Straßen/Wasser konsumieren dieselben Originalbestände (2281/2145); Klassifikation 2173.
2336 besitzt Bedarf/LOD über Boden, Flug und Orbit. Vorhandene native Produkte bleiben nutzbar;
2188s allgemeiner Lebenszyklus ersetzt konkrete Engine-Aufrufe. Kinder brauchen ihren
konsumierten Teilvertrag, keine pauschale Fertigstellung aller anderen Features.
Ein frischer warmer Prozess liefert belegte Quellcachehits ohne Remote-Starts und ohne
Generator-Diskcache. Ziel: vollständiger warmer Aufbau in ein bis zwei Sekunden; das
verbindliche Place-Gate aus AGENTS bleibt maßgeblich. Keine fehlenden Ferngebäude,
verkürzte Abdeckung oder verlorenen Tags als Optimierung. Alle acht Places bleiben offen,
bis native Quelle, vollständige Runtime und tatsächliches Bild dieselbe Welt belegen.
