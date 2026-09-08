Type: feature
State: open
Parent: 2169
Area: world, render, client
Tags: webcam, acceptance
Depends: 2092, 2111, 2133, 2138, 2154, 2166, 2170, 2171, 2172, 2173, 2175, 2176, 2195

# Graz: Bahnraum, Industrie und bewaldeter Stadthügel

## Referenz und Vertrag

Sichtprüfung der Webcam am 2026-09-08; dies ist das noch offene SOLL, keine Render-Abnahme.
Quelle: `build/shots/webcam/helmut-list-halle_2026-09-07_1240.jpg` (6000 × 4000 px, aus dem Bildheader).
SHA-256: `dd86eabd5c81ddc8a89c792c551334ff486e5a25b80ad4d8a4069193f5cae23e`.
Place: `Graz` in `src/client/PlaceCamera.cpp`; IST: `build/shots/places/Graz-*.png`.
Pose/Intrinsics aus WI 2170 übernehmen und im Vergleichsmanifest fixieren; vorhandene
Höhe/Pitch sind Schätzungen. Gleiche kalibrierte Kamera, unverzerrtes Seitenverhältnis.
Gemeinsamer Abnahmevertrag und Integrationsreihenfolge: WI 2169. RDR2/GTA5 auf PS4 sind
visueller Maßstab, keine Behauptung über proprietäre RAGE-Technik oder ein exaktes Zielbild.

## Erwartetes Gesamtbild

Vorne ein zusammenhängender Bahn-/Industriekomplex mit Gleisen und großer Hallendachfläche.
Dahinter mischen sich rote Wohndächer und höhere, verschiedenfarbige Wohnbauten. Rechts der
Bildmitte steht der bewaldete Stadthügel mit gegliederter Kuppe vor einem niedrigen Fernhorizont.

## Sichtbare Anforderungen

- Gleise bilden getrennte Schienen auf Schwellen und Schotter, mit nachvollziehbaren Weichen,
  Radien und Höhen. Oberleitungsmasten und Leitungen folgen dem Gleisraum und haben reale Auflager.
- Hallen besitzen breite, niedrige Volumen, Dachrippen/Oberlichter, Wandpaneele und Tore.
  Stahl, beschichtetes Blech, Glas, Beton und Schotter besitzen passende MR-Oberflächen.
- Mehrgeschossige Wohnbauten haben eigenständige Höhen und Fassadenrhythmen; kleinere Häuser
  vermitteln zum Straßenraum. Keine gleichförmige Extrusion aller Nutzungen.
- Schlanke hohe Laubbaumformen zwischen Hallen ergänzen rundere Kronen und den geschlossenen
  Hügelbestand. Bewaldete Hänge dürfen nicht durch helle nackte Gelände-Dreiecke ersetzt werden.
- Der Hügel behält seine DEM-Großform und belegte Silhouetten. Seine falsche Bildlage zuerst
  mit Intrinsics, Pose und Höhenbezug prüfen, getrennt von Stamping/Tessellation.
- Tageslicht modelliert Fassaden und Hallendächer ohne ausgebrannte Bleche; ferner Stadtgrund
  verliert Kontrast. Züge und Betriebsausstattung sind plausible generierte Population.

## Umsetzung und Abnahme

Vorhandene Generatoren über die genannten Blocker vervollständigen; keine Place-Sondermodelle.
Belegte OSM-/DEM-Strukturen erhalten, unbekannte Details regelbasiert und seeded ergänzen.
Referenzbilder sind Prüfmittel, niemals Textur, Skybox oder Generatorinput.

- [ ] Hügel rechts der Mitte und Gleis-/Hallenachsen dienen als getrennte Kalibrieranker.
- [ ] Gleisfahrt und Weichen-Nahansicht zeigen konsistente logische Verbindungen und Geometrie;
      keine kreuzenden Höhen ohne Bauwerk, schwebenden Leitungen oder kameraabhängig versetzten Hügel.
- [ ] Webcam und neues Client-PNG selbst nebeneinander öffnen; Gesamtbild und Ausschnitte
      der oben benannten Flächen beurteilen. Jeden Punkt mit Bildbeleg annehmen oder offen lassen.
- [ ] Manifest enthält Commit, Datenstände, Seed, Kamera, Zeit, Wetter, Features, Auflösung,
      Bildhash und Crop. Webcam-Zeitstempel aus Metadaten prüfen, nicht nur aus Dateinamen ableiten.
- [ ] Kamerafahrt sowie Zeit-/Wetterwechsel belegen räumliche und zeitliche Stabilität;
      Zielauflösung und Streaming-/Framebudget nach 2169/2092 bestehen mit voller Szene.
- [ ] Neue Daten/Seed-Variante prüft Übertragbarkeit. Fehlende belegte Daten ausdrücklich
      ausweisen; plausible Ersatzformen nicht als Rekonstruktion realer Details abnehmen.
