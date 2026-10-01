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
Kameraantwort besitzt Belichtung/Tonemapping. Zuerst indirekte Himmelsfüllung, stabile
Sonnenschatten und Belichtung in einer Stadt-/Bergansicht liefern; vorhandene Pässe nutzen.

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
