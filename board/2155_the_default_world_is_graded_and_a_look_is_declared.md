Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: render, engine, client
Tags: lighting, shadows, hdr, night, presentation

# Light and camera response make the world coherent

## Ergebnis und Ist
Kohärentes Tages-/Nachtlicht, Kontakt/Schatten, Atmosphäre, Reflexion und stabile Kamera-
antwort. SceneRenderer/SkyStage, LightVisibility/Irradiance, HDR/TemporalResolve/Tonemap
bestehen; Weltwirkung/Kamera-Fit und zeitliche Qualität sind unzureichend bzw. unbewiesen.

## Besitzer und nächste Lieferung
Renderer besitzt Licht/Pässe/History, Kameraantwort Belichtung/Tonemapping, Client
Presentation/Pacing. Zuerst Himmelsfüllung/Sonnenschatten/Belichtung einer Stadt-/Bergansicht
verbessern. Vorhandene Inputs reichen; 2172 erweitert denselben Zustand um Wolken/Wetter.
PlaceCamera/Referenzkatalog besitzen FOV/Pose/Datum/Aufnahmezeit/Kalibrierstatus.

## Verfahren und Invarianten
- Gerichtete Schatten nach Bedeutung staffeln, lokale Lichter/Schatten bündeln; Kontakt
  und Fernwelt erhalten. Himmel füllt Schatten plausibel, keine globale Überbelichtung.
- Roughness-gefilterte Weltreflexion mit Sichtbarkeit/vollständiger Mipkette; Wasser
  keine zweite Lichtwelt. Emissive Fenster/Straßenlichter tragen die Nacht, fern als
  kompakte Beiträge statt Detail-/Schattenarbeit pro Fenster. Bloom kaschiert keine Geometrie.
- HDR/Farbraum/Belichtung zeitlich stabil; Reflexion/Schatten teilen Frame-Ursprung/Datum.
  History nutzt konsistente Tiefe/Bewegung; Disocclusion, Ursprung-/Quellenwechsel und neue
  Produkte verwerfen ungültige History. Dach/Fenster/Wasser/Laub ohne Geisterbilder.
- Kamera zuerst gegen Landmarken/Relief kalibrieren, dann Pitch/Höhe; kein Fit für fehlende
  Gebäude. Vergleich hält Zeit/Wetter/Maße fest. Kein Spezialrenderer oder Look je Place.
- Rendermaß/Ausgabemaß/Zielrate getrennt konfigurieren, Ressourcen dem Extent anpassen.
  854×480, 1280×720, 1920×1080 und höhere explizite Maße; 25/30/60 fps ergeben
  1000/fps = 40/33,33/16,67 ms. Keine versteckte Profilreduktion bei Budgetfehlern.
- SDL-Ressourcenwechsel/Submission auf zuständigem Thread, native Frameprodukte geschlossen;
  Uniforms sind keine Storage-Puffer. Tatsächlich residente Ressourcen zählen.
  Begrenzte Frames in Flight, Deadline-/Event-Warten statt Busy-Wait; OS erhält CPU-Zeit.

## Abnahme
Klare/bedeckte Stadt-/Bergbilder sowie Morgen/Abend/Nacht gewinnen Tiefe und Materialien;
Bewegung zeigt stabile Schatten/Reflexion/History. Host/GPU/Bytes getrennt nachweisen.
Profile aus AGENTS gelten; 480p30/A18-Pro-Qualität erst nach Gerätetest behaupten.
