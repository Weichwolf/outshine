Type: bug
State: active
Architecture: ready
Priority: P0
Parent: 2145
Area: render
Tags: water, ownership
Depends:

# Switching a candidate back to the published plan keeps matching frame resources

## Ergebnis und vorhandene Fähigkeit
Ein Weltkandidat kann vom veröffentlichten Renderplan zu einem anderen und wieder
zurück wechseln. Seine Renderziele gehören immer zum gewählten Plan. Das neue
Wasser-Material aktiviert Transmission und legt einen vorhandenen Fehler offen:
Flensburg stürzt in SDL Metal beim Schreiben auf ein fehlendes Renderziel ab.

## Implementierung und Besitz
`SceneRenderer::ReuseFrameResources` wählt bei gleichem Plan den veröffentlichten
Frame. Ein schon vorhandener privater `Candidate::Frame` bleibt jedoch erhalten;
`ActiveFrame` und Publikation bevorzugen ihn. Plan und Ressourcen widersprechen sich.
Beim Wiederverwenden den privaten Frame freigeben und Bindings auf den publizierten
Frame setzen. Besitz bleibt eindeutig; Abbruch erhält die bisher veröffentlichte Welt.
Keine unterdrückten Passes, Ersatztexturen oder Entfernung des Wasser-Materials.
Bereits hochgeladene Ground-Klassen und Palette sind Weltinhalt, keine Frame-Ressource.
Plan-Reuse darf sie nicht leeren. Leere Bindings nur für einen noch unbestückten
Kandidaten initialisieren; Publikation erhält vorhandene Quellen-/GPU-Daten.

## Abnahme
- Veröffentlichte transmissive Welt → privater opaker Plan → ursprünglicher Plan:
  Nach Publikation stimmen echte lineare GPU-Pixel mit der Ausgangswelt überein.
- Gegenrichtung und Abbruch erhalten die zugehörigen Bilder. Kein fehlendes Ziel,
  abgestürzter Treiber oder stale Binding. Flensburg muss wieder tatsächlich rendern.
- Klassifikationsupload vor und nach Geometry-Build liefert dieselben Terrain-Pixel.
  Wasseraktivierung färbt weder Berge noch Straßen um.
- Format, fokussierte RuntimeScene-Suite, Places und vollständiger Lint.
