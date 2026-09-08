Type: feature
State: open
Parent: 2169
Area: world, render, client
Tags: webcam, acceptance
Depends: 2092, 2111, 2138, 2154, 2166, 2170, 2171, 2172, 2173, 2176, 2195

# Rosenheim: Stadt vor einer gestaffelten Alpenkulisse

## Referenz und Vertrag

Sichtprüfung der Webcam am 2026-09-08; dies ist das noch offene SOLL, keine Render-Abnahme.
Quelle: `build/shots/webcam/rosenheim_2026-09-07_1240.jpg` (5184 × 3174 px, aus dem Bildheader).
SHA-256: `8f90c46f0cc39ae4720527addc686bf3f4de87983d8ce0f08c4465eb26d41929`.
Place: `Rosenheim` in `src/assets/places/Rosenheim.scenario`; IST: `build/shots/places/Rosenheim-*.png`.
Pose/Intrinsics aus WI 2170 übernehmen und im Vergleichsmanifest fixieren; vorhandene
Höhe/Pitch sind Schätzungen. Gleiche kalibrierte Kamera, unverzerrtes Seitenverhältnis.
Gemeinsamer Abnahmevertrag und Integrationsreihenfolge: WI 2169. RDR2/GTA5 auf PS4 sind
visueller Maßstab, keine Behauptung über proprietäre RAGE-Technik oder ein exaktes Zielbild.

## Erwartetes Gesamtbild

Über gemischten Wohn- und Gewerbebauten steht eine breite, entfernte Alpenkette mit erkennbaren
Graten und Einschnitten. Stadt, grüner Übergangsgürtel und mehrere Berglagen bleiben getrennt.
Der hohe Himmel ist hell und leicht bewölkt; die Berge sind blauer und weicher als die Nahdächer.

## Sichtbare Anforderungen

- Vorne warme Ziegeldächer und helle Fassaden, dahinter größere graue Gewerbe-/Flachdächer;
  Gauben, Dachränder, Fensterreihen und maßstäbliche technische Dachaufbauten verhindern Blockoptik.
- Schlanker Backsteinschornstein links und Turmsilhouetten rechts dienen bei belegter Lage als
  Kalibrieranker; unbekannte Baudetails werden nicht aus dem Foto als Sondermodell übernommen.
- Baumkronen verbinden Höfe und Straßen mit dem grünen Gürtel vor den Bergen. Keine senkrechten
  Großbillboards zwischen Häusern; Kronenmaßstab passt zu Geschossen und Straßenbreiten.
- DEM-Grate bleiben bis zum Horizont markant. Zusätzliche Felsdetails ändern nicht die gemessene
  Großform; Luftperspektive reduziert Fernkontrast, ohne die gesamte Alpenkette auszublenden.
- Weiße Wände behalten Zeichnung, Dächer variieren materialgebunden, Schatten bleiben gefüllt.
  Nahdetails verschwinden gefiltert mit Entfernung statt als flimmerndes Muster.

## Umsetzung und Abnahme

Vorhandene Generatoren über die genannten Blocker vervollständigen; keine Place-Sondermodelle.
Belegte OSM-/DEM-Strukturen erhalten, unbekannte Details regelbasiert und seeded ergänzen.
Referenzbilder sind Prüfmittel, niemals Textur, Skybox oder Generatorinput.

- [ ] Bergsattel und Gipfelfolge sind nach Kamerafit unabhängig von der Stadtgeometrie prüfbar.
- [ ] Keine gerundete Ersatzhügelkette, monotone Dachmasse, übergroßen Baumkarten oder abrupte
      Nebelwand. Webcam-Unschärfe wird nicht benutzt, um fehlende Engine-Details zu kaschieren.
- [ ] Webcam und neues Client-PNG selbst nebeneinander öffnen; Gesamtbild und Ausschnitte
      der oben benannten Flächen beurteilen. Jeden Punkt mit Bildbeleg annehmen oder offen lassen.
- [ ] Manifest enthält Commit, Datenstände, Seed, Kamera, Zeit, Wetter, Features, Auflösung,
      Bildhash und Crop. Webcam-Zeitstempel aus Metadaten prüfen, nicht nur aus Dateinamen ableiten.
- [ ] Kamerafahrt sowie Zeit-/Wetterwechsel belegen räumliche und zeitliche Stabilität;
      Zielauflösung und Streaming-/Framebudget nach 2169/2092 bestehen mit voller Szene.
- [ ] Neue Daten/Seed-Variante prüft Übertragbarkeit. Fehlende belegte Daten ausdrücklich
      ausweisen; plausible Ersatzformen nicht als Rekonstruktion realer Details abnehmen.
