Type: feature
State: open
Parent: 2169
Area: world, render, client
Tags: webcam, acceptance
Depends: 2092, 2111, 2129, 2138, 2144, 2145, 2154, 2166, 2167, 2170, 2171, 2172, 2174, 2176, 2195

# Malcesine: Gegliederte Steilfelsen über mediterranem Seeufer

## Referenz und Vertrag

Sichtprüfung der Webcam am 2026-09-08; dies ist das noch offene SOLL, keine Render-Abnahme.
Quelle: `build/shots/webcam/malcesine_2026-09-07_1240.jpg` (6000 × 4000 px, aus dem Bildheader).
SHA-256: `13ae10442cf1a3efbcc3004a703977f0d0bc11fde299bda4822d37dd1876996f`.
Place: `Malcesine` in `src/assets/places/Malcesine.scenario`; IST: `build/shots/places/Malcesine-*.png`.
Pose/Intrinsics aus WI 2170 übernehmen und im Vergleichsmanifest fixieren; vorhandene
Höhe/Pitch sind Schätzungen. Gleiche kalibrierte Kamera, unverzerrtes Seitenverhältnis.
Gemeinsamer Abnahmevertrag und Integrationsreihenfolge: WI 2169. RDR2/GTA5 auf PS4 sind
visueller Maßstab, keine Behauptung über proprietäre RAGE-Technik oder ein exaktes Zielbild.

## Erwartetes Gesamtbild

Über einer weiten blaugrünen Wasserfläche liegt das gegenüberliegende Band hoher, heller
Felswände mit gestaffelten Bergrücken dahinter. Von rechts schiebt sich eine bewachsene
Halbinsel mit Villen und schmalem Ufersaum ins Bild. Landschaft, Wasser und Architektur wirken maßstäblich.

## Sichtbare Anforderungen

- Steilwände zeigen unregelmäßige Pfeiler, Rinnen, Absätze und bewachsene Bänder. Seitenflächen
  besitzen echte geometrische Unterteilung und korrekte Normalen; eine Normalmap heilt keine Zähne.
- Kalkartig heller Fels variiert grau/ocker mit Brüchen und Verwitterung auf mehreren Skalen.
  Material- und Detailwahl folgt einer plausiblen Geologie, ohne aus DEM gesicherte Lithologie zu behaupten.
- Halbinsel trägt schmale dunkle Zypressenformen, breite mediterrane Kronen, Sträucher und
  trockenere Grünwerte. Rosafarbene/helle Villenformen und niedrige Bauten sitzen im Gelände.
- Schmaler Ufersaum verbindet Land und See ohne Kragen, senkrechte Gittervorhänge oder Löcher.
  Wasser bleibt auf gemeinsamer Höhe; Wellen und Reflexionen folgen Wind, Licht und Ufergeometrie.
- Nahe Uferspiegelungen sind stärker gegliedert, ferne Felsreflexionen weicher. Segelboote,
  Moorings und vereinzelte Wakes sind plausible Simulation, keine fest eingebackenen Fotodetails.
- Sonnige Felsflächen behalten Zeichnung, Rinnen besitzen indirekte Füllung; fernere Rücken
  werden blauer und kontrastärmer. Wasser darf weder Plastikglanz noch konstante Grundfarbe zeigen.

## Umsetzung und Abnahme

Vorhandene Generatoren über die genannten Blocker vervollständigen; keine Place-Sondermodelle.
Belegte OSM-/DEM-Strukturen erhalten, unbekannte Details regelbasiert und seeded ergänzen.
Referenzbilder sind Prüfmittel, niemals Textur, Skybox oder Generatorinput.

- [ ] Halbinselspitze, gegenüberliegende Uferlinie und Bergprofil passen nach Kalibrierung.
- [ ] Fels-Nah-/Seitenansicht und bewegte Kamera bestehen zusätzlich zum Gesamtbild:
      kein senkrechter Faltenvorhang, regelmäßige Dreieckszähne, kahle Landzunge oder LOD-Riss.
- [ ] Webcam und neues Client-PNG selbst nebeneinander öffnen; Gesamtbild und Ausschnitte
      der oben benannten Flächen beurteilen. Jeden Punkt mit Bildbeleg annehmen oder offen lassen.
- [ ] Manifest enthält Commit, Datenstände, Seed, Kamera, Zeit, Wetter, Features, Auflösung,
      Bildhash und Crop. Webcam-Zeitstempel aus Metadaten prüfen, nicht nur aus Dateinamen ableiten.
- [ ] Kamerafahrt sowie Zeit-/Wetterwechsel belegen räumliche und zeitliche Stabilität;
      Zielauflösung und Streaming-/Framebudget nach 2169/2092 bestehen mit voller Szene.
- [ ] Neue Daten/Seed-Variante prüft Übertragbarkeit. Fehlende belegte Daten ausdrücklich
      ausweisen; plausible Ersatzformen nicht als Rekonstruktion realer Details abnehmen.
