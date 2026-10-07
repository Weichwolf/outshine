# Hierarchischer Bedarf vor Download und Geometrie

Codeprüfung: 2026-10-07. Besitzer: WI 2336 (Bedarf), 2280 (Assets), 2188 (API).
MapLibre und Cesium wurden im Quellcode geprüft, nicht als Tokyo-Laufzeitbenchmark ausgeführt.
Keine Aussage über deren konkreten Durchsatz auf dieser Maschine oder A18 Pro.

## Geprüfte Vorbilder

| Vorbild | Verfahren | Übertragung und Grenze |
|---|---|---|
| MapLibre GL JS | Hierarchische Kachelauswahl, Frustum-/Höhenbounds, entfernungsabhängiger Zoom; Ergebnis nah zuerst | MVT-Bedarf vor Requests. Keine geprüfte Gebäude-/Bergverdeckung vor API-IO |
| MapLibre TileManager | Gewünschte Kacheln anfordern, geladene Kinder/Eltern als Ersatz behalten, unbenötigte Arbeit abbrechen | Gültige Eltern bis Kinder bereit sind; kein Renderloch bei Miss |
| Cesium Quadtree | Sichtprüfung vor Verfeinerung; Screen-Space-Error, priorisierte Ladequeues, Ersatz durch vorhandene Stufen | Gemeinsamer Bedarf für grobe Eltern und gezielte Kinder; Interaktionsbedarf separat |
| Cesium Globe | Frustum und Ellipsoidhorizont schließen Kacheln aus | Erdkrümmung berücksichtigen; Ellipsoidprüfung ersetzt keine Verdeckung durch Berge |
| GigaVoxels | Strahlen durch residente Hierarchie melden fehlende benötigte Knoten; kompaktierte Anfragen, gröbere Ersatzdaten | Iterative Sicht-/LOD-Rückmeldung übernehmen; keine HTTP-Anfrage je Pixel, keine Pflicht zur Voxelwelt |

MapLibre-Code, Commit `537c4b145e71925121404a208164e685d792cae1`:
[covering_tiles.ts](https://github.com/maplibre/maplibre-gl-js/blob/537c4b145e71925121404a208164e685d792cae1/src/geo/projection/covering_tiles.ts)
(`isTileVisible`, Stack-Verfeinerung, Sortierung nach `distanceSq`),
[tile_manager.ts](https://github.com/maplibre/maplibre-gl-js/blob/537c4b145e71925121404a208164e685d792cae1/src/tile/tile_manager.ts)
(`update`, `_updateRetainedTiles`, `_removeTile`).

Cesium-Code, Commit `a3aea90b28d913faa3dd41454c5ab40f7e912459`:
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

## Entscheidung für Outshine

1. Stabile native Eltern mit Bounds/Qualität aus räumlichem Assetindex laden; bei Miss zuerst
   grobe Quelldaten. Kein feinster Nebenpfad für Kontakte, Gebäudehöhen oder Klassifikation.
2. Rundum-Hierarchie auf einem Worker nah → fern auswerten. Unbekannte Höhen/Coverage
   bleiben unbekannt. Grobe MVT ohne Gebäude bedeutet nicht, dass die Stadt leer ist.
3. Nur möglicherweise wirksame Kinder anfordern. Gleiche Anfragen bündeln, IO parallel begrenzen;
   Eltern behalten. Bewegung, Schatten, Reflexionen und aktive Interaktion ergänzen den Bildbedarf.
4. Für Koerbersee CPU-Horizont/Quadtree gegen niedrig aufgelöste Tiefen-/Cubemap-Auswahl messen.
   Für Tokyo native Gebäude-Coverage verwenden; Cluster-AABBs sind keine opaken Hauswände.
   GPU-Feedback wäre verzögert/asynchron, ohne blockierenden Readback im Frame.
5. Erst ausgewählte Produkte dekodieren/erzeugen. Fine-Daten hinter dem Berg bleiben ungeholt,
   verdeckte vorhandene Rohlinge auf SSD. MVT-Kachelgranularität setzt eine Untergrenze für Quellbytes.

Kein neuer Anbieter und kein zweiter Renderer. Übernommen werden Bedarfs-/Fallback-Verfahren.
Abnahme: gleiche Rundum-Silhouette und Bewegung ohne Löcher, weniger Quell-/Assetbytes und
CPU-Geometrie. Anfragen vor IO zählen; spätes GPU-Culling beweist keine vermiedenen Downloads.
