Type: feature
State: open
Architecture: planned
Priority: P2
Parent: 2169
Area: world, render, client
Tags: webcam, acceptance
Depends: 2092, 2111, 2129, 2137, 2138, 2144, 2145, 2154, 2166, 2167, 2170, 2171, 2172, 2176, 2195

# Koerbersee: Alpine Geländeformen, Bergsee und standortgerechte Vegetation

## Referenz und Vertrag

Quelle: https://www.foto-webcam.eu/webcam/koerbersee/ mit Archiv.
Gesichtete Jahres-/Tageszeiten und Einzelmetadaten stehen unter
build/shots/reference/webcams/archive-review-20260929/; src/assets/places/references.json
verknüpft sie mit dem Place. WI 2324 hält Auswahl und Herkunft, WI 2170 den Kamerafit.
Archivzeit ist nicht Abrufzeit. Pose/Höhendatum und Wettergleichheit sind noch nicht
vollständig kalibriert; daraus keine pixelgenaue Übereinstimmung behaupten.

## Erwartetes Gesamtbild

Ein dunkler, reflektierender Bergsee liegt links unter steilen, gegliederten Bergflanken.
Vorne formen wellige Almwiesen, einzelne schlanke Nadelbäume, Gruppen und kleine Gebäude den
Maßstab. Helle Schutthänge, grauer Fels und grüne Rücken greifen räumlich ineinander.

## Sichtbare Anforderungen

- Linke Felswand und Schuttkegel, mittlere Rinnen und rechter grüner Rücken behalten ihre
  Silhouetten. Tessellation verfeinert auch steile Seiten und den nahen Geländeabfall rechts.
- Fels besitzt unregelmäßige Brüche, Bänder und Vorsprünge; Schutt liegt plausibel unter
  Abbrüchen. Hangneigung allein macht weder jeden Steilhang zu Fels noch jede Fläche zu Gras.
- Wiesen zeigen Geländewellen, kleine Felsen, Pfade und standortgebundene Bodendeckung.
  Detailmaßstab und Übergänge bleiben über LOD konsistent; keine großen RGB-Farbpatches.
- Nadelbäume stehen einzeln und gruppiert in unterschiedlicher Höhe; dichterer Wald rechts
  erhält Randstruktur und Lücken. Kleine Hütten und Lodgeformen sitzen ohne schwebende Sockel.
- Seeufer ist unregelmäßig, mit flachem Kontakt zur Vegetation. Dunkle Berg-/Waldreflexionen,
  Himmelanteil und Windwellen erklären die Wasserfarbe; keine konstant schwarze Scheibe.
- Rinnen bleiben dunkel, aber lesbar; direkte Sonne, Himmelsfüllung und Kontaktschatten
  modellieren Tiefe. Ferne Felsformen verlieren Kontrast, behalten ihre charakteristischen Grate.

## Umsetzung und Abnahme

Vorhandene Generatoren über die genannten Blocker vervollständigen; keine Place-Sondermodelle.
Belegte OSM-/DEM-Strukturen erhalten, unbekannte Details regelbasiert und seeded ergänzen.
Referenzbilder sind Prüfmittel, niemals Textur, Skybox oder Generatorinput.

- [ ] Seeumriss, Bergsattel und Schuttkegel sind kalibrierte, unabhängige Großformanker.
- [ ] Seitenansicht und Kamerafahrt zeigen keine aufgeblähten Hänge, Falten, Nahtspalten,
      Baumscheiben oder abrupt wechselnden Felsmaßstäbe; See reflektiert die vorhandene Szene.
- [ ] Webcam und neues Client-PNG selbst nebeneinander öffnen; Gesamtbild und Ausschnitte
      der oben benannten Flächen beurteilen. Jeden Punkt mit Bildbeleg annehmen oder offen lassen.
- [ ] Manifest enthält Commit, Datenstände, Seed, Kamera, Zeit, Wetter, Features, Auflösung,
      Bildhash und Crop. Webcam-Zeitstempel aus Metadaten prüfen, nicht nur aus Dateinamen ableiten.
- [ ] Kamerafahrt sowie Zeit-/Wetterwechsel belegen räumliche und zeitliche Stabilität;
      Zielauflösung und Streaming-/Framebudget nach 2169/2092 bestehen mit voller Szene.
- [ ] Neue Daten/Seed-Variante prüft Übertragbarkeit. Fehlende belegte Daten ausdrücklich
      ausweisen; plausible Ersatzformen nicht als Rekonstruktion realer Details abnehmen.
