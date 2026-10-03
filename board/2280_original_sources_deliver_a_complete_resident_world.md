Type: feature
State: active
Architecture: planned
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
Zuerst Wien mit vollständigen Originalquellen bis zum residenten Rundumbild liefern,
danach die übrigen Places. Der Map-API-Erwerb scheitert am Anbieter-Bandbreitenlimit;
mehr Retries, Parallelität oder längere Fristen ersetzen keinen tragfähigen Quellenweg.
Die Erwerbsarchitektur bleibt deshalb `planned`; vorhandene Import-/Cacheverträge bleiben nutzbar.
Map-API-Abfragen sind kein belegter Bulk-Vertrag für den vollständigen Radius. Vor einem
weiteren Ausbau zuerst einen erlaubten Erwerbsweg mit vollständigem Objektabschluss liefern.
Kein Gesamtneustart: nur den unzureichenden Erwerb ersetzen; native Produkte und die
vorhandene Straßen-/Terrainqualität erhalten. Cachehinweise sind nachrangige Optimierungen.
Ein Bulk-Adapter muss Originalobjekte/Tags, Referenzabschluss, Revision und begrenzten
Erwerb erhalten. Regionale Drittanbieter-Extrakte benötigen eine ausdrückliche Erweiterung
der Quellenregel; weder reduzierten Inhalt noch eine weitere Quelle stillschweigend einsetzen.
2188 liefert den allgemeinen Generatorvertrag; sein übriger Sandbox-Ausbau blockiert
weder die Quellenvorbereitung noch die Abnahme vorhandener nativer Produkte. Die OSM-
Erweiterung unter `generators/osm` besitzt Provider, Adapter, Zellplanung und Erzeugung.
`SourceAcquisition`, `TransportPreparation`, Provider und API-/Zellerwerber liegen bereits
dort. Gebäudeparser/-adapter liegen in `generators/osm/buildings`, XML und Snapshots in
`generators/osm/import`. Native Produktverträge stehen in 2188; Engine-Aufrufer bleiben spezialisiert.
Allgemeine SourceSet-/ContentStore-Dienste halten Netzwerkbytes/Receipt. Bestehende
Tasks/Fetching nutzen begrenztes paralleles IO und genau einen gemeinsamen Compute-
Worker; StreetGraphPreparation und ClassificationBuild nutzen ihn ohne eigene Compute-Threads.
Render/Audio bleiben getrennt. Keine zweite Importqueue oder globaler Objektmerge.
`CellAcquisition`/`OsmCellPipeline`: ein IO-Besitzer betreibt bis zu acht Quellenanfragen;
fertige Zellen gehen einzeln
über höchstens zwei wartende XML-Produkte an den gemeinsamen Compute-Worker. Keine
Thread-Sicherheitsannahme über externe Provider/Transports. Ein langsamer Request hält
fertige Nachbarn nicht zurück. Revision/Abbruch gelten für IO, Übergabe und Decode;
atomare Publikation und vorhandene Snapshot-Admittanz bleiben verbindlich.
IO und Decode nutzen geliehene Queues; jeder OSM-Response ist auf 4 MiB begrenzt.

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
- Cache-/Residency-Regeln aus AGENTS gelten. Client, Shots und Prepare teilen dauerhaften
  SDL-Nutzerspeicher; kein Temp-Default. Generator-Artefakt-Lookups/-Writes aus Client-Pfaden entfernen.
  ContentStore/SourceSet speichern adressqualifizierte Netzwerk-Receipts mit Payload-Digest.
  Neue Prozesse rekonstruieren vorhandene Blattpartitionen aus verifizierten Originalbytes.
  Teilbestände vermeiden Elternproben; fehlende/beschädigte Bytes bleiben fehlende Abdeckung.
  `engine/EnginePreload.cpp` übergibt das öffentliche Preload-Budget auch als Quellenfrist,
  einschließlich laufender IO. Terrain-Sampling erhält ursprüngliche Fehleradresse,
  Quelle und Fehlergrund bis zur Runtime; eine Ablehnung liefert kein partielles Mesh.
  `FetchFailure` hält tatsächliche Quelle, Endstatus und Scheduler-Retryzahl. Der OSM-
  Reader reicht HTTP-Status und Transportgrund bis zum Client; andere Quelladapter
  müssen diese Metadaten noch übernehmen. Eine pauschale Ablehnung verliert keine Ursache.
  HTTP 509 beendet den Erwerb ohne automatische Wiederholung; erhaltene Originalbytes bleiben
  nutzbar. Ein erneuter Erwerb ist explizit, keine verdeckte Schleife gegen das Anbieterlimit.
  Bestätigtes HTTP 400 wegen Node-Kapazität fordert kleinere Zellen; HTTP 509 belegt
  diese Ursache nicht. Ablehnungen und Erwerbsplan-Hinweise sind niemals Datenabdeckung.
  CellCompiler überführt jede dekodierte Zelle auf dem gemeinsamen Compute-Worker in native
  Inputs vor dem vollständigen Erwerb. StructureCell hält Gebäudegrundrisse, konsumierte
  Referenzhüllen samt Tags und binäre SHA-256-Identitäten aller Originalobjekte; vollständige
  Archive werden freigegeben. Osm::StructurePreparation prüft Überlappungen und globale
  Relations-/Objektbesitzer, bevor StructureBuildQueue Terrainbedarf und Weltprodukte liefert.
  2188s öffentlicher Lebenszyklus und 2336s Detailbedarf vor Terrainarbeit bleiben offen.
  SourceAcquisition trennt Cache-Vorbereitung von residenten Eingaben: dekodierte Zellen
  nach Validierung freigeben, vollständige qualifizierte Byte-Abdeckung zuletzt auf IO prüfen.
  PlaceSourcePreparation verdrahtet registrierte Provider, HTTP und geliehene GenOSM-Worker;
  PrepareSourceCache gehört GenOSM. Der Client-Root darf dazu den Host-Transport bereitstellen.
  Prepare führt Cachephase vor Engine/Weltaufbau aus; beide teilen eine Gesamtfrist.
  Cache-Bereitschaft liefert keine Weltprodukte; erst vollständige Welt meldet Prepare ready.
  SourceDemand hält gemeinsame Zelllimits; GeodeticCamera denselben geografischen Fokus.
  Vollständigkeit folgt dem gesamten Weltbedarf, nicht dem Ende eines einzelnen IO-Auftrags.
  Die vorhandene Kronenvorbereitung übergibt Atlanten direkt im RAM; kein Runtime-Diskcache.
  Explizite Cachepfade bleiben erhalten. Per-Zell-Bytes qualifizieren den Katalog `current`;
  dieser Name behauptet keinen atomaren weltweiten OSM-Zeitstand.
  Quellen, Parse/Build-Scratch, gepinnte Altstände und GPU-Produkte getrennt begrenzen.
## Weitere Integration und Abnahme
Straßen/Wasser konsumieren dieselben Originalbestände (2281/2145); Klassifikation 2173.
2336 besitzt Bedarf/LOD über Boden, Flug und Orbit. Vorhandene native Produkte bleiben nutzbar;
2188s allgemeiner Lebenszyklus ersetzt konkrete Engine-Aufrufe. Kinder brauchen ihren
konsumierten Teilvertrag, keine pauschale Fertigstellung aller anderen Features.
Ein frischer warmer Prozess liefert belegte Quellcachehits ohne Remote-Starts und ohne
Generator-Diskcache. Ziel: ein bis zwei Sekunden warm; verbindliche Gates aus AGENTS gelten.
Keine fehlenden Ferngebäude, verkürzte Abdeckung oder verlorenen Tags als Optimierung.
Alle acht Places bleiben bis zum vollständigen Quellen-/Runtime-/Bildnachweis offen.
