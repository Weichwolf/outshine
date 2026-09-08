Type: feature
State: open
Parent: 2169
Area: world, render, client
Tags: webcam, acceptance
Depends: 2092, 2111, 2123, 2137, 2138, 2154, 2170, 2171, 2172, 2173, 2176, 2195

# Olympiaturm: Sportcampus, Wohnstaffelung und dichtes Stadtgrün

## Referenz und Vertrag

Sichtprüfung der Webcam am 2026-09-08; dies ist das noch offene SOLL, keine Render-Abnahme.
Quelle: `build/shots/webcam/tum-olympiapark_2026-09-07_1240.jpg` (6000 × 4000 px, aus dem Bildheader).
SHA-256: `91c871946ee07dff52f38cc0cc7ac0929e2e229652fc8913e5a49dcb830b9d48`.
Place: `Olympiaturm` in `src/client/PlaceCamera.cpp`; IST: `build/shots/places/Olympiaturm-*.png`.
Pose/Intrinsics aus WI 2170 übernehmen und im Vergleichsmanifest fixieren; vorhandene
Höhe/Pitch sind Schätzungen. Gleiche kalibrierte Kamera, unverzerrtes Seitenverhältnis.
Gemeinsamer Abnahmevertrag und Integrationsreihenfolge: WI 2169. RDR2/GTA5 auf PS4 sind
visueller Maßstab, keine Behauptung über proprietäre RAGE-Technik oder ein exaktes Zielbild.

## Erwartetes Gesamtbild

Hoher Blick auf einen niedrigen Sportcampus in einer dicht begrünten Stadt. Vorn liegen
Baumkronen, Wege und markierte Sportflächen; der große flache Hallenkomplex steht davor bzw.
daneben. Gestaffelte Wohnbauten im Mittelgrund gehen in Waldzüge und eine diesige Ebene über.

## Sichtbare Anforderungen

- Fußballfelder, Laufbahn und befestigte Plätze besitzen nutzungsrichtige Konturen, Linien,
  Tore, Zäune und Anschlüsse. Markierungen liegen auf der Oberfläche, ohne Z-Fighting oder Flimmern.
- Der Hallenkomplex bleibt niedrig und breit, mit Dachstaffelung, Innenhöfen, Oberlichtern und
  lesbarer Fassadenstruktur. Dachbegrünung, dunkle Verkleidung und Glas sind getrennte Materialien.
- Wohnscheiben und Türme dahinter besitzen unterschiedliche Höhen, Rücksprünge und Balkonbänder;
  fehlende Höhentags dürfen nicht die ganze Stadt in gleich hohe Hochhäuser verwandeln.
- Dichte Laubkronen rahmen Wege und Felder; Schatten, Lücken, Randbäume und Artenformen bilden
  einen Bestand. Fernwald bleibt volumetrisch lesbar, ohne sichtbares Atlasraster.
- Gedämpftes Tageslicht und zunehmender Dunst erhalten räumliche Tiefe. Parkplätze und kleine
  Fahrzeuge geben Maßstab, ohne die tatsächliche Belegung der Webcam zu verlangen.

## Umsetzung und Abnahme

Vorhandene Generatoren über die genannten Blocker vervollständigen; keine Place-Sondermodelle.
Belegte OSM-/DEM-Strukturen erhalten, unbekannte Details regelbasiert und seeded ergänzen.
Referenzbilder sind Prüfmittel, niemals Textur, Skybox oder Generatorinput.

- [ ] Sportflächen, niedrige Hallenmasse und Wohnstaffelung bleiben im kalibrierten Bild erkennbar.
- [ ] Keine kahlen Farbflächen statt Sportanlage, überhöhten Ersatzblöcke oder einheitlichen
      Baumkugeln. Kamerafahrt zeigt stabile Feldlinien und lückenlose Vegetations-LOD.
- [ ] Webcam und neues Client-PNG selbst nebeneinander öffnen; Gesamtbild und Ausschnitte
      der oben benannten Flächen beurteilen. Jeden Punkt mit Bildbeleg annehmen oder offen lassen.
- [ ] Manifest enthält Commit, Datenstände, Seed, Kamera, Zeit, Wetter, Features, Auflösung,
      Bildhash und Crop. Webcam-Zeitstempel aus Metadaten prüfen, nicht nur aus Dateinamen ableiten.
- [ ] Kamerafahrt sowie Zeit-/Wetterwechsel belegen räumliche und zeitliche Stabilität;
      Zielauflösung und Streaming-/Framebudget nach 2169/2092 bestehen mit voller Szene.
- [ ] Neue Daten/Seed-Variante prüft Übertragbarkeit. Fehlende belegte Daten ausdrücklich
      ausweisen; plausible Ersatzformen nicht als Rekonstruktion realer Details abnehmen.
