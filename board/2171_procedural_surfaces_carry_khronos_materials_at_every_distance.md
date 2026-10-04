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
### Einheitliche Materialkomposition
- Kleine gemeinsame Materialbibliothek für Gebäude, Terrain, Infrastruktur und später
  Vegetation. Native Metallic-Roughness-Parameter und dieselbe Lichtantwort für alle Quellen.
  Palettenentscheidung in 2155; vorliegende Quellfarben bleiben erhalten.
- Grundmaterial, Flecken, Schmutz, Decals, Nässe/Schnee über metrische, objekt-/weltverankerte
  Masken komponieren. Bedeckung und physische Beschichtung unterscheiden. Nur tatsächlich
  betroffene Kanäle ändern; Lack über Metall ist kein beliebiger Metallic-Mittelwert.
- Masken treiben Farbe, Roughness und Relief konsistent aus Seed, Höhe, Exposition und
  Wasserablauf. Gemeinsamer Pixel-Footprint filtert Muster/Bedeckung vor der Lichtauswertung.
  Keine Farbaufkleber nach Beleuchtung und keine Materialwechsel an LOD-/Objektgrenzen.
- Oberflächengradienten aus prozeduralen Höhen, UVs und Projektionen im gemeinsamen Raum
  komponieren, finale Normale einmal rekonstruieren. Keine naive Mischung fremder Normalräume.
  Gefilterte Normalvarianz und Glanzantwort gemeinsam behandeln; Silhouette braucht Geometrie.
- Günstige Parameterblend ist eine Näherung. Metall/Lack und stark verschiedene Roughness
  gegen getrennt gewichtete BRDFs prüfen; weitere Lobes nur bei belegtem Bildgewinn/Kosten.
  Inaktive Schichten vor Auswertung ausschließen, keine unbegrenzte Shader-/Passkette.

[Disney-PBR](../doc/references/materials/siggraph/2012-disney-physically-based-shading.pdf),
[UE4-Metallic-Roughness](../doc/references/materials/siggraph/2013-unreal-engine-4-shading.pdf),
[The Order: Materialpipeline](../doc/references/materials/siggraph/2013-the-order-material-pipeline.pdf)
und [Surface Gradients](../doc/references/materials/jcgt/2020-surface-gradient-bump-mapping.pdf):
gemeinsame Parameter/Materialbibliothek, begrenzte Komposition und zusammenhängendes Relief;
Khronos-Vertrag erhalten. Keine Foto-/Bake-Texturen oder fremde BRDF ungeprüft übernehmen.

[Frostbite-PBR](../doc/references/lighting/siggraph/2014-frostbite-pbr-course-notes.pdf)
und [Gabor-Noise](../doc/references/materials/siggraph/2009-sparse-gabor-noise.pdf)
([Primärquellen/Einordnung](../doc/references/README.md)): Roughness/Licht/Farbraum gemeinsam
kalibrieren; gerichtete Frequenzen aus dem Pixel-Footprint filtern. Teure Noise-Kernel
gegen einfachere Filter messen. Gelieferte Baustoff-/Farbwerte schlagen plausible Paletten.

## Abnahme
Nah- und Fernbaustoffe im echten Place lesbar, ohne Flimmern/Maßstabswechsel oder flache
Nahgeometrie. Licht/Bewegung erhalten Identität; Bildgewinn und GPU-Kosten getrennt belegen.
