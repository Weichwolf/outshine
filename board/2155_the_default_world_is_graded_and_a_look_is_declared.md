Type: feature
State: open
Parent: 2169
Area: render, scenario
Tags: webcam, measured
Depends: 2170, 2167, 2172

# A declared camera response preserves credible scene lighting

## IST / Ziel

Der aktuelle Renderplan meldet `autoExposure` ohne Body und aliasiert den Meter neutral.
Ein physikalisch glaubhafter Default-Look bleibt Ziel; mehr Grain oder Blur ist kein Beweis
für Fotorealismus. Die frühere Behauptung, ein bestimmter Tonemapper mache automatisch ein
Spielbild, wird durch getrennte lineare Licht- und Kameraabnahme ersetzt.

Künstlerischer Abnahmemaßstab: ein bewusst gestalteter, plausibler Look ist ein
vollwertiges Ziel und besser als verfehlter Realismus. Lichtführung, Farbgestaltung,
Tiefe und Materiallesbarkeit als Gesamtbild beurteilen, auch in Bewegung. RDR2/GTA5
bleiben Qualitätsreferenzen; keine Pflicht zur Fotokopie. Gewollte Vereinfachung ist
zulässig, Geometrie-/Lichtfehler zu kaschieren nicht.

Belichtungsmessung mit robuster Luminanzverteilung, zeitlicher Adaptation, deklariertem
EV-Bias und HDR-Shoulder implementieren. Ein Exposure gilt für Sky/Ground/Subjects/Water.
Linear-HDR-AOV und finale Display-Ausgabe sichern; Farbraum/Output-Transfer genau einmal.
Look bleibt deklarierbar und zeit-/wetterabhängig; Kameraoptik, Vignette, Grain und
Shutter sind nachvollziehbare Parameter, keine Place-spezifische Bildkosmetik.
Bewegungsunschärfe aus Simulationszeit/Belichtungsdauer, nicht schwankender CPU-Framezeit;
180° bei 60 Hz ergibt 0,5/60 s = 1/120 s. Bei eingefrorener Kamera kein erfundenes Motion Blur.

- [ ] Exposure-Reihe erhält Highlights/Farbverhältnisse, Adaptation pumpt bei Kameraschwenk
      nicht; lineare Beleuchtungsorakel bleiben vor Tone-Mapping prüfbar.
- [ ] Identity-Look reproduziert denselben aktuellen linearen Eingang bei gleicher Ausgabe-
      Transformation; kein Zwang, alte falsche Shader-Digests wiederherzustellen.
- [ ] Bedeckter Mittag, Sonne, Dämmerung, Nacht visuell abnehmen; keine Pflicht zur unbekannten
      Webcam-Automatik. Material-/Geometriefehler bleiben in scharfen Diagnosebildern sichtbar.
- [ ] Post-Kosten in 2092 inklusive Bewegung/History und kompletter Welt messen.

Wahl: Filament als physikalische Referenz und deklarative Zeit/Wetter-Looks als Sandbox-
Struktur; Unreal-CineCamera/RAGE-Look als Vergleich. Fassadengeometrie gehört in 2138/2171.
