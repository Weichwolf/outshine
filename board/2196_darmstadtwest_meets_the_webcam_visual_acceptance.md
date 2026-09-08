Type: feature
State: open
Parent: 2169
Area: world, render, client
Tags: webcam, acceptance
Depends: 2092, 2111, 2138, 2140, 2154, 2167, 2170, 2171, 2172, 2173, 2176, 2195

# DarmstadtWest: Dachlandschaft und diffuse Stadttiefe

## Referenz und Vertrag

Sichtprüfung der Webcam am 2026-09-08; dies ist das noch offene SOLL, keine Render-Abnahme.
Quelle: `build/shots/webcam/darmstadt-west_2026-09-07_1240.jpg` (5184 × 2916 px, aus dem Bildheader).
SHA-256: `972d9773fd8f4a511e66d30205acd479f0371c686c25cfc3d1fb3b2e8b47409c`.
Place: `DarmstadtWest` in `src/client/PlaceCamera.cpp`; IST: `build/shots/places/DarmstadtWest-*.png`.
Pose/Intrinsics aus WI 2170 übernehmen und im Vergleichsmanifest fixieren; vorhandene
Höhe/Pitch sind Schätzungen. Gleiche kalibrierte Kamera, unverzerrtes Seitenverhältnis.
Gemeinsamer Abnahmevertrag und Integrationsreihenfolge: WI 2169. RDR2/GTA5 auf PS4 sind
visueller Maßstab, keine Behauptung über proprietäre RAGE-Technik oder ein exaktes Zielbild.

## Erwartetes Gesamtbild

Erhöhter Blick über eine zusammenhängende Stadt unter einem weiten, weich bewölkten Himmel.
Vorne rahmen rote Ziegeldächer, helle Wohnfassaden und Baumkronen das Bild. Dahinter stehen
moderne Flachdachbauten neben gegliederten Altbauten; einzelne Türme unterbrechen die niedrige
Skyline. Zum flachen Horizont sinken Kontrast und Detail kontinuierlich.

## Sichtbare Anforderungen

- Dachfirste, Traufen, Gauben und Schornsteine bilden konstruktiv zusammenhängende Dächer;
  rote Tonziegel, dunkle Deckung und technische Flachdächer unterscheiden sich in Rauheit und Maßstab.
- Helle Fassaden besitzen Fensterachsen, Leibungen, Geschossrhythmus und Sockel. Der dunkle
  Glasbau im Mittelgrund liest sich als reflektierende Fassade, nicht als schwarzes Loch.
- Laubkronen füllen Höfe und Straßenräume mit variierenden Silhouetten und gedeckten Grüntönen;
  sie verdecken Teile der Gebäude und besitzen lesbare Eigen- und Kontaktschatten.
- Weiches Himmelslicht erhält Zeichnung an weißen Wänden und unter Traufen. Wolkenvolumen und
  diffuse Beleuchtung passen zusammen; die Ferne wird heller und kontrastärmer.

## Umsetzung und Abnahme

Vorhandene Generatoren über die genannten Blocker vervollständigen; keine Place-Sondermodelle.
Belegte OSM-/DEM-Strukturen erhalten, unbekannte Details regelbasiert und seeded ergänzen.
Referenzbilder sind Prüfmittel, niemals Textur, Skybox oder Generatorinput.

- [ ] Vordergrunddächer, mittlere Baugruppen und niedrige Skyline bleiben räumlich getrennt.
- [ ] Keine identisch extrudierten Häuserzeilen, fensterlosen Nahfassaden, spiegelnden Ziegel
      oder schwarzen Kronen; bedeckter Himmel erzeugt keine unpassend harten Sonnenschatten.
- [ ] Webcam und neues Client-PNG selbst nebeneinander öffnen; Gesamtbild und Ausschnitte
      der oben benannten Flächen beurteilen. Jeden Punkt mit Bildbeleg annehmen oder offen lassen.
- [ ] Manifest enthält Commit, Datenstände, Seed, Kamera, Zeit, Wetter, Features, Auflösung,
      Bildhash und Crop. Webcam-Zeitstempel aus Metadaten prüfen, nicht nur aus Dateinamen ableiten.
- [ ] Kamerafahrt sowie Zeit-/Wetterwechsel belegen räumliche und zeitliche Stabilität;
      Zielauflösung und Streaming-/Framebudget nach 2169/2092 bestehen mit voller Szene.
- [ ] Neue Daten/Seed-Variante prüft Übertragbarkeit. Fehlende belegte Daten ausdrücklich
      ausweisen; plausible Ersatzformen nicht als Rekonstruktion realer Details abnehmen.
