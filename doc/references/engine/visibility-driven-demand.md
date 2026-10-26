# Hierarchischer Bedarf vor Download und Geometrie

Codeprüfung: 2026-10-07. Besitzer: WI [#12](https://github.com/Weichwolf/outshine/issues/12) (Bedarf), [#10](https://github.com/Weichwolf/outshine/issues/10) (Assets), [#9](https://github.com/Weichwolf/outshine/issues/9) (API).
MapLibre und CesiumJS wurden im Quellcode geprüft; Cesium Native ergänzt eine
Originaldokumentationsprüfung vom 2026-10-26. Keine lokalen Laufzeitbenchmarks dieser Projekte.
Keine Aussage über deren konkreten Durchsatz auf dieser Maschine oder A18 Pro.

## Geprüfte Vorbilder

| Vorbild | Verfahren | Übertragung und Grenze |
|---|---|---|
| MapLibre GL JS | Hierarchische Kachelauswahl, Frustum-/Höhenbounds, entfernungsabhängiger Zoom; Ergebnis nah zuerst | MVT-Bedarf vor Requests. Keine geprüfte Gebäude-/Bergverdeckung vor API-IO |
| MapLibre TileManager | Gewünschte Kacheln anfordern, geladene Kinder/Eltern als Ersatz behalten, unbenötigte Arbeit abbrechen | Gültige Eltern bis Kinder bereit sind; kein Renderloch bei Miss |
| CesiumJS Quadtree | Sichtprüfung vor Verfeinerung; Screen-Space-Error, priorisierte Ladequeues, Ersatz durch vorhandene Stufen | Gemeinsamer Bedarf für grobe Eltern und gezielte Kinder; Interaktionsbedarf separat |
| CesiumJS Globe | Frustum und Ellipsoidhorizont schließen Kacheln aus | Erdkrümmung berücksichtigen; Ellipsoidprüfung ersetzt keine Verdeckung durch Berge |
| GigaVoxels | Strahlen durch residente Hierarchie melden fehlende benötigte Knoten; kompaktierte Anfragen, gröbere Ersatzdaten | Iterative Sicht-/LOD-Rückmeldung übernehmen; keine HTTP-Anfrage je Pixel, keine Pflicht zur Voxelwelt |

MapLibre-Code, Commit `537c4b145e71925121404a208164e685d792cae1`:
[covering_tiles.ts](https://github.com/maplibre/maplibre-gl-js/blob/537c4b145e71925121404a208164e685d792cae1/src/geo/projection/covering_tiles.ts)
(`isTileVisible`, Stack-Verfeinerung, Sortierung nach `distanceSq`),
[tile_manager.ts](https://github.com/maplibre/maplibre-gl-js/blob/537c4b145e71925121404a208164e685d792cae1/src/tile/tile_manager.ts)
(`update`, `_updateRetainedTiles`, `_removeTile`).

CesiumJS-Code, Commit `a3aea90b28d913faa3dd41454c5ab40f7e912459`:
[QuadtreePrimitive.js](https://github.com/CesiumGS/cesium/blob/a3aea90b28d913faa3dd41454c5ab40f7e912459/packages/engine/Source/Scene/QuadtreePrimitive.js)
(`visitTile`, `visitIfVisible`, `screenSpaceError`),
[GlobeSurfaceTileProvider.js](https://github.com/CesiumGS/cesium/blob/a3aea90b28d913faa3dd41454c5ab40f7e912459/packages/engine/Source/Scene/GlobeSurfaceTileProvider.js)
(`computeTileVisibility`). SSE dort: geometrischer Fehler × Bildhöhe / (Distanz × Projektionsfaktor).
Der Ladepfad unterscheidet außerdem verdeckte, für Kamerahöhe/Referenzposition benötigte Kacheln.

**GigaVoxels: Ray-Guided Streaming for Efficient and Detailed Voxel Rendering**,
Crassin / Neyret / Lefebvre / Eisemann, I3D 2009.
[Autoren-PDF](https://www-sop.inria.fr/reves/Basilic/2009/CNLE09/CNLE09.pdf) ·
[Lokales PDF](../geometry/i3d/2009-gigavoxels-ray-guided-streaming.pdf).
Das Paper begrenzt Verfeinerungsanforderungen pro Strahl; neue Daten können im Folgeframe
weitere Traversierung durch Verdeckung verhindern. Es belegt keinen schnellen OSM-API-Bootstrap.

## Cesium Native Selection

Getrennte [Originaldokumentation 0.65.0](https://cesium.com/learn/cesium-native/ref-doc/selection-algorithm-details.html),
[fixierter Originaltext](https://github.com/CesiumGS/cesium-native/blob/1bca8021711294cc83a5a032599ac72d9b3166c9/doc/topics/selection-algorithm-details.md).
BVH-/SSE-Traversierung wählt vor Content-IO. `AncestorMeetsSSE` hält bereits gezeigte
Kinder, solange der gröbere Ersatz fehlt; `Kicking` behält Ersatzabdeckung beim
Verfeinern. Die Quelle wartet nicht pauschal vor jeder Traversierung auf alle Kinder.
Die Original-GIFs zeigen Detailverlust beim Herauszoomen ohne ersteren Mechanismus
und erhaltene Abdeckung mit ihm; optisch geprüft, Belege in
[#1225](https://github.com/Weichwolf/outshine/issues/1225#issuecomment-6410933180).

Standardauswahl kann neu aufgedeckte Bereiche leer lassen. `forbidHoles` bezahlt
zusätzliche Ladungen für Abdeckung, garantiert aber keine überall erreichte Zielqualität.
Geometrische SSE ersetzt weder Material-/Schattenfehler noch Funktionsbedarf.
Logische Ladejobs umfassen IO/Decode; daraus keine blockierten CPU-Slots übernehmen.
Native bleibt Verfahrensreferenz, ohne 3D-Tiles-/glTF-Produkteinführung.

## Entscheidung für Outshine

1. Stabile native Eltern mit Bounds/Qualität aus räumlichem Assetindex laden; bei Miss zuerst
   grobe Quelldaten. Kein feinster Nebenpfad für Kontakte, Gebäudehöhen oder Klassifikation.
2. Rundum-Hierarchie auf einem Worker nah → fern auswerten. Unbekannte Höhen/Coverage
   bleiben unbekannt. Grobe MVT ohne Gebäude bedeutet nicht, dass die Stadt leer ist.
3. Nur möglicherweise wirksame Kinder anfordern. Gleiche Anfragen bündeln, IO parallel begrenzen;
   Eltern behalten. Bewegung, Schatten, Reflexionen und aktive Interaktion ergänzen den Bildbedarf.
4. CPU-Horizont/Quadtree gegen niedrig aufgelöste Tiefen-/Cubemap-Auswahl messen.
   Native Gebäude-Coverage verwenden; Cluster-AABBs sind keine opaken Hauswände.
   GPU-Feedback wäre verzögert/asynchron, ohne blockierenden Readback im Frame.
5. Erst ausgewählte Produkte dekodieren/erzeugen. Fine-Daten hinter dem Berg bleiben ungeholt,
   verdeckte vorhandene Rohlinge auf SSD. MVT-Kachelgranularität setzt eine Untergrenze für Quellbytes.

Kein neuer Anbieter und kein zweiter Renderer. Übernommen werden Bedarfs-/Fallback-Verfahren.
Abnahme: gleiche Rundum-Silhouette und Bewegung ohne Löcher, weniger Quell-/Assetbytes und
CPU-Geometrie. Anfragen vor IO zählen; spätes GPU-Culling beweist keine vermiedenen Downloads.
