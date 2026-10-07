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
Die Regionintegration überspringt spätere Boden-/Straßen-/Wassererzeugung,
liest aber zuvor weiter Zwischenfelder. Exakte Kamera-Schlüssel und ganze Regionen ersetzen
keine räumliche Eltern-/Kindhierarchie. Koerbersee speichert nach Entfernen redundanter
Terrain-Dreiecke 83,57 MiB und lädt warm in 3,59 s, p99 2,51 ms; gleiche Pixel wie vorher.
Flensburg liefert nach kohärenter Flächenbelegung bei Miss und Hit dieselben Pixel.
Ausstehende Gebäudegrundrisse sind keine freie Fläche; Wien/CP/Tokyo liefern gleiche Miss-/Hit-Pixel.

## Kostenbefund und nächste Lieferung
Offline 1280×720/60, 60 Frames/360°, volle Treffer, Producer 66067cb7ed9f:
| Place | Laden s | p99 ms | Prozess-Footprint GiB | Terrain / Prototypen entpackt MiB |
|---|---:|---:|---:|---:|
| Wien | 7,73 | 3,47 | 2,80 | 874 / 620 |
| CentralPark | 6,18 | 1,10 | 2,14 | 709 / 400 |
| Tokyo | 11,38 | 3,38 | 5,23 | 971 / 620 |
Regionprodukte: 85,88 / 73,43 / 114,45 MiB; separater Decode 156 / 132 / 293 ms.
Tokyo überschreitet das Ladeziel; erste Aufbau-Läufe Wien/Tokyo überschreiten p99 16,67 ms.
Entpackte Bytes sind kumulierter Durchsatz, keine SSD-/Residencygröße. Footprint ist Prozesspeak,
kein isolierter GPU-Wert. Logs: System-Temp `outshine-ground-region-2c428fa9c-<Place>-warm.log`;
gleicher Producer belegt den Quellstand. Kein neuer Geräte-/Internetnachweis.
1. P0: den Bedarf aus 2336 vor Quellbeschaffung und Cachedecode anschließen. Hierarchische
   räumliche Rohlinge statt kompletter kameragebundener Snapshots; verdeckte Kinder ungeöffnet.
   Eltern aus Cache, nur fehlende Eltern grob aus DEM; anschließend mögliche sichtbare Kinder
   nachfordern. OSM ebenfalls nah → fern nach Coverage/Bewegungsbedarf; nicht alle Ringkacheln vorab.
2. P0: finale native Höhen-/Kontaktprodukte laden, statt trotz Regionhit 709–971 MiB Felder zu
   entpacken. Mehrere Kandidaten teilen unveränderte Daten; einmal laden/publizieren, dann Scratch frei.
   Proben/Bodenabfrage aus Höhenprodukten; keine flächendeckend expandierte CPU-Dreieckssuppe.
3. P1: Modellprototypen nur für benötigte räumliche Gruppen laden. Ein Atlas kostet
   256² × 8 × 40 Byte = 20 MiB; 31 ergeben 620 MiB und 1,47 s Lesen/Decode in Tokyo.
   Quantisierte Tiefe/Normalen/Material/Coverage vergleichen; 12 Byte/Texel wären 186 MiB,
   eine zu prüfende Formatvariante mit Bild-/Fehlernachweis, keine bereits bewiesene Einsparung.
   Generator-/Codec-/Capture-Abhängigkeiten versionieren; CrownBuildIdentity bindet heute
   sämtlichen src/include-Code und invalidiert Prototypen auch bei fachfremden Änderungen.
4. P1: begrenzte räumliche Pakete, pro Block entpacken/validieren und direkte Produktübernahme
   gegen heutige Paketbuffer → Decoderarrays → native Produkte messen. Kompression allein
   reduziert weder entpackte Arbeit noch GPU-/RAM-Residency. Keine bloße Erhöhung der Paketgrenze.
Ladeziel: warm <10 s, <1 s als Challenge; Koerbersee/Flensburg zuerst reparieren, Straßen erhalten.
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
Outshine überträgt dieses Muster auf prozedurale Runtime-Misses, nicht UEs gesamte Buildarchitektur.
[World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine) /
[HLOD](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine):
räumliche Zellen und grobe Verbandsassets. [Retention](https://dev.epicgames.com/documentation/en-us/unreal-engine/texture-streaming-overview-for-unreal-engine):
Sichtbedarf, Speicherbudget und letzte Nutzung; kein Beleg für feste Detail-TTL-Sekunden.
## Abnahme
Frischer Offline-Prozess lädt vollständige Assets bei Hits ohne Providerdecode, Anreicherung,
Rohling-Neubau. Laufzeit-Nahdetails verwenden nur fertige Rohlinge und werden gezielt erneuert. Kalter Aufbau erzeugt genau einmal; Version-/Inputwechsel gezielt.
Frustum-/Radius-/LOD-Abfragen stimmen gegen vollständige räumliche Referenz; Grenze, leere Region,
Drehung, Bewegung und Wiederstart ohne verlorene Assets/ungeplante Arbeit. Defekte/Teilpakete testen.
Alle Pflicht-Places, besonders Wien/CP/T: Bilder vergleichen, Laden/CPU/GPU/p99/SSD/RAM-Peaks getrennt
bei gleicher Sichtweite/Inhalt/Profil prüfen. Dateiexistenz allein belegt kein Ready.
