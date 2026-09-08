Type: feature
State: open
Parent: 2169
Area: world, render, client
Tags: webcam, acceptance
Depends: 2092, 2111, 2121, 2138, 2144, 2145, 2154, 2166, 2170, 2171, 2172, 2175, 2176, 2195

# Feldkirch: Altstadt im bewaldeten Tal mit eingebundenem Flussraum

## Referenz und Vertrag

Sichtprüfung der Webcam am 2026-09-08; dies ist das noch offene SOLL, keine Render-Abnahme.
Quelle: `build/shots/webcam/feldkirch_2026-09-07_1240.jpg` (6000 × 4000 px, aus dem Bildheader).
SHA-256: `e08517756ea9ec1cb3a68d018add44eae8085177115f30647a848a8202f839be`.
Place: `Feldkirch` in `src/client/PlaceCamera.cpp`; IST: `build/shots/places/Feldkirch-*.png`.
Pose/Intrinsics aus WI 2170 übernehmen und im Vergleichsmanifest fixieren; vorhandene
Höhe/Pitch sind Schätzungen. Gleiche kalibrierte Kamera, unverzerrtes Seitenverhältnis.
Gemeinsamer Abnahmevertrag und Integrationsreihenfolge: WI 2169. RDR2/GTA5 auf PS4 sind
visueller Maßstab, keine Behauptung über proprietäre RAGE-Technik oder ein exaktes Zielbild.

## Erwartetes Gesamtbild

Erhöhter Blick über eine dichte Altstadt in ein offenes Tal. Vorn rahmen Parkkronen und helle
Gebäude die roten und dunklen Dachflächen. Fluss- und Verkehrsachsen führen in die Tiefe;
ein gerundeter grüner Hügel steht mittig, bewaldete Talflanken und ferne Berge staffeln den Horizont.

## Sichtbare Anforderungen

- Altstadtblöcke besitzen gegliederte Sattel-/Walmdächer, enge Zwischenräume, Traufen,
  Fensterachsen und einzelne Turmsilhouetten; keine durchgehende uniforme Dachplatte.
- Nahe helle Gebäude behalten Geschossmaßstab und Kontakt zum Park. Hangbebauung sitzt auf
  plausiblen Fundamenten; Gelände-Stamping darf keine bildfüllende Wand rechts erzeugen.
- Flusskontur, befestigte Ufer und Brücken bilden einen zusammenhängenden Wasserraum.
  Straßen und Bahn folgen nachvollziehbaren Höhen, mit tragenden Bauwerken an Überführungen.
- Der zentrale Hügel zeigt Wiesenlichtungen und bebaute Ränder, die Talflanken geschlossenen
  Mischwald. Pflanzreihen nur aus belegter Nutzung oder plausibler generischer Flächenregel.
- Warme Nahdächer und grüne Kronen gehen in kühlere, kontrastärmere Fernlagen über.
  Mauern, Ziegel, Glas, Wasser und Vegetation behalten unterschiedliche Materialantworten.
- Pose/Datum und finale Geländeform separat prüfen: die Referenz belegt die falsche Nahwand,
  nicht deren Ursache. Keine kompensierende Kameraänderung, die einen Stampingfehler versteckt.

## Umsetzung und Abnahme

Vorhandene Generatoren über die genannten Blocker vervollständigen; keine Place-Sondermodelle.
Belegte OSM-/DEM-Strukturen erhalten, unbekannte Details regelbasiert und seeded ergänzen.
Referenzbilder sind Prüfmittel, niemals Textur, Skybox oder Generatorinput.

- [ ] Zentraler Hügel, Flussknicke und Talöffnung stimmen als unabhängige Kamera-/DEM-Anker.
- [ ] Ufer- und Brücken-Nahansicht zeigen geschlossene Kontakte; keine rechte Nahwand,
      tiefer künstlicher Ufergraben, schwebenden Häuser oder kahlen Ersatzhänge.
- [ ] Webcam und neues Client-PNG selbst nebeneinander öffnen; Gesamtbild und Ausschnitte
      der oben benannten Flächen beurteilen. Jeden Punkt mit Bildbeleg annehmen oder offen lassen.
- [ ] Manifest enthält Commit, Datenstände, Seed, Kamera, Zeit, Wetter, Features, Auflösung,
      Bildhash und Crop. Webcam-Zeitstempel aus Metadaten prüfen, nicht nur aus Dateinamen ableiten.
- [ ] Kamerafahrt sowie Zeit-/Wetterwechsel belegen räumliche und zeitliche Stabilität;
      Zielauflösung und Streaming-/Framebudget nach 2169/2092 bestehen mit voller Szene.
- [ ] Neue Daten/Seed-Variante prüft Übertragbarkeit. Fehlende belegte Daten ausdrücklich
      ausweisen; plausible Ersatzformen nicht als Rekonstruktion realer Details abnehmen.
