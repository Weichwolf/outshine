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
speichern und denselben Lade-/Publikationspfad bedienen. Für alle Weltklassen.
Vorhanden: AssetCache/ResolveAsset, SQLite-R*Tree, native Codecs, Höhenfelder, Gebäude/LOD,
Impostoren. Pakete: Zstandard Level 1; alte Rohpakete/Quellen erhalten, Länge/CRC/Frame prüfen.
Straßennetz-Treffer umgehen Layout/Profilierung; fehlende Höhen ergeben kein Ready.
Regionshits überspringen Boden-/Straßen-/Wassererzeugung und rohe Terrain-Mesh-Jobs.
WaterAsset hält Koordinaten/Pegel/Konturen/Flussprofile/Tileindex. Abfragen, Mesh und Decoder sind quellunabhängig; WaterField besitzt Erzeugung/Fortschritt.
Coverage beobachtet nur; native Produkte bestimmen Ready und veröffentlichte Screenshot-Kennzahlen.
Stabile Regionsanfragen binden Regeln/Form/Bedarf vor Quellen; Inhalts-IDs behalten Digests. Bestehende Pakete per atomarer Metadatenbindung übernehmen.

## Kostenbefund und nächste Lieferung
Frische Offline-Prozesse, 1280×720/60, 60 Frames/360°; Höhensnapshot-Treffer:
| Place | Höhenfelder vorher MiB | Höhenfelder jetzt MiB | Laden s | p99 ms |
|---|---:|---:|---:|---:|
| Wien | 25,01 | 0 | 1,60 | 1,77 |
| CentralPark | 25,01 | 2,00 | 1,40 | 1,92 |
| Tokyo | 32,01 | 2,00 | 2,06 | 1,90 |
Zusätzlich je 0,504 MiB Snapshot. Zehn Regionshits: Peak-RSS bis 1,51 GiB;
Neuaufbau bis 2,42 GiB und 18,67 s. Aktueller OS-Footprint nicht gemessen; GPU-/Gerätenachweis offen.
Gebäude speichern jetzt Schema-4-Metadaten und separat komprimierte Formen je belegter Zelle.
Auswahl öffnet keine Formen; Detail lädt seine Zelle. Misses nutzen Pläne/Kontakte.
Inhaltsschlüssel bleiben stabil; Schema 3 wird atomar nach 4 übernommen, ohne Quellbeschaffung.
Native Gebäude-Basis: Koordinaten, Quell-IDs/Zellen, Origin/Höhenbindung. LOD-Hits ohne Pläne; Geometriemisses öffnen Eltern. Fehlende Pläne invalidieren die Basis. Hierarchie/Arbeitsmenge offen.
[Zellenmodell](../test/experiments/prepared_building_residency.py) begründet Formladung bei Bedarf.
1. P0: Bedarf aus 2336 vor Quellen/Cachedecode; räumliche Eltern/Kinder statt Kamerasnapshots.
   [Liefermodell](../test/experiments/building_asset_delivery.py): Basis hält Koordinaten/Origin/Höhenbindung/Quell-IDs/Zellen; volle Pläne/Auswahl nur bei Miss/Nahdetail.
   Gebäude-Anfragen finden Basis/Inhalts-ID vor Straßen-Digests. Native OSM-Kacheln werden vor
   Provider/MVT auf dem Compute-Worker geladen; Geometrie/Klassifizierung teilen den Pfad.
   Regionsbedarf bindet Regeln, Form, Layout/Kacheladressen und Parameter ohne Quelldigests.
   Teilstände dürfen vollständigen Bedarf nicht treffen. Vorhandene Inhalts-IDs/Pakete werden
   atomar per Metadaten übernommen; alle 41 Pakete unverändert.
   Preload lädt native Klassen vor Feldern und hält Abdeckung über Übergaben. Regionshits sparen globale Straßenfelder; Nahplatzierung nutzt eine Kachel. Basismiss/Bewegung fordert Quellen. Gebäude-/Terrainfelder offen.
2. P0: fertige 257²-Höhensamples (528.416 B) asynchron laden; nur Miss sampelt/speichert. [Modell](../test/experiments/prepared_ground_patch.py): Rastergröße/Präzision. Zehn gleiche Offline-Bildpaare: 25–32 MiB Zusatzlesungen entfallen; CP/Tokyo je 2 MiB Kamerahöhe. Deformierte Kontakte bleiben separat. WaterAsset integriert, Konturen/Löcher erhalten. [Packmodell](../test/experiments/water_asset_coordinates.py): 9–286 kB statt 0,19–60 MB. Weitere Produkte nach 2188 trennen.
3. P0: bestätigten Tokyo-Anstieg und verbliebene W/CP-Footprint-Regressionsursache zuordnen/beseitigen: Decoder-Scratch, Allocator-/Treiberreserven und Upload-Lebensdauer. Gemeldete Puffer erklären den OS-Footprint nicht vollständig.
   Renderer teilt Maps/Material ([Modell](../test/experiments/material_image_residency.py)). Native Bilder halten Basis und optionale Linear-/sRGB-/Normalmips; Producer bereitet vor Publikation vor, Hits uploaden direkt. Eigene Produkt-/Codecversion; bildlose Assets und Captures bleiben gültig.
4. P1: [Paketmodell](../test/experiments/impostor_ready_payload.py): Capture 20 MiB, Flat-Karten 6 MiB; [Mip-Modell](../test/experiments/prepared_image_mips.py): +2 MiB untere Stufen, keine Basisduplikation.
   Native GeometryAsset-Karten umgehen Coverage-/Farbvorbereitung; Atlas-Rohlinge bleiben. Miss/Defekt repariert nur Karten; ca. −60 % entpackte Bytes, kein RAM-/Framegewinnbeweis.
   Atlas-Rezept bindet Artdefinition, Größe/Blicke und getrennte Generator-/Capture-Versionen; Änderungen an Wachstum oder Capture-Semantik erhöhen die jeweilige Version. Codecs prüfen eigene Formate. Fachfremde Engine-/Buildänderungen invalidieren nicht.
5. P1: SSD automatisch budgetiert verdrängen; aktive Assets, Paketmitglieder, Eltern und Quellen erhalten. Nutzdaten bleiben opaque. Warm <10 s; Gebäudebedarf vor Decode.
Lookup-Abschlüsse unabhängig von erneuten Anfragen abholen; veraltete Parameter dürfen
kein IO-Fenster blockieren. Teilweise gelieferte Höhen bis zur vollständigen Eingabe halten;
nur Blöcke der aktuellen Terrainrevision behalten. Abgewiesene Blöcke dürfen spätere gültige
Lieferungen derselben Revision nicht blockieren.
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
   Kindbezug, Qualitäts-/Kostenangaben, Abhängigkeiten und Speicherort/Bytebereich. Eine optionale Anfrage-ID bindet Generator-/Quellkonfiguration, Region/LOD und Parameter vor Quellenarbeit; ein eindeutiger SQLite-Index adressiert ihr fertiges Produkt. Inhalts-ID/Herkunft binden weiter die tatsächlichen Eingaben. Publikation aktualisiert die Bindung atomar; kein automatisches Refresh und keine Kamerasnapshot-ID.
   Schema 3 ergänzt optionale Anfragebindungen ohne Payload-Neubau; Schema 1/2 erhalten. [Indexmodell](../test/experiments/native_request_index.py): Metadatenspalte statt separater Bindungstabelle; Builtins/Erweiterungen teilen ResolveAssetRequest.
3. Regelmäßige OSM-/DEM-Quellkacheln direkt über Kachel-IDs adressieren. SQLite-R*Tree hält
   Paketbounds in ECEF; Radius-/Frustumtests folgen konservativer Boxabfrage.
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
Wasser, Gebäude-LOD und native OSM-Kacheln umgehen Providerdecode; Regionseinstieg/übrige Zwischenfelder offen.
Miss-/Hit-Abweichungen in CP/Wien besitzt 2340. Peak-RSS und OS-Footprint getrennt messen; Speicherbedarf bleibt zu senken.
Frischer Offline-Prozess lädt vollständige Assets bei Hits ohne Providerdecode, Anreicherung,
Rohling-Neubau. Laufzeit-Nahdetails verwenden nur fertige Rohlinge und werden gezielt erneuert. Kalter Aufbau erzeugt genau einmal; Version-/Inputwechsel gezielt.
Räumliche/LOD-Abfragen gegen vollständige Referenz; Grenze, leere Region, Drehung, Bewegung, Wiederstart
ohne verlorene Assets/ungeplante Arbeit prüfen. Defekte/Teilpakete testen.
Alle Pflicht-Places, besonders Wien/CP/T: Bilder vergleichen, Laden/CPU/GPU/p99/SSD/RAM-Peaks getrennt
Gleiche Sichtweite/Inhalt/Profil prüfen. Dateiexistenz allein belegt kein Ready.
