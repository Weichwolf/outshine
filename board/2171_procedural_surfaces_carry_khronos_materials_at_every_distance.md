Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: generators, render, world
Tags: materials, filtering, visual

# Materials have metric detail and coherent light response

## Ergebnis und Ist
Lesbarer Asphalt/Putz/Backstein/Beton/Fels/Glas/Dach mit Maßstab, Relief, Rauheit und
Alterung. Native Metallic-Roughness/UVs/Materialbindung und GroundMaterials-Parameter
bestehen; tatsächliche Places sind flach/repetitiv. Keine neue Materialwelt erforderlich.

## Besitzer und nächste Lieferung
GroundMaterials/native Materialparameter besitzen Semantik/Einheiten; BuildingMesh/
FacadeUv Geometrie/Koordinaten. Surfacing/material.glsl/facadePattern besitzen Auswertung.
Zuerst Asphalt/Wand/Dach in einem bebauten Place sichtbar unterscheiden; vorhandene
Parameter anschließen. 2173 liefert Öffnungsgeometrie, 2337 Felsformen, 2172 Wetterzustand.
Die vorhandenen Inputs erlauben den ersten Schritt ohne fehlenden externen Vertrag.

## Verfahren und Invarianten
- Metrische Fassaden-/Dachkoordinaten: versetztes Backstein-/Ziegelraster mit gefilterten
  Fugen, Putzkorn/Betonporen; Cell-Hash variiert Elemente, Objektseed/Exposition/Ablauf
  Alterung. Keine gleich große Zufallstextur auf jeder Wand oder Place-Fototextur.
- Höhenableitungen liefern Bumpnormalen; größere Vertiefungen echte Geometrie. Pixel-
  Derivate/Footprint filtern Frequenzen; Normalvarianz verbreitert spekulare Antwort.
  Gemeinsame Tangenten/Flächennormalen, Farbräume und Khronos-BRDF-Konventionen erhalten.
- Glas: Fresnel, Roughness-gefilterte Welt-/Himmelsreflexion, energiegeteilte Transmission;
  begrenztes Box-Interior-Mapping plausibel, kein Ersatz für betretbare Räume. Fernfenster
  werden stabile Material-/Nachtlichtbeiträge. Wasserform/-material besitzt 2145.
- Parameter/Seeds einmal vorbereiten und geschlossen publizieren. Shaderarbeit nur für
  sichtbare Varianten; keine Texturerzeugung je Frame, redundanten Meshdaten oder Diskbakes.
- Nässe/Schnee ändern dieselben Materialien; Reflexion/Licht aus 2155. Materialressourcen
  existieren vor Geometriepublikation; Form/Detailwechsel ändern keine Baustoffidentität.

## Abnahme
Echter Place zeigt lesbare Nah-/Fernbaustoffe ohne Flimmern, Maßstabswechsel oder
aufgemalte Nahgeometrie. Bewegung/Lichtwechsel erhalten Identität und Zeit-/Speicherbudget.
