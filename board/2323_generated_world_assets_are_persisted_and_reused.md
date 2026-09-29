Type: feature
State: active
Priority: P0
Architecture: ready
Parent: 2092
Depends:
Area: generators, content, world, engine
Tags: cache, loading, generated-assets

# Generated world assets survive process restart

## Ergebnis

Eine bereits erzeugte Stadt lädt native Assets aus dem Cache. Gebäude, Straßen,
Terrain-Deformation und Materialaufbereitung werden bei gleichen Eingaben nicht erneut
berechnet. Wien bleibt vollständig sichtbar; Kaltaufbau, warmer Prozessstart, GPU-Upload
und einzelne Frames erhalten getrennte Kosten. Ziel: warmes Weltladen in ein bis zwei
Sekunden, Darstellung innerhalb 1000/60 ms. Die 120 Messframes bleiben erhalten.

## Unveränderliche Qualität

Cache und Streaming erhalten optische und funktionale Qualität der OSM-Infrastruktur.
Keine fehlenden Gebäude, unterbrochenen Verkehrsnetze, verlorenen Tags oder reduzierten
Sichtweiten als Kostenoptimierung. Die konfigurierte Standardsichtweite beträgt 240 km
(`kSightUnsaidM`); dichte und dünne Regionen erfüllen denselben Sichtweitenvertrag.
LOD und räumliche Bündelung begrenzen Arbeit bei erhaltener Weltabdeckung und Funktion.
CentralPark, Shibuya und Wien sind ausdrücklich gegen frühere funktionierende Git-Stände
zu prüfen. Die berichtete Echtzeitregression bleibt bis zur gemessenen Wiederherstellung
offen; identische Cache-Bilder allein beweisen keine ausreichende Ausgangsqualität.

## Audit des vorhandenen Pfads

| Erzeuger / Besitzer | Wiederverwendung heute | Fehlende Integration |
|---|---|---|
| Registrierte `Generator::make`, `engine/Declaring.cpp` | Keine Ergebnisablage; Redeclaration ruft Producer erneut auf | Versions- und Abhängigkeitsidentität, native Produktablage und Wiederaufnahme |
| Gebäude, `StructureBake`, `StructureBuildQueue` | Versionierte Source-/Zell-/LOD-Artefakte und asynchroner Runtime-Lookup implementiert | Prozessneustart, vollständige Places und Kosten noch abnehmen |
| Terrain, `HeightSheets`, `TerrainRefinementJob`, `TerrainPressJob` | Quelldaten und prozesslokale Felder | Finale verformte Geometrie, Seiten und wiederverwendbare abgeleitete Produkte |
| Straßen, `Corridors`, `RoadMesher`, `RoadSurfaceBuilder` | Kandidaten und residente Welt | Netzabhängige Profile, Kreuzungen, Meshes und Kontaktprodukte |
| Wasser, `WaterSurfaceBuilder`, `WaterDepth` | Kandidaten und residente Welt | Quellengebundene Wassergeometrie und statische Tiefenprodukte |
| Vegetation, `TreePrototype`, `TreeMesher`, `Forest` | Prototypen im Prozess; persistente Impostor-Atlanten | Baumgeometrie, LODs und deterministische Platzierung |
| `ImpostorCache` / `VegetationStreaming` | Persistentes Encode/Decode und asynchrones Lesen vorhanden | Schreibfehler erreichen über PreparedError_ bereits die Runtime; übrige Produkte fehlen |

`ContentStore` hält vor allem Provider-Bytes. Das ist kein Cache generierter Welt-Assets.
Der öffentliche Vertrag schließt seit `45370314d` einen Ergebnis-Cache ausdrücklich aus:
Die damalige Reparatur verhindert veraltete Geometrie bei geänderten Provider-Daten.
Diese Korrektheit erhalten; Wiederverwendung braucht vollständige Identität statt bloß
vergleichbarer Szenarioparameter. Kein allgemeiner `cachable`-Vertrag ist implementiert.

## Architektur und Implementierung

1. `world/data` besitzt begrenzte persistente Artefaktablage über `ContentStore`.
   `content` besitzt native Geometrie-Codecs; der Gebäudeprodukt-Codec bleibt beim
   erzeugenden Modul, ohne umgekehrte Abhängigkeit von `content` auf Generatoren.
   `engine/streaming` orchestriert Lookup, begrenztes IO, Decode, Miss-Generierung,
   atomisches Publish und Runtime-Upload. Quelldaten und abgeleitete Produkte erhalten
   getrennte Namensräume und Verdrängungsbudgets; abgeleitete Produkte dürfen nicht die
   für ihren Wiederaufbau nötigen Quelldaten verdrängen. Kein synchrones Datei-IO im Framepfad.
   `world/data/ArtifactStore` speichert jedes vollständige Produkt als atomisch ersetztes
   Containerfile: gestreamte Blöcke, Prüfsummen und abschließendes Manifest. Ein geöffneter
   Reader hält denselben Dateisnapshot bis zum Ende. LRU verdrängt ganze Produkte;
   erfolgreiche Zugriffe aktualisieren die Nutzung. Keine produktübergreifenden Blockdateien
   und kein dauerhaft wachsender Restgraph. Bytebudget und begrenzte IO-Puffer bleiben bestehen.
2. Schlüssel enthält Produktart, Producer-/Codec-Version, vollständige Parameter und Seed,
   OSM-/DEM-Inhaltsidentität, relevante Material-/Regelversionen, räumliche Zelle und LOD.
   Abhängigkeiten vor Lookup schließen. OSM-Tagänderung muss betroffene Produkte erneuern.
   Explizite LOD-Produkte unabhängig von Kamera erzeugen; verbleibende echte
   Kameraabhängigkeiten deklarieren. Prozesszähler und Pointer sind keine Cache-Identität.
3. Native Artefakte enthalten vollständige Produktdaten und lokale Ressourcenreferenzen.
   Keine GPU-Handles, Borrowed-Spans oder laufzeitspezifischen Terrain-Zertifikate speichern.
   Quellen beim Laden validieren und aktuelle Nachweise binden. Alle Größen, Indizes,
   Checksummen, Versionen und Ressourcenbezüge vor Publikation prüfen. Große Produkte
   in begrenzte inhaltsadressierte Blöcke zerlegen; ein erst nach vollständigem Publish
   sichtbares Manifest verbindet sie. Die bisherige 64-MiB-Grenze pro Gesamtprodukt
   scheitert an Wien und ist kein belastbarer Asset-Vertrag. Stückweise kodieren und
   dekodieren, statt zusätzlich zum residenten Produkt eine vollständige Bytekopie zu halten.
4. Erster Durchstich: Gebäude-Source- und Zell-LOD-Produkte über `StructureBuildQueue`
   lesen/schreiben; notwendige Geometrie-, Material- und Clusterdaten vollständig erhalten.
   Gleiches Eingabemanifest im zweiten Client-Prozess muss ohne erneuten Structure-Bake
   dasselbe vollständige Place-Bild erzeugen. Dann Terrain-/Straßen-/Wasserprodukte über
   denselben Speichervertrag integrieren, registrierte Generatoren und Vegetation ergänzen.
   `StructureBuildTask` erhält Lookup-/Bake-/Publish-Phasen über getrennte IO-/Compute-Queues.
   `BakedTile` umfasst `Raised`, Cluster, Footprints und Oberflächenfehler, nicht nur Meshes.
   `RawTile` und die tatsächlich verwendeten HeightField-Raster bestimmen die Eingaben;
   `SourceFirst` und AnchorEcef sind derzeit kontextabhängig und müssen beim Laden korrekt
   zugeordnet werden. Keine prozesslokalen SourceKeys als persistente Identität verwenden.
   Explizites RequestedDetail umgeht bereits die kameraabhängige LOD-Wahl im Bake;
   adaptive Altprodukte brauchen dagegen Eye/FocalPx im Manifest bis zu ihrer Ablösung.
5. Registry-Producer brauchen explizite Producer-Version und vollständige deklarierte
   Abhängigkeiten. Nicht identifizierbare Eingaben sind ein Vertragsfehler; keine stillen
   dauerhaften Bypässe. Dynamischer Simulationszustand ist kein generiertes Asset;
   seine statischen Vorlagen und vorberechneten Produkte sind dennoch zu cachen.
6. `TilePool` trennt gehaltene Cache-Ergebnisse von noch nicht übernommenen
   Fertigmeldungen. `AwaitLanding` wartet auf unübernommene Ergebnisse; ein bereits
   übernommenes, weiterhin gecachtes Feld darf die Preload-Schleife nicht erneut wecken.
   Publikation, Übernahme und Verdrängung pflegen diesen Zustand unter `QueueMutex_`.
   Vor dem Warten fertiggestellte Ergebnisse bleiben sofort beobachtbar.
   `GroundStack` beobachtet zusätzlich eine Publikationsrevision pro wartendem Besitzer:
   ein liegen gebliebenes Ergebnis ist kein erneuter Fortschritt. Die Condition Variable
   prüft Revision und Abbruch unter demselben Mutex; neue Publikationen dürfen nicht verloren gehen.
7. Jeder erfolgreiche Asset-Producer publiziert sein Ergebnis über diesen Pfad. Fehler,
   Abbruch und unvollständige Produkte werden nicht als Treffer gespeichert. Schreibfehler
   sichtbar melden; keine Cache-Erfolge behaupten. Ein beschädigter Eintrag wird als Miss
   neu aufgebaut. Speicher- und Plattenbudgets begrenzen Residency und Eviktion.

## Fertig-Kriterien

- Alle oben genannten Producer haben einen nachgewiesenen Lookup-/Publish-Pfad.
- Zwei getrennte Client-Prozesse: zweiter Lauf nutzt Assets statt erneutem Bake;
  CPU-/GPU-/IO-Zeiten, generierte/geladene Bytes und Bilder belegen das Verhalten.
- Geänderte Quelle, Tags, Seed, LOD oder Producer-Version erneuern betroffene Assets;
  geänderte Kameraposition allein zerstört keine kameraunabhängige Wiederverwendung.
- Beschädigte/abgebrochene Einträge und geänderte Provenienz können keine alte Welt als
  gültig ausgeben. Alle Places öffnen; vollständige Welt und Bildqualität bleiben Pflicht.
- `make format`, fokussierte Cache-/Generator-/Runtime-Suites, Places und `make lint`.
