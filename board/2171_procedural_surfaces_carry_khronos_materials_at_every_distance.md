Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: world, generators, render
Tags: materials, filtering, visual

# Materials have metric detail and coherent light response

## Ergebnis und vorhandene Fähigkeit
Asphalt, Putz, Backstein, Beton, Fels, Glas und Dächer unterscheiden sich durch Maßstab,
Relief, Rauheit und Lichtantwort. Native Metallic-Roughness, UVs und Materialbindung
existieren; eröffnete Places wirken flach und repetitiv. GroundMaterials enthält bereits
Kornmaß und Reliefparameter. Vorhandene Parameter an die tatsächliche Auswertung anschließen.

## Besitzer und nächste Lieferung
GroundMaterials und native Materialparameter besitzen Semantik/Einheiten.
BuildingMesh/FacadeUv liefern passende Geometrie/UVs; Materialshader konsumieren sie.
Zuerst Asphalt, Wand und Dach eines bebauten Places sichtbar unterscheiden, ohne
Place-Texturen, neue Materialregistry oder vervielfachte Meshdaten.

## Konkrete Shaderverfahren und Integration
- Metrische Fassadenkoordinaten aus 2173s Plan: versetztes Backsteinraster mit gefilterten
  Fugenabständen, Putzkorn/Betonporen und Dachdeckung mit festen realen Längen. Cell-Hash
  variiert einzelne Steine/Ziegel; Objekt-Seed, Exposition und Ablaufspuren variieren
  Alterung über größere Skalen. Keine gleich große Zufallstextur auf jedem Gebäudekörper.
- Höhenableitungen liefern Bumpnormalen; größere Vertiefungen kommen aus Geometrie.
  Pixel-Derivate/Footprint filtern periodische Muster und Noise vor BRDF-Auswertung;
  Normalvarianz verbreitert die spekulare Antwort statt fernes Normalflimmern zu erhalten.
- Glas teilt Fensterplan/Normale: Fresnel und Roughness-gerechte Welt-/Himmelsreflexion,
  Energieaufteilung für Transmission und begrenzte Innenraumwirkung. Analytisches
  Box-Interior-Mapping ist eine plausible Ergänzung, kein Ersatz für betretbare Räume.
  Fernfenster integrieren zu stabilem Material-/Nachtlichtbeitrag statt Einzelöffnungen.
- Material-/Instanzparameter einmal im gemeinsamen Compute vorbereiten und geschlossen
  publizieren; Surfacing/material.glsl/facadePattern konsumieren dieselben Maße und Seeds.
  Rockdetail aus 2337 teilt Filter-/BRDF-Konventionen. Keine doppelte Materialwelt oder
  pro Frame erzeugte Textur; GPU-Kosten folgen aktiven sichtbaren Materialvarianten.

## Umsetzung und Invarianten
- Khronos Metallic-Roughness, explizite BRDF und korrekte Farbräume verwenden. Physikalische
  Einheiten/Skalen bleiben von Geometrieerzeugung bis Shader konsistent; keine Showroom-Werte.
- Prozedurale metrische Struktur, Variation und Alterung aus Material-/Objektparametern
  ableiten. Mip-/Footprintfilter erhalten mittlere Energie und zeitliche Stabilität.
- Generierte Tangenten, Normalmaps und Flächennormalen stimmen überein. Anisotrope/
  spekulare Filter berücksichtigen Geometrie- und Normaldetail, ohne pauschale Glanzlöschung.
- Glas erhält Reflexion, Transmission und nachvollziehbare Energieaufteilung; Wasser 2145.
  Materialressourcen existieren vor Geometriepublikation und werden atomar ersetzt.
- Wetterzustand aus 2172 ändert Nässe/Schnee desselben Materials; keine separate Place-Palette.

## Abnahme
Nah-/Fernansichten und Bewegung zeigen lesbare Baustoffe ohne Flimmern, aufgemalte
Geometrie oder Maßstabswechsel. Licht-/Belichtungsänderung erhält Materialidentität.
Gewinn am echten Place bei erhaltenem Zeit-/Speicherbudget beurteilen.
