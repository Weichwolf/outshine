Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Area: client, scene, world
Tags: webcam, measured
Depends:

# Acht Webcam-Kameras mit nachvollziehbarer Projektion und Höhenreferenz

## Ergebnis

Webcam und Client zeigen dieselbe räumliche Ansicht ohne Bildstreckung oder
ortsabhängige Geländeänderung. Restfehler und unsichere Kameraparameter bleiben sichtbar.
Der Vergleich trennt falsche Pose von fehlerhafter Weltgeometrie.

## Vorhandene Fähigkeit und offene Grenze

PlaceCamera liest acht deklarative Szenarien unter src/assets/places; Position,
Bearing, Pitch und vertikaler FOV stehen in den Szenarien, nicht in einer Kameratabelle.
references.json enthält Archivquellen und vorläufige Kameraprovenienz. Kein Fit ist
abgenommen. Bildabmessungen, Crop/Hauptpunkt und verifizierte Zeitzone fehlen noch.
Flensburg übernimmt veröffentlichte Position, Bearing und 66 m Höhe; Pitch ist
geschätzt, Höhenbezug ungeklärt. Ein fehlendes vollständiges Rendering sperrt den Fit.

ResolveGeodeticCamera in src/engine/GeodeticCamera.h übernimmt heightM direkt;
bei samplesHeight addiert es GroundSample::AslM vor TangentFrame::ToLocalPosition.
Die ECEF-Umrechnung in src/base/geo/Geodesy.h verwendet HeightM als Ellipsoidhöhe.
Damit sind Kamera- und DEM-Höhenbezug noch kein durchgängig belegter Vertrag.
Eine Differenz beweist keinen pauschalen Place-Offset: Providerdatum zuerst prüfen.

Die frühere Graz-Nahwand entstand nachweislich durch eine falsche Terrain-Naht,
nicht durch die Kamera. Die reparierten Nachbarschaftsprüfungen bleiben erhalten.
Feldkirchs sichtbare Hangwand gehört bis zum Quellen-/Oberflächenvergleich in 2166;
Pose niemals zum Verstecken eines Geometriefehlers verändern.

## Architektur und Implementierung

1. Vergleichswerkzeug und references.json besitzen Referenzmetadaten: Originalmaß,
   Aufnahmezeit mit belegter Zeitzone oder ausdrücklich unbekanntem Offset,
   veröffentlichte Pose/Bildwinkel samt Quelle und Unsicherheit. Kein Foto im Runtimepfad.
2. Szenario-Projektion bleibt vertikaler FOV in Grad. Für ein horizontales Bildfeld gilt
   vfov = 2 atan(tan(hfov/2) / aspect). Crop und Hauptpunkt im Vergleich explizit
   transformieren; keine 3:2-Aufnahme auf 16:9 strecken. Nicht unterstützte Intrinsics
   melden statt stillschweigend mit einem symmetrischen Frustum gleichsetzen.
3. DEM-Provider und öffentliche Geodäsiegrenze erhalten belegte Höhenreferenzen.
   Orthometrisch H nach ellipsoidisch h = H + N nur mit passendem Geoidmodell;
   dessen Einführung braucht einen expliziten Quelldatenvertrag. Keine erfundenen N-Werte.
   Bis zur Klärung Höhenunsicherheit ausweisen und keine endgültige Kalibrierung behaupten.
4. Zuerst eine vollständig ladende Szene kalibrieren, danach alle acht: verteilte
   Uferknicke, Brückenachsen und DEM-Gipfel mit unabhängigen geodätischen Punkten
   zuordnen. Dachhöhen nur bei belegtem OSM-Wert. Bearing, Pitch und FOV innerhalb
   dokumentierter Unsicherheit fitten; Höhe erst nach geklärtem Datum freigeben.
   Roll nicht stillschweigend fitten, solange der Szenariovertrag ihn nicht abbildet.
5. Korrespondenzen für unabhängige Validierung zurückhalten. Restfehler in Pixeln und
   Parameterunsicherheit speichern; quellreines DEM gegen finale Erdarbeiten vergleichen.
   Unverfügbare Daten bleiben ein benannter Fehler, kein angenommener Nullfehler.

## Besitzer und unveränderliche Verträge

src/assets/places besitzt Szenarien und Referenzmanifest; test/scripts besitzt das
Offline-Vergleichswerkzeug. PlaceCamera lädt und validiert den Katalog. Scenario und
GeodeticCamera besitzen die Runtime-Kamera; Geodesy die Höhen-/Koordinatengrenze.
Keine Referenzfoto-Abhängigkeit der Engine, keine Place-Sondergeometrie, kein verdeckter
Kameraoffset. Bestehende Straßen-/Terrainanschlüsse und 360-Grad-Verfügbarkeit erhalten.

## Abnahme und Widerlegung

Alle acht Referenzen enthalten Provenienz, Bildmaß, Projektion und Höhenstatus.
Synthetische bekannte Kameras prüfen Projektion unabhängig; falscher FOV oder Datum
muss den Projektionsfehler erhöhen. Zurückgehaltene reale Punkte dürfen nicht durch
mehr freie Fit-Parameter scheinbar besser werden. Keine Pixelgrenze unterhalb der
Quellauflösung behaupten. Fehlendes Bild oder ungeklärtes Datum verhindert Abschluss.
make format; Kamera-/Szenario-Suiten; acht Places mit geöffnetem Vergleich; make lint.
