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
Engine-Assetbedarf → räumlicher Cacheindex → Hit: fertiges Asset laden → RAM/GPU.
Miss → Generator → Provider/Quellcache/API nach Bedarf → vollständige Anreicherung/Asset-Erzeugung
→ Asset/Index atomar speichern → denselben Lade-/Publikationspfad bedienen.
Das gilt für Gebäude, Terrain, Straßen/Wasser, Vegetation und andere generierte Inhalte.
Ein Treffer enthält vollständig angereicherte Asset-Rohlinge, keine unvollständigen Quellen.
content/assets speichert native Paketbytes und Raumindex atomar; Hits umgehen den Miss-Callback.
Native Höhenfelder umgehen DEM-Decode/Nahtaufbereitung; aktive Felder teilen Speicher.
Pakete erhalten Samples/Herkunft/Randlücken; Offline-Replay braucht keine Quelle.
Gebäuderohlinge enthalten Höhen/Kontakte, native Formen/Dächer, Materialparameter und Seeds.
Der MVT-Client lädt sie asynchron vor Höhenanforderung/Grundrissextraktion; Misses erzeugen,
speichern und laden dieselbe vollständige Basis. Herkunft/Signatur/Qualifikation bleiben erhalten.
Ausgewählte LOD-Geometrie wird als Kindprodukt gespeichert; gleiche Position/Projektionsparameter
laden sie ohne erneute Planung/Emission. Drehung erzeugt keinen neuen Schlüssel. Bewegung und
Nahdetails brauchen weiterhin 2336; der private Anschluss ersetzt noch nicht den öffentlichen Vertrag.
Native Terrainprodukte speichern verformte Höhen/Kontakte in Paketen ≤64 MiB; große Regionen
veröffentlichen ihren Verbund erst nach allen Paketen. Laden prüft das Residencybudget des Aufrufers.
Ein Compute-Worker lädt/erzeugt sie; Abbruch blockiert den Frame nicht und behält
Job-Eingaben bis zum Workerabschluss. Misses speichern und laden denselben nativen Pfad.
Schlüssel binden Höhen, vollständige Kontakte/Physikrahmen und Version; Scratch bleibt begrenzt.
Frische Offline-Treffer (765d3d189, 720p60): Wien 25,26 s, Central Park 20,68 s, Tokyo 27,02 s.
Je zwei Terrainprodukte: 34,25/21,61/35,32 MiB; keine Terrain-Deformations-Misses/Writes.
Koerbersee warm: 11,56 s, 152,82 MiB aus zwei Terrainverbünden, keine Misses/Writes.
Alle zehn Places: je 50 Gebäude-/LOD-Hits; keine Basis-/LOD-Emission, Bilder pixelgleich zum Vorgänger.
p99 Wien/CP/Tokyo: 27,09/11,50/33,08 ms; Wien, Tokyo und Koerbersee (19,13 ms) über 720p60.
Unter 1 s bleibt unerreicht: 1,98/1,81/2,03 GiB Terrain-Zwischenfelder gelesen; Kontakte,
Straßen und Terrain-/Wassermeshes werden noch aufgebaut. Die späte Terrain-Assetabfrage
umgeht die eigentliche Verformung, aber noch nicht deren komplette Eingabevorbereitung.
Prozesspeaks 4,21/3,88/5,37 GiB sind kein akzeptierter Speicherbedarf.
SSD-Payload 17,93 GiB: 15,64 GiB (87 %) Terrain-Zwischenfelder; keine begründete Residency.
Diagnose zählt fertige Kontakte bei Besitzänderungen; Tokyo-Earthworks 12,54 → 0,46 s.
Preload pausiert nicht nach tatsächlich fortgeschrittener Wasser-/Straßenaufnahme; große
Ladeeinsparung dadurch unbewiesen. Idle bleibt bei Wien/CP/Tokyo 11,30/8,08/11,06 s: Ursache offen.
Nächste Integration: fertige Terrain-/Straßen-/Wasserprodukte vor Zwischenfeldern laden;
keine erneute Kontakt-/Straßenformung bei Treffern. Bis vollständig sichtbare Welt messen,
keine Gebäudeteilzeit als Weltladezeit ausgeben. Terrain-/Straßenqualität erhalten.
Öffentliche Anbindung, Original-OSM-/weitere Generatorpfade, SSD-Budget und kompaktere Produkte
fehlen. Worker-/Phasenzeiten überlappen.
Wien: 49 Nahkacheln; vollständige 240-km-Assetabdeckung bleibt in 2336 unbewiesen.
Ziel: vorbereitete Places warm <1 s und wenige ms Draw bei vollständigem, mindestens gleichem Bild.

## Besitzer und Grenzen
2280 besitzt Asset-Speicherung/Index/Laden; Generatoren besitzen Anreicherung und Asset-Inhalt.
2336 besitzt Hierarchie/LOD und 2188 den öffentlichen generischen Vertrag. Gemeinsame Dienste
kennen Bounds, Versionen und native Produkte, keine OSM-/Vegetationssemantik.

## Fertige Assets
- Gebäude: Typ, vollständige Höhen/Dachparameter, Grundrisse/Parts/Höfe, Terrainkontakt,
  Material-/Fassadenpläne/Seeds, einfache Hüllen und geeignete LOD-/Verbandsprodukte.
  Laibungen, Rahmen, Dachdetails und andere Nahgeometrie dürfen daraus zur Laufzeit entstehen.
- Terrain/Infrastruktur: vorbereitete Höhen/Kontakte, Mesh-/Materialprodukte samt Nachbar-/LOD-
  Anschlüssen. Kein erneuter DEM-/MVT-Decode oder Straßen-/Terrainaufbau bei Assettreffern.
- Vegetation: Bestands-/Instanzdaten, gemeinsame Prototypen mit LODs, Material/Alpha-Mips,
  Windparameter und Fernverbände. 2111 bleibt nach Gebäuden/Terrain/Infrastruktur.
- Kompakte, deviceunabhängige Renderprodukte; kein Fine-Mesh je Fernhaus/Prototypkopie je Instanz. Licht, Wetterantwort und dynamische Pose bleiben aktuell.

## Räumlicher Index und Ladeabfragen
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
   [Python-Modell](../test/experiments/asset_residency.py): 100.545 Wiener Polygone → 1.837 Pakete;
   Planarrays 25,17 MB, warmer OS-Dateicache-Read ca. 2,9 ms statt ca. 5,3 s Quellen-Decode.
   Kein nativer Rohling-/GPU-Nachweis. Paketindex gegen Objektindex und lineare Arrays gemessen;
   weite residente Abfragen bevorzugen Arrays, lokale persistente Abfragen den Paket-R*Tree.
   SQLite 3.54.0, zstd 1.5.7 lokal; Bounds-Abfrage plus konservative Frustum-/Radiusprüfung.
   Räumlich gebündelte Assetpakete/Bereichslesungen, keine Datei/IO-Anfrage je Haus/Baum.
4. Quelldaten bleiben separat unverändert. Asset-Schlüssel binden Quellstand/Parameter und
   Generator-/Formatversion. Gelieferte und ergänzte Eigenschaften behalten ihre Herkunft.
   Vollständiges Asset samt Index atomar veröffentlichen; abgebrochene/kaputte Produkte sind Misses.
   Explizite Änderungen erneuern betroffene Assets, nicht die gesamte Welt oder unveränderte Quellen.
5. Cachemisses bündeln: identische Anforderungen teilen genau einen Erzeugungsjob; räumliche
   Pakete statt Einzeljobs je Haus/Baum. Job nutzt paralleles begrenztes IO und einen Compute-Worker.
   Nur fehlende benötigte Produkte erzeugen; Hits/Misses teilen den Ladepfad, Abbruch/Rückstau/Fehler explizit.
   Fehlende Details behalten gültige Eltern bis vollständige Kinder bereitstehen; Fehler explizit.
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
[Systembibliotheken](../doc/dependencies.md), [Recherche](../doc/references/README.md).
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
Wien/Central Park/Tokyo: Bilder öffnen/vergleichen, Laden/CPU/GPU/p99/SSD/RAM-Peaks getrennt belegen.
Danach alle Pflicht-Places; Sichtweite/Inhalt/Profil unverändert. Kein Ready allein aus Dateiexistenz.
