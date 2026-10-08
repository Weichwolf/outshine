Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends:
Area: generators, data, engine, client
Tags: assets, loading, cache, spatial-index, residency

# Prepared base assets load from a spatial cache

## Ergebnis und Ist
Hierarchischer Weltbedarf (2336) → räumlicher Assetindex → Hit: fertigen Rohling laden.
Nur fehlende benötigte Produkte starten Generator/Provider/Quellcache/API; anschließend atomar
speichern und denselben Lade-/Publikationspfad bedienen. Das gilt für alle Weltklassen.
Vorhanden: öffentlicher AssetCache/ResolveAsset, SQLite-R*Tree, native Geometrie-/Netzcodecs,
Höhenfelder, Gebäuderohlinge/LOD-Produkte und Impostoren. Pakete komprimiert Zstandard Level 1;
alte Rohpakete/Quellen erhalten, native Länge/CRC/Frame prüfen. Keine Gerätehandles persistieren.
Straßennetz-Treffer umgehen Layout/Profilierung; fehlende Höhen ergeben kein Ready.
Regionshits überspringen Boden-/Straßen-/Wassererzeugung und rohe Terrain-Mesh-Jobs.
Pegel, Konturen und Flussprofile kommen bei Hits aus demselben Regionspaket; keine Wasser-Höhenabfragen.
Coverage beobachtet nur; native Produkte bestimmen Ready. Screenshot-Kennzahlen beschreiben veröffentlichte Produkte, keinen unbefriedigten Quellbedarf. Erste Quellenbereitschaft verlangt keinen äußeren Ring.
Regionschlüssel binden Quell-Digests, Formparameter, Detailauftrag und Regeln; kein serialisiertes Straßennetz.
Exakte Kamera-Schlüssel und ganze Regionen ersetzen keine räumliche Eltern-/Kindhierarchie.
Ausstehende Grundrisse sind keine freie Fläche; Flensburg/Wien/CP/Tokyo liefern gleiche Miss-/Hit-Pixel.

## Kostenbefund und nächste Lieferung
Historischer OS-Footprint: Offline 1280×720/60, 60 Frames/360°, Treffer 009f6a2b7 / Producer e46c4abd61a5:
| Place | Laden s | p99 ms | Bildphase / Peak GiB | Terrain / Prototypen entpackt MiB |
|---|---:|---:|---:|---:|
| Wien | 5,85–6,13 | 3,28–11,40 | 2,46–2,47 / 2,46–2,53 | 25 / 186,34 |
| CentralPark | 4,79 | 2,49 | 2,21 / 2,22 | 25 / 120,22 |
| Tokyo | 7,87–8,13 | 14,29–24,25 | 3,95 / 4,18–4,19 | 32 / 186,34 |
OS-Footprint: 50-ms-Stichproben um Render/PNG, kein exaktes GPU-/Frameintervall; Sampler 1,9–3,5 ms CPU/Lauf.
27b7f413b: fünf warme Place-Gates in beiden Builds grün; Laden 2,93–8,52 s, p99 1,92–7,97 ms (2340).
Erster Aufbau: gemeldeter C++-Live-Heap CP 1,76, Feldkirch 3,42, Tokyo 2,33, Wien 1,85 GiB.
Live-Heap und Produktmaximum sind verschiedene Zeitpunkte, kein OS-Footprint/GPU-Budgetbeweis.
Gebäude speichern jetzt Schema-4-Metadaten und separat komprimierte Formen je belegter Zelle.
Auswahl öffnet keine Formen; Detail lädt seine Zelle. Misses nutzen gecachte Pläne/Kontakte, keine Höhenprovider.
Inhaltsschlüssel bleiben stabil; Schema 3 wird atomar nach 4 übernommen, ohne Quellbeschaffung.
Pläne/Koordinaten bleiben im Elternpaket; kleiner Hierarchieindex und Arbeitsmenge noch offen.
[Paketmodell](../test/experiments/prepared_building_residency.py): Tokyo historisch 553,40 MiB, Formen 325,69; 1-km-Zellen 4,92 MiB, kein Bildnachweis.
1. P0: Bedarf aus 2336 vor Quellbeschaffung und Cachedecode anschließen. Räumliche Eltern/Kind-Rohlinge
   statt Kamerasnapshots: kleine Bounds-/Produktindizes, gecachte Fernprodukte, unabhängige komprimierte
   Form-/Kontaktblöcke. Erst Bedarf → Paket öffnen; Rohling-Formen behalten, keine Rekonstruktion bei Hits.
2. P0: verbliebene Quellenproben aus nativen Höhen-/Kontaktprodukten bedienen; 25–32 MiB Felder
   bei Treffern vermeiden. Kandidaten teilen unveränderte Daten; einmal laden, dann Scratch frei.
3. P0: bestätigten Tokyo-Anstieg und verbliebene W/CP-Footprint-Regressionsursache zuordnen/beseitigen: Decoder-Scratch, Allocator-/Treiberreserven und Upload-Lebensdauer. Gemeldete Puffer erklären den OS-Footprint nicht vollständig.
   Renderer: identische ungebundene Maps pro Device/Transfer-/Samplerzustand teilen; heute 4.538 Staging-Allokationen in Tokyo. Pakete blockweise übernehmen; Materialpakete teilen.
   [Bildbesitz-Modell](../test/experiments/material_image_residency.py): Teilen statt Klonen; native Peaks separat messen.
4. P1: [Reales Paketmodell](../test/experiments/impostor_ready_payload.py): Capture 20 MiB, vorhandene Flat-Karten 6 MiB Payload.
   Native Karten/RGBA8-Maps über GeometryAsset implementiert; Hits umgehen Coverage-Füllung, Farbkonversion und Kartenaufbau.
   Atlas-Rohlinge bleiben; Miss/Defekt repariert nur Karten. Bytegleichheit/Hit ohne Vorbereitung geprüft; zehn Places unverändert, entpackte Bytes ca. −70 %. RAM/Frames verbessert dies nicht allgemein.
   Fachfremde Änderungen nicht invalidieren: Generator-/Codec-/Capture-Abhängigkeiten statt sämtlichen src/include-Code versionieren.
Ladeziel: warm <10 s, <1 s als Challenge; Gebäudebedarf vor Decode, Straßen erhalten.
## Besitzer und Grenzen
2280: Speicherung/Index/Laden; Generatoren: Anreicherung/Inhalt; 2336: Hierarchie/LOD; 2188: API.
AssetCache besitzt Speicherung/Kompression/Integrität und die AssetRecord-Hülle; Nutzdaten sind opaque.
Versionierte Codecs gehören Produkttypen, Fachrohlinge ihrer Erweiterung.
Generatoren teilen Geometrie-/Netzcodecs; GPU-Handles bleiben flüchtig. Kein Universalformat für Weltzustand.
## Fertige Assets
- Gebäude: Typ, vollständige Höhen/Dachparameter, Grundrisse/Parts/Höfe, Terrainkontakt,
  Material-/Fassadenpläne/Seeds, einfache Hüllen und geeignete LOD-/Verbandsprodukte.
  Laibungen, Rahmen, Dachdetails und andere Nahgeometrie dürfen daraus zur Laufzeit entstehen.
- Terrain/Infrastruktur: vorbereitete Höhen/Kontakte, Mesh-/Materialprodukte samt Nachbar-/LOD-
  Anschlüssen. Kein erneuter DEM-/MVT-Decode oder Straßen-/Terrainaufbau bei Assettreffern.
- Vegetation: Bestands-/Instanzdaten, gemeinsame Prototypen mit LODs, Material/Alpha-Mips,
  Windparameter und Fernverbände. 2111 bleibt nach Gebäuden/Terrain/Infrastruktur.
- Kompakte, deviceunabhängige Renderprodukte; kein Fine-Mesh je Fernhaus/Prototypkopie je Instanz. Licht, Wetterantwort und dynamische Pose bleiben aktuell.
## Index und Laden
1. Gemeinsamer Index unterstützt Frustum und Radius R um Weltposition x,y,z. Konservative Bounds
   liefern Kandidaten; Ebenen-/Abstandstests und LOD wählen tatsächlich benötigte Produkte.
   Abfragen dürfen nicht alle Objekte öffnen/dekodieren. Reine Frustum-Selektion ist kein IO-Befehl
   je Frame: grobe Rundumprodukte und nahe Arbeitsmenge bleiben für schnelle Drehungen resident.
2. Metadaten enthalten stabile Asset-ID, Weltbounds/Anker, Produkt-/Generatorversion, LOD-/Eltern-
   Kindbezug, Qualitäts-/Kostenangaben, Abhängigkeiten und Speicherort/Bytebereich. Assetkeys
   binden Region, Produkt/LOD, Seed/Parameter und Generatorversion, keine reine Blickrichtung.
   Gemeinsame Abfragen ohne Place-Listen/OSM-Spalten; Builtins und Erweiterungen nutzen denselben Vertrag.
3. Regelmäßige OSM-/DEM-Quellkacheln direkt über Kachel-IDs adressieren. SQLite-R*Tree hält
   native Paketbounds in ECEF; genaue Radius-/Frustumtests folgen konservativer Boxabfrage.
   Geladene Pakete verwenden gepackte Bereiche, keine SQL-Abfrage je Frame.
   [Python-Modell](../test/experiments/asset_residency.py): 100.545 Polygone → 1.837 Pakete,
   25,17 MB, warmer Dateicache ca. 2,9 ms statt 5,3 s Decode; kein nativer/GPU-Nachweis.
   Weite residente Abfragen bevorzugen Arrays, lokale persistente Abfragen den Paket-R*Tree.
   Räumlich gebündelte Assetpakete/Bereichslesungen, keine Datei/IO-Anfrage je Haus/Baum.
4. Quelldaten bleiben separat unverändert; Assetkeys binden zusätzlich Quellstand/Formatversion.
   Herkunft gelieferter/ergänzter Angaben erhalten; vollständige Assets samt Index atomar veröffentlichen.
   Defekt/Abbruch ist ein Miss; explizite Änderungen erneuern nur betroffene Assets.
5. Cachemisses bündeln: identische Anforderungen teilen genau einen Erzeugungsjob; räumliche
   Pakete statt Einzeljobs je Haus/Baum. Job nutzt paralleles begrenztes IO und einen Compute-Worker.
   Nur fehlende benötigte Produkte erzeugen; Hits/Misses teilen den Ladepfad, Abbruch/Rückstau/Fehler explizit.
   Fehlende Details behalten gültige Eltern bis vollständige Kinder bereitstehen.
6. SSD → RAM → GPU haben getrennte Residencybudgets. Räumliche Vorhaltebereiche/Hysterese
   verhindern Flattern; IO/Entpacken blockieren keinen Frame. CPU-Scratch nach Aufbau freigeben.
   Feine Runtime-Details zuerst freigeben, wenn ungenutzt; grobe Rundumprodukte länger halten.
   Sichtbare/interaktions-/schattenrelevante Produkte bleiben gepinnt. TTL ist optionale
   Residency-Hysterese je Detail-/Kostenklasse, keine Qualitätskürzung oder Ablaufzeit gültiger
   Rohlinge. SSD-Assetbudget eviktiert ungenutzte Produkte nach Kosten/Nutzung/Bytes; Quellen
   bleiben unverändert. Weniger Thrashing statt möglichst früher Freigabe optimieren.
## Quellen und Verfahren
OpenFreeMap/MVT, Mapterhorn/Terrarium, Open-Meteo/JSON: je Datenart ein Hauptanbieter.
Endstatus/Payload, Abbruch, begrenztes Retry/Backoff/Retry-After und Rückstau über libcurl.
Bestätigtes NoData bleibt von Transportfehlern unterscheidbar. Offline/Attribution bleibt zu klären.
[Systembibliotheken](../doc/dependencies.md), [Auswahl/Fallback vor IO](../doc/references/engine/visibility-driven-demand.md).
[Lokale Verfahrensnotiz](../doc/references/engine/unreal/prepared-assets.md) trennt Belege und Outshine-Entscheidungen.
[Unreal DDC](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-derived-data-cache-in-unreal-engine):
Lookup → Miss erzeugt/speichert; UE nutzt DDC beim Asset-Build, gekochte Spiele brauchen ihn nicht.
[World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine) /
[HLOD](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine):
räumliche Zellen und grobe Verbandsassets. [Retention](https://dev.epicgames.com/documentation/en-us/unreal-engine/texture-streaming-overview-for-unreal-engine):
Sichtbedarf, Speicherbudget und letzte Nutzung; kein Beleg für feste Detail-TTL-Sekunden.
## Abnahme
Integration aba95cbe1: Format/Tidy (357 Units)/32 Claims/48 öffentliche Header/481 Shader grün;
voller Lauf Exit 2: gepinnte PNGs fehlen. Place-Gates 9/10, Tokyo-p99 rot; Startspitzen offen (2340).
Frischer Offline-Prozess lädt vollständige Assets bei Hits ohne Providerdecode, Anreicherung,
Rohling-Neubau. Laufzeit-Nahdetails verwenden nur fertige Rohlinge und werden gezielt erneuert. Kalter Aufbau erzeugt genau einmal; Version-/Inputwechsel gezielt.
Räumliche/LOD-Abfragen gegen vollständige Referenz; Grenze, leere Region, Drehung, Bewegung, Wiederstart
ohne verlorene Assets/ungeplante Arbeit prüfen. Defekte/Teilpakete testen.
Alle Pflicht-Places, besonders Wien/CP/T: Bilder vergleichen, Laden/CPU/GPU/p99/SSD/RAM-Peaks getrennt
bei gleicher Sichtweite/Inhalt/Profil prüfen. Dateiexistenz allein belegt kein Ready.
