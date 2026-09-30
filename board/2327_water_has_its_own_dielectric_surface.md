Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2145
Area: engine, render
Tags: water, webcam
Depends: 2328

# Existing water geometry renders as its own dielectric surface

## Ergebnis und vorhandene Fähigkeit
Wasser verwendet seine eigenen Dreiecke und ein eigenes lichtdurchlässiges Material.
Die Wasserdreiecke existieren bereits; `Laying::BuildWaterSurfaces` reicht jedoch
das Terrain-Material weiter. Dadurch werden Wasseroberfläche und Terrain gemeinsam
klassifiziert und beleuchtet. Der Renderer besitzt bereits den Transmission-Pass.

## Implementierung und Besitz
- Engine erzeugt ein eigenes natives Material für den vorhandenen Wasser-Part und
  reicht dessen Handle an `Generators::AppendWaterSurfaceGeometry`. Ausschließlich
  das echte Terrain behält `GroundSurface` und den Ground-Klassifikationsshader.
- Wasser ist ein nichtmetallisches Dielektrikum: IOR 1.333, Transmission 1, deckende
  geometrische Coverage. Keine Alpha-Mischung als Ersatz für Lichttransmission.
  Rauheit 0.08 ist ein allgemeiner Ausgangswert, kein gemessener Wetterzustand.
- Bestehende Materialauflösung bestimmt den SurfaceKind und Renderplan. Material,
  Mesh und GPU-Lebensdauer bleiben beim selben atomar veröffentlichten Weltprodukt.
  Keine zusätzlichen Welt-Tiles, persistenten Produkte oder Frame-Generierung.
- Keine erfundene konstante Wassertiefe. Tiefenabhängige Absorption, Brechung,
  windgetriebene Wellen und Szenenreflexion bleiben ausdrücklich in 2145/2129 offen.
  Dieser Schritt repariert weder fehlende Küsten noch falsche Gewässerpegel.

## Invarianten und Abnahme
- Inseln und konkave Ufer behalten ihre vorhandene Wassertriangulation. Straßen-
  und Terrain-Materialien behalten ihre bisherigen Handles und Eigenschaften.
- Runtime ordnet Wasser dem Transmission-Pass und Terrain der Ground-Domain zu.
  Eine Rückbindung des Wassers an GroundSurface verletzt diesen Vertrag.
- Flensburg und ein Binnengewässer zeigen den getrennten Wasserpfad im echten
  Place-Rendering; Bilder öffnen, Ufer und Budget vergleichen. Unverbesserte oder
  fehlerhafte Flächen bleiben rot. Format, fokussierte Suiten und vollständiger Lint.
