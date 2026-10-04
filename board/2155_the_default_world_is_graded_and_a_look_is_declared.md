Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: render, engine, client
Tags: lighting, shadows, hdr, presentation

# Light and camera response make existing world geometry convincing

## Ergebnis und Ist
Kohärentes Tages-/Nachtlicht, räumliche Schatten/Reflexion und stabile Belichtung.
SceneRenderer/SkyStage, LightVisibility/Irradiance, HDR/TemporalResolve/Tonemap bestehen;
Weltwirkung und Kameraantwort sind unzureichend oder nicht am Place belegt.

## Besitzer und nächste Lieferung
Renderer besitzt Licht/Pässe/History, Client Kamera/Pacing; PlaceCamera/Referenzkatalog
Pose/FOV/UTC und Kalibrierung. Mit vorhandenen Inputs zuerst eine Stadt- und Bergansicht
über Himmelsfüllung, Sonnenschatten und Belichtung verbessern. Kein Quellen-/SDK-Blocker.
2172 ergänzt später denselben Lichtzustand um Wolken und Wetter.

## Verfahren
- Kamera gegen Landmarken/Relief kalibrieren; falsche Gebäude nicht durch Pose kaschieren.
  Gerichtete Schatten nach Bildwirkung staffeln, Kontakt und Fernwelt erhalten. Lokale
  Lichter/Schatten bündeln; Himmel füllt Schatten ohne globale Überbelichtung.
- Roughness-gefilterte Weltreflexion mit Sichtbarkeit und vollständigen Mips; Wasser nutzt
  dieselbe Lichtwelt. Emissive Fenster/Straßenlichter fern als kompakte Beiträge, keine
  Detail-/Schattenarbeit je Fenster. Bloom ersetzt keine Geometrie.
- HDR/Farbraum/Belichtung zeitlich stabil; konsistente Tiefe/Bewegung und Frame-Ursprung.
  Disocclusion, neue Produkte und Ursprungswechsel invalidieren betroffene History.
- Arbeit pro tatsächlich benötigtem Pass/Extent; keine ungenutzten Renderressourcen.
  Render-/Ausgabemaß und Zielrate getrennt; Profile aus AGENTS, kein stiller Qualitätswechsel.
- SDL-Submission/Ressourcenwechsel auf zuständigem Thread, begrenzte Frames in Flight,
  Deadline-/Event-Warten statt Busy-Wait. OS erhält CPU-Zeit, GPU-Freigabe nach letzter Nutzung.

## Forschungsgrundlage
[Frostbite-PBR](../doc/references/downloads/lighting/siggraph/2014-frostbite-pbr-course-notes.pdf),
[TAA-Übersicht](../doc/references/downloads/presentation/cgf/2020-temporal-antialiasing-survey.pdf)
([Primärquellen/Einordnung](../doc/references/README.md)): Lichtgrößen/IBL/Belichtung zusammen
kalibrieren. Schatten nach projizierter Wirkung staffeln; Bias gegen Kontaktverlust prüfen.
History anhand Tiefe, Bewegung und Produktgültigkeit validieren; flimmerfreie Unschärfe ist
kein Bildgewinn. Bewegtes Wasser/Laub und Ursprungssprünge gesondert integrieren.

## Abnahme
Datierte klare/bedeckte Stadt-/Bergbilder gewinnen Tiefe und Materiallesbarkeit; Morgen,
Abend und Nacht erhalten plausible Helligkeit. Bewegung ohne Geisterbilder/Belichtungssprünge.
Host/GPU/Bytes getrennt messen, kein Geräteversprechen aus Desktop-Messungen.
