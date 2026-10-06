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
Nahdetails entstehen zur Laufzeit daraus; die Basis wird bei Treffern weder ergänzt noch neu gebaut.
content/assets speichert native Paketbytes und Raumindex atomar; Hits umgehen den Miss-Callback.
Der Client prüft native Höhenfelder vor DEM-Decode/Nahtaufbereitung; aktive Felder teilen Speicher.
Versionierte Pakete erhalten alle Samples, Quellherkunft und fehlende Randdaten; Offline-Replay
benötigt keine Quelle; reale Warmhits sparen DEM-Decode. Öffentliche Anbindung und SSD-Budget fehlen.
Gebäuderohlinge enthalten Höhen/Kontakte, native Formen/Dächer, Materialparameter und Seeds.
Ein versionierter Codec speichert/lädt diese Basis über content/assets; der Wiederstarttest
verfeinert ohne Quellen/Höhen dieselbe Geometrie und wiederholt keine Hausformplanung. Das ist
noch ein privater Baustein: öffentlicher Vertrag/Gebäudeanschluss und verformtes Terrain/Infrastruktur fehlen;
Ein source-freier Worker verarbeitet diese Basis in begrenzten Arbeitsblöcken; die echte
Queue muss früh laden und Terrainkontakte ohne erneuten Höhenfelderwerb übernehmen.
Koordinatenkopien, Fernverbände und residente Kosten müssen im echten Ladepfad begrenzt werden.
Warm Wien/CentralPark/Tokyo: 59,22/36,44/69,45 s statt 96,83/83,54/109,95 s; noch weit von <1 s.
Bilder pixelgleich, unveränderte Dreieckzahlen. p99 28,21/10,18/27,51 ms; Wien/Tokyo bleiben rot.
Alle drei: 0 native Misses/Writes und 0 DEM-Decodes. Höhenfeld-Worker 3,14/2,52/3,41 s,
liest aber 1,93/1,81/2,02 GiB Zwischenfelder; drei Szenen belegen rund 5 GiB im Assetcache.
Gebäudearbeit 29,96/13,99/23,47 s; Terrain-Verformung 5,03/2,59/14,58 s. Fertige Gebäude-
und verformte Terrain-/Infrastrukturprodukte vor erneuter Höhenanforderung laden; keine Feldmengen
als effizientes Endformat ausgeben. Zeiten überlappen; Lieferungen beweisen keine eindeutigen Decodes.
Wien hat 49 Nahkacheln, keine belegte vollständige 240-km-Assetwelt. 2336 besitzt Fernabdeckung.
Ziel: vorbereitete Places warm <1 s und wenige ms Draw bei vollständigem, mindestens gleichem Bild.
Quellerwerb/Erstaufbau, Warmstart, erste Einreichung und p99 getrennt messen; AGENTS-Budgets gelten.

## Besitzer und Grenzen
2280 besitzt Asset-Speicherung/Index/Laden; Generatoren besitzen Anreicherung und Asset-Inhalt.
2336 besitzt Hierarchie/LOD und 2188 den öffentlichen generischen Vertrag. Gemeinsame Dienste
kennen Bounds, Versionen und native Produkte, keine OSM-/Vegetationssemantik.
Der neue Ablauf ersetzt ungeeignete Vorbereitung/Verwaltung. Alte Tests/Module sind keine Vorgabe.
Vorhandene Straßenqualität, Terrain-Deformation, Quellbytes und Referenzen bleiben erhalten.

## Fertige Assets
- Gebäude: Typ, vollständige Höhen/Dachparameter, Grundrisse/Parts/Höfe, Terrainkontakt,
  Material-/Fassadenpläne/Seeds, einfache Hüllen und geeignete LOD-/Verbandsprodukte.
  Laibungen, Rahmen, Dachdetails und andere Nahgeometrie dürfen daraus zur Laufzeit entstehen.
- Terrain/Infrastruktur: vorbereitete Höhen/Kontakte, Mesh-/Materialprodukte samt Nachbar-/LOD-
  Anschlüssen. Kein erneuter DEM-/MVT-Decode oder Straßen-/Terrainaufbau bei Assettreffern.
- Vegetation: Bestands-/Instanzdaten, gemeinsame Prototypen mit LODs, Material/Alpha-Mips,
  Windparameter und Fernverbände. 2111 bleibt nach Gebäuden/Terrain/Infrastruktur.
- Gemeinsame/kompakte, deviceunabhängige Renderprodukte speichern; kein Fine-Mesh je Fernhaus,
  keine Kopie desselben Prototyps je Instanz. Licht, Wetterantwort und dynamische Pose bleiben aktuell.
  Rohlinge enthalten alle Quellergänzungen/Parameter/Abhängigkeiten für ihre spätere Verfeinerung.
  Treffer bedeuten Lesen, Entpacken/Upload, Auswahl und budgetierte Nahdetails, keinen Rohling-Neubau.

## Räumlicher Index und Ladeabfragen
1. Gemeinsamer Index unterstützt Frustum und Radius R um Weltposition x,y,z. Konservative Bounds
   liefern Kandidaten; Ebenen-/Abstandstests und LOD wählen tatsächlich benötigte Produkte.
   Abfragen dürfen nicht alle Objekte öffnen/dekodieren. Reine Frustum-Selektion ist kein IO-Befehl
   je Frame: grobe Rundumprodukte und nahe Arbeitsmenge bleiben für schnelle Drehungen resident.
2. Metadaten enthalten stabile Asset-ID, Weltbounds/Anker, Produkt-/Generatorversion, LOD-/Eltern-
   Kindbezug, Qualitäts-/Kostenangaben, Abhängigkeiten und Speicherort/Bytebereich. Assetkeys
   binden Region, Produkt/LOD, Seed/Parameter und Generatorversion, keine reine Blickrichtung.
   Gleiche native Abfrage für Builtins/externe Generatoren; keine Place-Listen oder OSM-Spalten im gemeinsamen Index.
3. Regelmäßige OSM-/DEM-Quellkacheln direkt über Kachel-IDs adressieren. SQLite-R*Tree hält
   native Paketbounds in ECEF; genaue Radius-/Frustumtests folgen konservativer Boxabfrage.
   Geladene Pakete verwenden gepackte Bereiche, keine SQL-Abfrage je Frame.
   [Python-Modell](../test/experiments/asset_residency.py): 100.545 Wiener Polygone → 1.837 Pakete;
   Planarrays 25,17 MB, warmer OS-Dateicache-Read ca. 2,9 ms statt ca. 5,3 s Quellen-Decode.
   Kein nativer Rohling-/GPU-Nachweis. Paketindex gegen Objektindex und lineare Arrays gemessen;
   weite residente Abfragen bevorzugen Arrays, lokale persistente Abfragen den Paket-R*Tree.
   SQLite 3.54.0 und zstd 1.5.7 lokal vorhanden; keine eigene Datenbank/Index-Engine.
   Breite Bounds-Abfrage plus genaue Frustum-/Radiusprüfung; Rundung muss konservativ bleiben.
   Räumlich gebündelte Assetpakete/Bereichslesungen, keine Datei/IO-Anfrage je Haus/Baum.
4. Quelldaten bleiben separat unverändert. Asset-Schlüssel binden Quellstand/Parameter und
   Generator-/Formatversion. Gelieferte und ergänzte Eigenschaften behalten ihre Herkunft.
   Vollständiges Asset samt Index atomar veröffentlichen; abgebrochene/kaputte Produkte sind Misses.
   Explizite Änderungen erneuern betroffene Assets, nicht die gesamte Welt oder unveränderte Quellen.
5. Cachemisses bündeln: identische Anforderungen teilen genau einen Erzeugungsjob; räumliche
   Pakete statt Einzeljobs je Haus/Baum. Job nutzt paralleles begrenztes IO und einen Compute-Worker.
   Nur fehlende angeforderte Produkte erzeugen, nicht vorsorglich alle Fein-LOD-Stufen der Welt.
   Hits/erzeugte Assets nutzen denselben Ladepfad. Abbruch/Rückstau und Fehler bleiben explizit.
   Vorhandene Rohcachetreffer ohne Assets benötigen einmaligen Aufbau; keine Quellen löschen.
   Fehlende Details behalten gültige Eltern bis vollständige Kinder bereitstehen; Fehler explizit.
6. SSD → RAM → GPU haben getrennte Residencybudgets. Räumliche Vorhaltebereiche/Hysterese
   verhindern Flattern; IO/Entpacken blockieren keinen Frame. CPU-Scratch nach Aufbau freigeben.
   Feine Runtime-Details zuerst freigeben, wenn ungenutzt; grobe Rundumprodukte länger halten.
   Sichtbare/interaktions-/schattenrelevante Produkte bleiben gepinnt. TTL ist optionale
   Residency-Hysterese je Detail-/Kostenklasse, keine Qualitätskürzung oder Ablaufzeit gültiger
   Rohlinge. SSD-Assetbudget eviktiert ungenutzte Produkte nach Kosten/Nutzung/Bytes; Quellen
   bleiben unverändert. Weniger Thrashing statt möglichst früher Freigabe optimieren.
   GPU-Ressourcen erst nach letzter Nutzung freigeben; geteilte Bytes nicht mehrfach zählen.

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
2188 bindet denselben Assetbedarf/Missvertrag öffentlich an; keine SDK-Neufassung davor.

## Abnahme
Frischer Offline-Prozess lädt vollständige Assets bei Hits ohne Providerdecode, Anreicherung,
Rohling-Neubau. Laufzeit-Nahdetails verwenden nur fertige Rohlinge und werden gezielt erneuert. Kalter Aufbau erzeugt genau einmal; Version-/Inputwechsel gezielt.
Frustum-/Radius-/LOD-Abfragen stimmen gegen vollständige räumliche Referenz; Grenze, leere Region,
Drehung, Bewegung und Wiederstart ohne verlorene Assets/ungeplante Arbeit. Defekte/Teilpakete testen.
Wien/Central Park/Tokyo: Bilder öffnen/vergleichen, Laden/CPU/GPU/p99/SSD/RAM-Peaks getrennt belegen.
Danach alle Pflicht-Places; Sichtweite/Inhalt/Profil unverändert. Kein Ready allein aus Dateiexistenz.
