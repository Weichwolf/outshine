Type: feature
State: open
Parent: 2169
Area: world, render, client
Tags: webcam, acceptance
Depends: 2092, 2121, 2129, 2138, 2145, 2154, 2167, 2170, 2171, 2172, 2174, 2175, 2195

# Husum: Gebaute Hafenkante und lebendiges Wasser

## Referenz und Vertrag

Sichtprüfung der Webcam am 2026-09-08; dies ist das noch offene SOLL, keine Render-Abnahme.
Quelle: `build/shots/webcam/husum-hafenklappbruecke_2026-09-07_1230.jpg` (6000 × 4000 px, aus dem Bildheader).
SHA-256: `0e04683064a44cea3c88908fbb2bc406b53e4fbe4624be33dd410e8d1b83eabf`.
Place: `Husum` in `src/assets/places/Husum.scenario`; IST: `build/shots/places/Husum-*.png`.
Pose/Intrinsics aus WI 2170 übernehmen und im Vergleichsmanifest fixieren; vorhandene
Höhe/Pitch sind Schätzungen. Gleiche kalibrierte Kamera, unverzerrtes Seitenverhältnis.
Gemeinsamer Abnahmevertrag und Integrationsreihenfolge: WI 2169. RDR2/GTA5 auf PS4 sind
visueller Maßstab, keine Behauptung über proprietäre RAGE-Technik oder ein exaktes Zielbild.

## Erwartetes Gesamtbild

Naher Blick über Hafenwasser auf eine diagonal zurückweichende Kaikante mit eng gereihten,
individuellen Giebelfronten. Roter Backstein, gelber und heller Putz, dunkle Dächer und vertikale
Wasserreflexionen bestimmen das Bild. Der Hafen ist ein gebauter Raum mit sichtbaren Kontakten.

## Sichtbare Anforderungen

- Die Kaimauer ist eine vertikale, geschlossene Stützkonstruktion mit oberem Abschluss,
  Fugen, Wasserlinie und plausibler Feuchte-/Algenzone. Keine aus dem Höhenfeld gezogene Böschung.
- Pfähle, Leitern, Poller und Geländer stehen maßstäblich an nutzbaren Kaikanten; Brücken-/Stegteile
  rechts besitzen Auflager und Unterseiten. Uferweg, Gebäude und Bauwerk schließen räumlich an.
- Giebel unterscheiden sich in Breite, Höhe, Fensterachsen und Dachform. Gauben, Leibungen,
  Schaufenster und Markisen geben Nahmaßstab; Putz, Backstein, Holz und Metall reagieren verschieden.
- Graugrünes bis bräunliches Hafenwasser zeigt feine Wellen und längliche, gebrochene Reflexionen
  der Fronten und Pfähle. Schatten im Vordergrund verdunkelt das Wasser, löscht seine Struktur nicht.
- Plausible Boote, Masten und sparsame Hafen-/Cafépopulation beleben den Raum. Keine Pflicht,
  reale Firmenschilder, Bootsnamen, Personen oder die konkrete Liegeplatzbelegung nachzubauen.

## Umsetzung und Abnahme

Vorhandene Generatoren über die genannten Blocker vervollständigen; keine Place-Sondermodelle.
Belegte OSM-/DEM-Strukturen erhalten, unbekannte Details regelbasiert und seeded ergänzen.
Referenzbilder sind Prüfmittel, niemals Textur, Skybox oder Generatorinput.

- [ ] Kaiknick, Giebelstaffelung und hinterer Hafenabschluss passen nach Kamerakalibrierung.
- [ ] Nah-/Seitenansicht beweist geschlossene Kaimauer und Wasserkontakt. Keine gezahnte
      Erdwand, durchgehenden hellen Uferbänder, schwebenden Pfähle oder statische Reflexionskopie.
- [ ] Webcam und neues Client-PNG selbst nebeneinander öffnen; Gesamtbild und Ausschnitte
      der oben benannten Flächen beurteilen. Jeden Punkt mit Bildbeleg annehmen oder offen lassen.
- [ ] Manifest enthält Commit, Datenstände, Seed, Kamera, Zeit, Wetter, Features, Auflösung,
      Bildhash und Crop. Webcam-Zeitstempel aus Metadaten prüfen, nicht nur aus Dateinamen ableiten.
- [ ] Kamerafahrt sowie Zeit-/Wetterwechsel belegen räumliche und zeitliche Stabilität;
      Zielauflösung und Streaming-/Framebudget nach 2169/2092 bestehen mit voller Szene.
- [ ] Neue Daten/Seed-Variante prüft Übertragbarkeit. Fehlende belegte Daten ausdrücklich
      ausweisen; plausible Ersatzformen nicht als Rekonstruktion realer Details abnehmen.
