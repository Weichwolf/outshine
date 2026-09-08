Type: feature
State: open
Parent: 2169
Area: world, render, client
Tags: webcam, acceptance
Depends: 2092, 2111, 2129, 2133, 2138, 2145, 2154, 2170, 2171, 2172, 2175, 2176, 2195

# Wien: Flussinseln, tragende Brücke und Stadtpanorama

## Referenz und Vertrag

Sichtprüfung der Webcam am 2026-09-08; dies ist das noch offene SOLL, keine Render-Abnahme.
Quelle: `build/shots/webcam/wien_2026-09-07_1240.jpg` (6000 × 4000 px, aus dem Bildheader).
SHA-256: `c67cdb7a923314c81ecb0dd18aacecff3203998d974077644f863e419bc59d8a`.
Place: `Wien` in `src/assets/places/Wien.scenario`; IST: `build/shots/places/Wien-*.png`.
Pose/Intrinsics aus WI 2170 übernehmen und im Vergleichsmanifest fixieren; vorhandene
Höhe/Pitch sind Schätzungen. Gleiche kalibrierte Kamera, unverzerrtes Seitenverhältnis.
Gemeinsamer Abnahmevertrag und Integrationsreihenfolge: WI 2169. RDR2/GTA5 auf PS4 sind
visueller Maßstab, keine Behauptung über proprietäre RAGE-Technik oder ein exaktes Zielbild.

## Erwartetes Gesamtbild

Ein weiter Himmel über einer niedrigen Stadt und fernen, bläulich gestaffelten Hügeln.
Die Brücke zieht diagonal von links unten in die Bildtiefe. Wasserarme und eine langgestreckte,
bewachsene Insel bilden getrennte Ebenen; hinter ihnen liegt eine dichte, unterschiedlich hohe Stadt.

## Sichtbare Anforderungen

- Die Brücke besitzt Fahrbahnaufbau, tragende Unterseite, Widerlager, Pfeiler, Geländer und
  maßstäbliche Leuchten. Rampen schließen höhenstetig an; Wasser und Wege laufen darunter weiter.
- Insel und Ufer tragen zusammenhängende Baumgruppen, offene Wiesen, Wege und plausible kleine
  Nutzbauten. Böschungen treffen den Wasserspiegel ohne Spalten oder senkrechte DEM-Vorhänge.
- Flusswasser zeigt räumlich kohärente Windwellen, gebrochene Himmel-/Uferreflexionen und den
  Brückenschatten. Ruhige und exponierte Abschnitte unterscheiden sich ohne sichtbare Materialkacheln.
- Stadtmassen, vereinzelte Türme und die rote Kirchenform jenseits des Flusses geben Tiefe;
  belegte Landmarkenlagen erhalten, fehlende Architektur generisch plausibel ergänzen.
- Helle Tagesluft lässt die Ferne verblauen; Vegetation und Brückenunterseite behalten Zeichnung.
  Verkehr und Boote entstehen aus der Sandbox, einschließlich plausibler Kontakte und Bewegung.

## Umsetzung und Abnahme

Vorhandene Generatoren über die genannten Blocker vervollständigen; keine Place-Sondermodelle.
Belegte OSM-/DEM-Strukturen erhalten, unbekannte Details regelbasiert und seeded ergänzen.
Referenzbilder sind Prüfmittel, niemals Textur, Skybox oder Generatorinput.

- [ ] Brückenachse, Inselkontur und hinteres Ufer stimmen nach Kalibrierung als Bildanker.
- [ ] Unteransicht beweist tragende Konstruktion und freie Durchfahrt; Render-LOD verändert
      keine logische Verbindung. Kein dünnes Brückenband, kahler Inselkörper oder schwarzes Wasser.
- [ ] Webcam und neues Client-PNG selbst nebeneinander öffnen; Gesamtbild und Ausschnitte
      der oben benannten Flächen beurteilen. Jeden Punkt mit Bildbeleg annehmen oder offen lassen.
- [ ] Manifest enthält Commit, Datenstände, Seed, Kamera, Zeit, Wetter, Features, Auflösung,
      Bildhash und Crop. Webcam-Zeitstempel aus Metadaten prüfen, nicht nur aus Dateinamen ableiten.
- [ ] Kamerafahrt sowie Zeit-/Wetterwechsel belegen räumliche und zeitliche Stabilität;
      Zielauflösung und Streaming-/Framebudget nach 2169/2092 bestehen mit voller Szene.
- [ ] Neue Daten/Seed-Variante prüft Übertragbarkeit. Fehlende belegte Daten ausdrücklich
      ausweisen; plausible Ersatzformen nicht als Rekonstruktion realer Details abnehmen.
