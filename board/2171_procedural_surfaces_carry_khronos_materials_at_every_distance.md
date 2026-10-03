Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: generators, render, world
Tags: materials, filtering, procedural

# Metric materials make walls, roofs and ground readable

## Ergebnis und Ist
Asphalt/Putz/Backstein/Beton/Fels/Glas/Dach mit passendem Maßstab, Relief und Rauheit.
Native Metallic-Roughness-Materialien, UVs und GroundMaterials bestehen; Places bleiben
flach/repetitiv. Vorhandene Parameter anschließen, keine zweite Materialarchitektur.

## Besitzer und nächste Lieferung
Generator/FacadeUv besitzt Geometrie und metrische Koordinaten, GroundMaterials native
Parameter, Surfacing/material.glsl/facadePattern die Auswertung. Zuerst Asphalt/Wand/Dach
im bebauten Place unterscheiden. Vorhandene Inputs genügen; kein Warten auf weitere WIs.
2173 ergänzt Öffnungsgeometrie, 2337 Felsform und 2172 gemeinsamen Wetterzustand.

## Verfahren
- Metrische Ziegel-/Backsteinraster mit gefilterten Fugen, Putzkorn/Betonporen. Cell-Hash
  variiert Elemente; Objektseed/Exposition/Ablauf steuern Alterung. Identität bleibt bei LOD
  und Kamerabewegung stabil; keine gleich große Zufallstextur oder Place-Fototextur.
- Höhenableitungen für Bumpnormalen, größere Vertiefungen als Geometrie. Pixel-Footprint
  filtert Frequenzen, Normalvarianz verbreitert Glanzantwort. Gemeinsame Tangenten,
  Flächennormalen, Farbräume und Khronos-BRDF erhalten.
- Glas mit Fresnel, gefilterter Welt-/Himmelsreflexion und energiegeteilter Transmission.
  Begrenztes Interior-Mapping plausibel ergänzen; betretbare Räume bleiben echte Geometrie.
  Fernfenster werden stabile Material-/Nachtlichtbeiträge.
- Parameter/Seeds einmal vorbereiten, Ressourcen vor Geometriepublikation bereitstellen.
  Shaderarbeit nur für benötigte Varianten. Nässe/Schnee ändern dieselben Materialien;
  kein Neubau unveränderter Gebäude, keine erzeugten Disktexturen.

## Abnahme
Nah- und Fernbaustoffe im echten Place lesbar, ohne Flimmern/Maßstabswechsel oder flache
Nahgeometrie. Licht/Bewegung erhalten Identität; Bildgewinn und GPU-Kosten getrennt belegen.
