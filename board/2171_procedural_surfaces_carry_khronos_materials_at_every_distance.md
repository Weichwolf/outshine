Type: feature
State: active
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
Parameter, Surfacing/material.glsl/facadePattern die Auswertung. Zuerst Wand/Dach im bebauten Place unterscheiden; Asphalt folgt mit Infrastruktur. Vorhandene Inputs genügen; kein Warten auf weitere WIs.
2173 ergänzt Öffnungsgeometrie, 2337 Felsform und 2172 gemeinsamen Wetterzustand.

[EGSR 2010: kompakte Fassadenauswertung](https://peterwonka.net/Publications/pdfs/2010.EGSR.Haegler.GrammarBasedEncoding.pdf),
[lokales PDF](../doc/references/buildings/egsr/2010-grammar-based-encoding-of-facades.pdf).

## Aktuelle Lieferung: zusammenhängende Gebäudematerialien
Die vorhandene Gebäudeidentität steuert Wand-/Dachvarianten des explizit prozeduralen
Fassadenmaterials. Negative Flächencodes unterscheiden geneigte/ebene Dächer und Sockel;
Dächer müssen ihre vorhandenen Koordinaten bis zum Shader behalten. Materialwahl bleibt
je Gebäude und LOD stabil, statt jeder Fläche unabhängig eine Farbe zu geben.
Plausible Farb-/Baustoffvarianten sind keine belegten OSM-Materialangaben. Keine neuen
Dreiecke, Disktexturen oder Fassadenmeshes pro Ferngebäude. Bestehende Fensterfilter erhalten.
Zuerst vorhandene Dachcodes bis zur Runtime erhalten: Flachdächer neutral/rau, geneigte
Dächer mit stabilen gedeckten Ziegel-/Schiefervarianten. Das sind plausible Ergänzungen;
gelieferte Farbe/Material benötigt weiterhin einen nativen Parameterpfad. Acht gedeckte
Wandvarianten ergänzen die Dächer. Variante, Nutzung und Eingangsseite liegen im konstanten
U-Gruppenanteil; V bleibt die unbeschränkte Geschosskoordinate. Hohe Wände wechseln nicht
bei 64 Geschossen die Variante. Bestehende Vertexgröße und Geometrie bleiben erhalten.
Wandfarbe beeinflusst ausschließlich den Wandanteil; Glas, Metallrahmen und Türen
behalten ihre eigene Materialantwort statt die Wandfarbe mitzuerben.

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

## Forschungsgrundlage
[Frostbite-PBR](../doc/references/downloads/lighting/siggraph/2014-frostbite-pbr-course-notes.pdf)
und [Gabor-Noise](../doc/references/downloads/materials/siggraph/2009-sparse-gabor-noise.pdf)
([Primärquellen/Einordnung](../doc/references/README.md)): Roughness/Licht/Farbraum gemeinsam
kalibrieren; gerichtete Frequenzen aus dem Pixel-Footprint filtern. Teure Noise-Kernel
gegen einfachere Filter messen. Gelieferte Baustoff-/Farbwerte schlagen plausible Paletten.

## Abnahme
Nah- und Fernbaustoffe im echten Place lesbar, ohne Flimmern/Maßstabswechsel oder flache
Nahgeometrie. Licht/Bewegung erhalten Identität; Bildgewinn und GPU-Kosten getrennt belegen.
