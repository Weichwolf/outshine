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
WaterAsset besitzt kompakte Koordinaten/Pegel/Konturen/Flussprofile/Tileindex; unveränderliche Abfragen, Mesh und Decoder ohne OsmField. WaterField besitzt allein Erzeugung/Fortschritt; Wasser-Mesher erreicht OSM nicht mehr.
Coverage beobachtet nur; native Produkte bestimmen Ready und veröffentlichte Screenshot-Kennzahlen.
Regionschlüssel binden Quell-Digests, Formparameter, Detailauftrag und Regeln; kein serialisiertes Straßennetz.
Ausstehende Grundrisse sind keine freie Fläche; Flensburg/Wien/CP/Tokyo liefern gleiche Miss-/Hit-Pixel.

## Kostenbefund und nächste Lieferung
Historischer OS-Footprint: Offline 1280×720/60, 60 Frames/360°, Treffer 009f6a2b7 / Producer e46c4abd61a5:
| Place | Laden s | p99 ms | Bildphase / Peak GiB | Terrain / Prototypen entpackt MiB |
|---|---:|---:|---:|---:|
| Wien | 5,85–6,13 | 3,28–11,40 | 2,46–2,47 / 2,46–2,53 | 25 / 186,34 |
| CentralPark | 4,79 | 2,49 | 2,21 / 2,22 | 25 / 120,22 |
| Tokyo | 7,87–8,13 | 14,29–24,25 | 3,95 / 4,18–4,19 | 32 / 186,34 |
OS-Footprint: 50-ms-Stichproben um Render/PNG, kein exaktes GPU-/Frameintervall; Sampler 1,9–3,5 ms CPU/Lauf.
Gebäude speichern jetzt Schema-4-Metadaten und separat komprimierte Formen je belegter Zelle.
Auswahl öffnet keine Formen; Detail lädt seine Zelle. Misses nutzen gecachte Pläne/Kontakte, keine Höhenprovider.
Inhaltsschlüssel bleiben stabil; Schema 3 wird atomar nach 4 übernommen, ohne Quellbeschaffung.
Pläne/Koordinaten bleiben im Elternpaket; kleiner Hierarchieindex und Arbeitsmenge noch offen.
[Paketmodell](../test/experiments/prepared_building_residency.py): Tokyo historisch 553,40 MiB, Formen 325,69; 1-km-Zellen 4,92 MiB, kein Bildnachweis.
1. P0: Bedarf aus 2336 vor Quellen/Cachedecode; räumliche Eltern/Kinder statt Kamerasnapshots.
   [Liefermodell](../test/experiments/building_asset_delivery.py): drei große Schema-4-Wurzeln 12,0–12,9 MB; LOD-Basis modelliert 1,94–2,30 MB. Treffer braucht
   Koordinaten/Origin/Höhenbindung/Quell-IDs und Zellen; volle Pläne/Auswahl nur bei Miss/Nahdetail.
   Kein Runtime-/RAM-Beleg; Koordinaten-Packen dort nur 5–6 % Gewinn am Koordinatenanteil.
2. P0: verbliebene Terrainproben aus nativen Höhen-/Kontaktprodukten bedienen, 25–32 MiB Zwischenfelder bei Hits vermeiden. WaterAsset integriert; monotone Bereichsumsetzung erhält Konturreihenfolge/Löcher. [Packmodell](../test/experiments/water_asset_coordinates.py): drei echte Pakete, 9–286 kB statt 0,19–60 MB Gesamtkoordinaten. Weitere Generatoren trennen fertige Rohlinge von Cursor/Quelllayout; gemeinsamer Vertrag aus 2188.
3. P0: bestätigten Tokyo-Anstieg und verbliebene W/CP-Footprint-Regressionsursache zuordnen/beseitigen: Decoder-Scratch, Allocator-/Treiberreserven und Upload-Lebensdauer. Gemeldete Puffer erklären den OS-Footprint nicht vollständig.
   Renderer teilt ungebundene Maps je Device/Transfer/Sampler und Materialpakete ([Modell](../test/experiments/material_image_residency.py)). Native Bilder speichern die Basis einmal und optionale untere Mips für Linearwerte, sRGB-Farbe oder Normalmomente; keine GPU-Handles. Producer bereitet vor Publikation vor, Treffer uploaden direkt. Eigene Codec-/Produktversion; bildlose Assets und Captures bleiben gültig. Rosenheim-Sampling: Mainthread 4,63 → 2,96 s; beobachtete Mip-/Upload-Leaves 831 → 4 ms, kein GPU-Zeitnachweis.
4. P1: [Paketmodell](../test/experiments/impostor_ready_payload.py): Capture 20 MiB, Flat-Karten 6 MiB; [Mip-Modell](../test/experiments/prepared_image_mips.py): +2 MiB untere Stufen, keine Basisduplikation.
   Native GeometryAsset-Karten umgehen Coverage-/Farbvorbereitung bei Hits; Atlas-Rohlinge bleiben.
   Miss/Defekt repariert nur Karten; mit Mips ca. −60 % entpackte Bytes gegenüber Captures. Das 16²-Fixture trägt 4.297 B Basis + 1.020 B Mips + 15 B Schema statt 5.268 B Capture; kein allgemeiner RAM-/Framegewinn.
   Atlas-Rezept bindet Artdefinition, Größe/Blicke und getrennte Generator-/Capture-Versionen; Änderungen an Wachstum oder Capture-Semantik erhöhen die jeweilige Version. Codecs prüfen eigene Formate. Fachfremde Engine-/Buildänderungen invalidieren nicht.
5. P1: SSD dauerhaft begrenzen: Asset-DBs nach Aufbau/Retention 1,82 statt 6,02 GiB; zehn Offline-Place-PNGs SHA-256-identisch. Verwendete Assets, Paketmitglieder und Eltern erhalten.
   Automatische budgetierte Verdrängung fehlt; Nutzdaten bleiben opaque, aktive Assets/Quellen erhalten. Warm <10 s; Gebäudebedarf vor Decode.
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
Wasser: zehn bildgleiche Miss-/Hit-Paare und ein weiterer frischer Hit ohne Impostor-Neubau;
warm 1,83–5,50 s, p99 0,61–8,13 ms. Erstaufbau neuer Regionen 18,69–68,67 s;
kalte Startspitzen CP 13,05/Koerbersee 25,10 ms bleiben bei 2340. Kein OS-RAM-/GPU-Zeit-Gewinnbeweis.
Wasserdecoder und Abfragen sind quellunabhängig; Regionsschlüssel und übrige Quellenarbeit noch nicht.
Volles Lint 6d462324c: Exit 2, fehlendes gepinntes Khronos-PNG und falscher Wasser-Headerguard;
Headerguard korrigiert, neuer vollständiger Lauf ausstehend.
Frischer Offline-Prozess lädt vollständige Assets bei Hits ohne Providerdecode, Anreicherung,
Rohling-Neubau. Laufzeit-Nahdetails verwenden nur fertige Rohlinge und werden gezielt erneuert. Kalter Aufbau erzeugt genau einmal; Version-/Inputwechsel gezielt.
Räumliche/LOD-Abfragen gegen vollständige Referenz; Grenze, leere Region, Drehung, Bewegung, Wiederstart
ohne verlorene Assets/ungeplante Arbeit prüfen. Defekte/Teilpakete testen.
Alle Pflicht-Places, besonders Wien/CP/T: Bilder vergleichen, Laden/CPU/GPU/p99/SSD/RAM-Peaks getrennt
bei gleicher Sichtweite/Inhalt/Profil prüfen. Dateiexistenz allein belegt kein Ready.
