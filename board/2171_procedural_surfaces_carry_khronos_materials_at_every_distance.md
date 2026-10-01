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
