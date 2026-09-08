Type: feature
State: open
Parent: 2169
Area: client, scene, world
Tags: webcam, measured
Depends:

# A reference camera has calibrated intrinsics and a declared height datum

## Befund und Grenze

Audit 2026-09-07, Commit 12ceb790, siehe 2169. `src/client/PlaceCamera.cpp` erklärt
neun Kameras mit geschätzter Höhe/Pitch, festem 1280×720 und vertikalem FOV. Die JPGs
haben unterschiedliche Seitenverhältnisse. Graz zeigt den Hügel links statt rechts der
Mitte; Feldkirch einen deutlich anderen Vordergrund. Das belegt eine Abweichung, noch
keine eindeutige fehlerhafte Kamerakomponente. DEM/Stamping können dieselbe Abweichung erzeugen.
`HeightAslM` wird an eine öffentliche geodätische Höhe übergeben: ASL und Ellipsoid müssen
an der Grenze explizit unterschieden werden. Der tatsächliche DEM-Provider-Datum ist zu prüfen.

## Implementierung

1. Referenzmanifest: Bildhash, ursprüngliche Breite/Höhe, Aufnahmezeit/Zeitzone,
   veröffentlichte Position/Bearing/Sektor samt Quelle, geschätzte Werte mit Unsicherheit.
   Kein Stretch eines 3:2-Fotos auf 16:9. Crop/Letterbox und Hauptpunkt deklarieren.
2. `PlaceCamera` und `include/`-Kamera-/Geodäsiegrenze: FOV eindeutig vertikal in Grad;
   vfov = 2 atan(tan(hfov/2) / aspect). Für Crops die Intrinsics mittransformieren.
   Rechtshändig, glTF-Kamera blickt lokal -Z, Up +Y; geodätische Basis explizit umrechnen.
3. Höhenquelle deklarieren; orthometrisch H nach ellipsoidisch h = H + N nur mit zum
   DEM passendem Geoidmodell. Kein pauschaler Höhenoffset je Place.
4. Pro Kamera mehrere statische, räumlich verteilte Korrespondenzen erfassen: Uferknicke,
   Brückenachsen, DEM-Gipfel. Gebäudedachhöhen nur bei belegtem OSM-Wert. Bearing, Pitch,
   Roll, FOV und Höhe begrenzt fitten; Restfehler und Parameterunsicherheit ausgeben.
   Zur Validierung Korrespondenzen zurückhalten. Stempel-freie DEM-Ansicht gegen finale
   Oberfläche trennt Kalibrierfehler von Geländezerstörung.

## Abnahme

- [ ] Alle neun Manifeste enthalten Herkunft/Datum/Intrinsics und unverzerrte Vergleiche.
- [ ] Synthetische bekannte Kamera wird innerhalb numerischer Toleranz rekonstruiert;
      absichtlicher Roll-/FOV-/Datumfehler erhöht den unabhängigen Projektionsfehler.
- [ ] Reale Restfehler in Pixeln samt Unsicherheit dokumentiert; keine harte Pixelgrenze
      unterhalb der DEM-/Metadatenauflösung. Keine poseabhängigen Gelände-Sonderfälle.

Wahl: übliche photogrammetrische Kalibrierung und glTF/Cesium-kompatible Grenze.
Unreal/RAGE liefern die visuelle Vergleichsklasse; ihre internen Webcam-Fits sind kein
verfügbarer Implementierungsvertrag. Die Sandbox erhält dadurch keine Foto-Abhängigkeit.
