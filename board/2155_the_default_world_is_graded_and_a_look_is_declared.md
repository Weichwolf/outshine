Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: render, engine
Tags: lighting, shadows, hdr, night

# Light and camera response make the world coherent

## Ergebnis und vorhandene Fähigkeit
Sonne, Himmel und lokale Lichter beleuchten eine zusammenhängende Welt. Schatten,
Atmosphäre, Spiegelungen und Belichtung erzeugen räumliche Tiefe; nachts bleibt die
Stadt lesbar. HDR-/Material-/Sky-Pfade existieren, kohärente Weltwirkung ist unzureichend.

## Besitzer und nächste Lieferung
SceneRenderer, SkyStage und vorhandene Pass-/Materialpfade besitzen Licht und Sichtbarkeit.
TemporalResolve/Tonemap bestehen; Kamera-Fit und zeitliche Bildqualität sind noch unbewiesen.
Kameraantwort besitzt Belichtung/Tonemapping. Zuerst indirekte Himmelsfüllung, stabile
Sonnenschatten und Belichtung in einer Stadt-/Bergansicht liefern; vorhandene Pässe nutzen.

## Ausgabeprofile und künstlerischer Schwerpunkt
- Öffentliche Extent-/RenderTarget-Verträge erlauben bereits frei gewählte Maße.
  Renderauflösung, Ausgabeauflösung und Zielrate getrennt konfigurieren; native Renderer-
  Ressourcen folgen dem tatsächlichen Extent. 480p (16:9: rund 854×480), 1280×720,
  1920×1080 (1080p), darüber explizite Maße. Das angefragte „1920p“ meint 1080p.
- Zielraten 25/30/60 fps ergeben 1000/fps = 40/33,33/16,67 ms pro Frame.
  Profil trägt gemeinsame Qualitäts-/Zeit-/Speicherwerte; framerateabhängige Simulation
  ist verboten. Das vorhandene 720p60-Gate bleibt bis zum expliziten Profilausbau lesbar.
- Hohe Material-/Licht-/Schattenqualität zuerst erhalten; Rendermaß und Rekonstruktion
  nach gemessenem Bildgewinn wählen. 480p30 auf A18 Pro ist unbewiesen, kein Geräteclaim.
  Temporale Verfahren müssen auch bei 25/30 fps und Drehung stabile Details erhalten.
- Client/Shots konfigurieren Profil und Pacing gemeinsam; keine heimliche Reduktion bei
  Budgetfehlern. Referenzvergleich verwendet gleiche Kamera, Zeit und dokumentierte Maße.

## Kamera und bewegtes Bild
- PlaceCamera und der Referenzkatalog besitzen Kamera/FOV, Höhendatum, Aufnahmezeit
  und Kalibrierstatus. Zuerst horizontale Landmarken/Relief mit unveränderter Geometrie
  abgleichen, dann Pitch/Höhe prüfen. Ein Kamera-Fit repariert keine fehlenden Gebäude.
- Reprojektion verwendet konsistente Tiefe, Kameratransform und Bewegungsdaten.
  Disocclusion, Ursprung-/Quellenwechsel und neue Produkte verwerfen ungültige History;
  Dachkanten, Fenster, Wasser und später Laub bleiben ohne Geisterbilder/Flimmern.
- Zielbild ist eine kohärente Außenwelt; kein zusätzlicher Spezialrenderer pro Place.

## Umsetzung und Invarianten
- Geometrie, Licht, Schatten und Wasser teilen Frame-Ursprung und Datumsbezug aus 2188.
  Atmosphäre/Sonne/Wolken verwenden denselben Weltzustand aus 2172.
- Gerichtete Schatten nach sichtbarer Bedeutung staffeln; lokale Lichter/Schatten begrenzt
  bündeln. Schattenkontakt erhalten, keine globale Überfüllung oder verschwundene Fernwelt.
- Himmel füllt beschattete Flächen plausibel. Reflexion vorhandener Welt mit Roughness,
  Sichtbarkeit und vollständiger Mipkette; Wasser keine zweite Lichtberechnung.
- Emissive Fenster, Straßen-/Gebäudelicht und ihre Reflexion tragen die Nacht. In der
  Ferne kompakte Lichtbeiträge statt voller Detailgeometrie und Einzel-Schatten pro Fenster.
- Belichtung und HDR-Antwort bleiben zeitlich stabil; physikalisch gleiche Eingaben
  erzeugen gleiche Antwort. Bloom/Glare ergänzt Licht, kaschiert keine falsche Geometrie.
- SDL-Submission und Ressourcenwechsel erfolgen atomar auf zulässigem Thread. Keine
  doppelten/mismatched Frameprodukte oder ungeprüften GPU-Zeitbehauptungen.
- Client-Presentation und Idle-Warten geben dem OS CPU-Zeit; begrenzte Frames in Flight
  und deadline-/ereignisorientiertes Pacing statt Busy-Wait oder ungeregelter SDL-Schleife.

## Abnahme
Morgen/Mittag/Abend/Nacht und klar/bedeckt zeigen nachvollziehbare Tiefe und Material.
Weltursprung-/Kamerabewegung verschiebt keine Schatten oder Reflexionsobjekte. Bildgewinn
und GPU-/Hostkosten getrennt nachweisen; keine überhöhten Grenzwerte für einen schönen Screenshot.
