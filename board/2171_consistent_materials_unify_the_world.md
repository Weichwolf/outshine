Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: generators, render, world
Tags: materials, composition, filtering

# Consistent materials make the whole world look coherent

## Ergebnis und Ist
Gebäude, Straßen, Fels/Boden, Wasser und Vegetation teilen Maßstab, Materialkomposition und Licht.
Beton/Putz/Backstein/Dach/Glas müssen lesbar sein; keine aufgesetzten Texturen, Fenster oder Bäume.
Native Metallic-Roughness-Materialien/UVs und prozedurale Shader bestehen; Places bleiben flach
und repetitiv. Farbvarianten allein erfüllen das Ziel nicht. Licht/Palette besitzt 2155.

## Besitzer und Lieferung
Generatoren erzeugen stabile Form-/Materialpläne; Renderer wertet dieselbe Komposition aus.
Erst Wände/Dächer im bebauten Place verbessern, dann mit Terrain/Asphalt abgleichen.
2173 besitzt räumliche Öffnungen, 2337 Felsform, 2172 Nässe/Schnee und 2111 Pflanzen.

## Verfahren und Assetgrenze
- Asset-Rohlinge (2280) enthalten Materialwahl, metrische Frames/UVs, Objektseed, Exposition,
  Fassadenachsen und nötige Basisfelder/Atlasprodukte. Nahrelief/-geometrie entsteht daraus
  zur Laufzeit; Cachehits wiederholen weder Quellergänzung noch Basisaufbau.
- Kleine gemeinsame Materialbibliothek; BaseColour/Metallic/Roughness/Normal und Coverage
  gelten für native Geometrie und Fernprodukte. Komposition in linearem RGB vor derselben BRDF.
  Grundmaterial + Flecken/Schmutz/Decals + Nässe/Schnee bilden dieselbe Oberfläche, keine Collage.
- Metrische gefilterte Fugen/Ziegel, Putzkorn/Betonporen und gerichtete Felsstruktur. Objekt-/
  Weltverankerung verhindert Schwimmen; Alterung folgt Lage/Wasserablauf statt blindem Rauschen.
  Frequenzen nach Pixel-Footprint filtern, Normalvarianz in Roughness überführen.
- Feinrelief mit Surface-Gradient/Bumpnormalen; Silhouette und echte Vertiefungen als Geometrie.
  Keine zusätzliche Geometrie/individuelle Texturkopie je Fernhaus. Gemeinsame Baustoffe teilen.
- Öffnungen gezielt aus Nutzung/Geschossen/Achsen; kein pauschales Fensterraster/aufgemalte Tür.
  Nahglas, gefilterte Hüllen und Fernverbände erhalten mittlere Helligkeit/Coverage und Material.
  Glas nutzt Fresnel, gemeinsame Himmel-/Weltreflexion und energiegeteilte Transmission.
- Innenräume: begrenztes Interior-Mapping aus Raum-/Achsenplan; betretbare Räume echte Geometrie.
  Fernemission kompakt filtern; ein sichtbares Fenster ist nicht automatisch ein Point Light.
- Prozedurale Varianten bleiben Ergänzungen; gelieferte Farben/Materialien haben Vorrang.
  Negative Dach-/Sockelcodes und unbeschränkte Geschosskoordinaten bleiben bis zum Shader erhalten.
- Infrastrukturkontakte (2281) liefern die Herkunft steiler Schnitt-/Stützflächen: natürlicher
  Boden/Fels oder bauliche Sicherung mit Beton/Mauerwerk. Steilheit allein bestimmt keinen Baustoff.
- Fern-Capture speichert unbeleuchtete Tiefe/Normalen/Material/Coverage. Aktuelles Licht, Wind,
  Nässe und Schatten bleiben Runtime. Nicht erfasste Effekte brauchen geeignete native Produkte.
  LOD/Impostor wechseln ohne mittlere Farb-, Glanz-, Coverage- oder Kontaktänderung.

## Bewährte Verfahren
[Disney-PBR](../doc/references/materials/siggraph/2012-disney-physically-based-shading.pdf),
[Frostbite-PBR](../doc/references/lighting/siggraph/2014-frostbite-pbr-course-notes.pdf):
Metallic-Roughness und gemeinsame direkte/indirekte Lichtantwort; Khronos/glTF bleibt Austauschformat.
[Surface Gradient](../doc/references/materials/jcgt/2020-surface-gradient-bump-mapping.pdf):
zusammenhängende Reliefkomposition. [Fassaden-Encoding](../doc/references/buildings/egsr/2010-grammar-based-encoding-of-facades.pdf):
kompakte metrische Regeln. [Recherche/weitere Papers](../doc/references/README.md) erhält Quellen.

## Abnahme
Datierte Stadt-/Bergbilder zeigen lesbare, kohärente Baustoffe unter Sonne, Bedeckung, Nässe/Nacht.
Nah-/Fernwechsel und Kamerabewegung ohne Schwimmen/Flimmern/Helligkeitssprung. Stamm/Boden-
Kontakt später mit 2111; gemeinsame Licht-/Schattenantwort statt pauschal dunklerem Laub.
Kosten nach Profil messen; Bildgewinn und Native-/Atlasübergänge belegen. AGENTS-Gates gelten.
