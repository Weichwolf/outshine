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
Coverage beobachtet nur; native Produkte bestimmen Ready. Erste Quellenbereitschaft verlangt keinen äußeren Ring.
Regionschlüssel binden Quell-Digests, Formparameter, Detailauftrag und Regeln; kein serialisiertes Straßennetz.
Exakte Kamera-Schlüssel und ganze Regionen ersetzen keine räumliche Eltern-/Kindhierarchie.
Ausstehende Grundrisse sind keine freie Fläche; Flensburg/Wien/CP/Tokyo liefern gleiche Miss-/Hit-Pixel.

## Kostenbefund und nächste Lieferung
Offline 1280×720/60, 60 Frames/360°, volle Treffer, Producer 8c49e00b2ee4:
| Place | Laden s | p99 ms | Bildphase / Peak GiB | Terrain / Prototypen entpackt MiB |
|---|---:|---:|---:|---:|
| Wien | 6,02 | 3,51 | 2,50 / 2,50 | 25 / 620 |
| CentralPark | 5,16 | 2,23 | 2,24 / 2,24 | 25 / 400 |
| Tokyo | 8,38 | 3,49 | 3,49 / 4,16 | 32 / 620 |
Alle zehn Places pixelgleich. Tokyo: Gebäudedecode 560,70 → 358,87 MiB, keine Formenreads bei LOD-Hits.
Vorher 725642fbe: W/CP/T Bildphase 2,21/2,00/3,49 GiB, Peaks 2,61/2,00/5,08 GiB.
Tokyo-Peak kleiner; W/CP-Bildphase höher. Live-Heap fast gleich, gleiche GPU-Puffer; Ursache noch offen.
OS-Footprint: 50-ms-Stichproben um Render/PNG, kein exaktes GPU-/Frameintervall; Sampler 2–4 ms CPU/Lauf.
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
3. P0: W/CP-Footprint-Regressionsursache beseitigen: Decoder-Scratch, Allocator-/Treiberreserven und Upload-Lebensdauer. Gemeldete Puffer erklären den OS-Footprint nicht vollständig.
   Pakete blockweise übernehmen; Weltkandidaten teilen unveränderliche Materialpakete; 4-MiB-Fixture: 368 statt 4.196.784 Byte Snapshot-Allokation.
   [Bildbesitz-Modell](../test/experiments/material_image_residency.py): Teilen statt Klonen; native Peaks separat messen.
4. P1: Modellprototypen bedarfsgerecht laden. 256² × 8 × 40 Byte = 20 MiB; 31 Atlanten = 620 MiB.
   Quantisierte Tiefe/Normalen/Material/Coverage vergleichen; 12 Byte/Texel wären 186 MiB,
   Hypothese mit Bild-/Fehlernachweis; entpackte Bytes sind keine dauerhafte Residency. Fachfremde Änderungen nicht
   invalidieren. Generator-/Codec-/Capture-Abhängigkeiten statt sämtlichen src/include-Code versionieren.
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
4. Quelldaten bleiben separat unverändert. Asset-Schlüssel binden Quellstand/Parameter und
   Generator-/Formatversion. Gelieferte und ergänzte Eigenschaften behalten ihre Herkunft.
   Vollständiges Asset samt Index atomar veröffentlichen; abgebrochene/kaputte Produkte sind Misses.
   Explizite Änderungen erneuern betroffene Assets, nicht die gesamte Welt oder unveränderte Quellen.
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
Integration 725642fbe: Tidy/Claims/Shader grün; fehlender gepinnter Khronos-Frame cfb0bc5a hält das Gesamtgate rot.
Frischer Offline-Prozess lädt vollständige Assets bei Hits ohne Providerdecode, Anreicherung,
Rohling-Neubau. Laufzeit-Nahdetails verwenden nur fertige Rohlinge und werden gezielt erneuert. Kalter Aufbau erzeugt genau einmal; Version-/Inputwechsel gezielt.
Räumliche/LOD-Abfragen gegen vollständige Referenz; Grenze, leere Region, Drehung, Bewegung, Wiederstart
ohne verlorene Assets/ungeplante Arbeit prüfen. Defekte/Teilpakete testen.
Alle Pflicht-Places, besonders Wien/CP/T: Bilder vergleichen, Laden/CPU/GPU/p99/SSD/RAM-Peaks getrennt
bei gleicher Sichtweite/Inhalt/Profil prüfen. Dateiexistenz allein belegt kein Ready.
