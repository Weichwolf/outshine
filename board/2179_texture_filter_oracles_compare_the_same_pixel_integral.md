Type: bug
State: active
Area: test, harness, render
Tags: khronos, measured

# Texture filter oracles compare the same pixel integral

## Befund

Beim Mip-Schritt in 2171 wurde der historische Blocker 1130 erneut gelesen. Dessen
ABeautifulGame-Wiederholungsprüfung ist nicht mehr über `make suite` enumeriert:
`make suite SUITE=khronos/glTF/ABeautifulGame` verweigert den vorhandenen Manifestfall.
`make corpus-render CASES=ABeautifulGame` rendert dagegen und vergleicht PNGs.

Baseline af20ca1f ohne Mips: 97.9320 % innerhalb 8 Codes, 735 abweichende Pixel,
Digest 04937f96 (`build/mip-chess-render-before.log`, Exit 2).
Mit flächenintegrierten Mips: 80.7462 %, 6843 Pixel, Digest 76fd0957; Wiederholung
identisch (`build/mip-chess-render-after.log` und `build/mip-corpus-repeat.log`).
Die bestehende 99.99-%-Schranke bleibt unverändert und rot.

ABeautifulGames Manifest beschreibt 1 Sample durch einen 0.01-Pixel-Boxfilter.
SimpleTexture/four-texels-per-pixel beschreibt dagegen 256 Samples über eine ganze
Pixelfläche und begründet ausdrücklich, weshalb Punktabtastung keine Mip-Filterabnahme ist.
Dieser integrierte Fall wird vom heutigen scenario_for mit
`ValueError: unsupported oracle world: factory` abgewiesen. Ein vorhandener Prüfauftrag
ist damit unerreichbar. Nicht durch einen anderen Grauwert oder einen größeren Grenzwert ersetzen.

## Lösung und Nachweis

Unreal/RAGE sind Vergleich für gefilterte Darstellung; die mathematische Referenz hier
ist das Integral des identisch deklarierten Signals. Vorhandene Blender-Provenienz und
Manifest nutzen, keine neue Engine-eigene Soll-PNG als unabhängiges Oracle ausgeben.

1. Factory-World aus der tatsächlich gemessenen Provenienz übertragen; vorhandene
   Default-Konstante nur verwenden, wenn deren Gültigkeit belegt ist. Kamera, Material-
   Closure, Farbraum und Belichtung ebenfalls identisch deklarieren. Diffuse ≠ MR mit
   verbliebener Specular-Lobe; diese Unterscheidung im Übersetzer prüfen.
2. Den vorhandenen four-texels-per-pixel-Fall durch Make ausführbar machen. Pixelintegral
   gegen Mip-Kette prüfen; fehlende Kette und Zweifarben-Rundung als echte Mutationen rot.
3. ABeautifulGame mit entsprechend deklarierter, unabhängig integrierter Referenz
   zusätzlich abnehmen. Die alte Punktreferenz mit ihrem Auftrag erhalten. Erst anhand
   der Messung entscheiden, welche bisherigen Checks tatsächlich falsch spezifiziert sind.
4. Wiederholbarkeit separat auf linearen Pixeln prüfen. Die neue native Konventionsfixture
   deckt Unlit/Farbmaps ab; beleuchtete Normal-/MR-Maps und bewegte Frames ergänzen.

- [ ] Integrierter vorhandener Fall läuft statt Translation-Exception.
- [ ] Gleiche Kamera, Beleuchtung, Closure und Integralpopulation belegt.
- [ ] Positive/negative Filterkontrollen und unabhängig gerenderte PNGs geprüft.
- [ ] Schachbrett wiederholt lineare beleuchtete Frames; verbleibende Fehler ursächlich benannt.

Keine Änderung an Vendor-Manifesten, Oracles oder Akzeptanzschranken im bisherigen Mip-Schritt.

## Wiederholungsfehler erneut reproduziert, 2026-09-08

Beim gemeinsamen Piece-Instanzpfad schlägt MipmappedChessRepeatsLinearPixels wieder fehl.
`build/shared-piece-index-proof.log` und unveränderte Produktquellen in
`build/shared-piece-repeat-probe.log`: drei Wiederholungen unterscheiden sich jeweils
in 420 Kanälen vom ersten Frame, maximal 0.00268555 linear, erster Kanalindex 1635972.
Die erwartete Gleichheit bleibt unverändert. First-/Repeat-PNG werden getrennt gespeichert.

Nächste kausale Probe: nur Occlusion im Cull-Uniform temporär deaktivieren. Der erste Frame
hat keine vorherige Tiefenpyramide, spätere Frames schon. Falls die Differenz verschwindet,
ist die Übergangshypothese gestützt; das wäre keine Erlaubnis, Occlusion dauerhaft zu entfernen.
Andernfalls die Hypothese verwerfen und Upload-/Matrixreihenfolge weiter isolieren.

## Kontrollbefunde, 2026-09-08

Die Occlusion-Probe `build/shared-piece-no-occlusion-control.log` endet mit Exit 0,
18/18 PASS; alle drei Wiederholungen haben null abweichende lineare Kanäle.
Occlusion ist danach wieder aktiviert; Abschalten ist keine Reparatur.
`build/shared-piece-depth-probe-fixed.log` endet rot (17/18 PASS): sämtliche
1280×720 Tiefenwerte bleiben gleich, die drei Farbvergleiche behalten jeweils
420 Abweichungen und maximal 0.00268555. Gleiche Tiefe allein beweist keine
identische Zuordnung koplanarer Oberflächen.

Eine isolierte Probe ersetzt im Unlit-Fragmentshader den impliziten Texture-Tap
durch textureGrad mit dFdxFine/dFdyFine. `build/shared-piece-fine-derivative-probe.log`
endet mit Exit 2, 17/18 PASS, exakt denselben Farb- und Tiefendifferenzen.
Diese Änderung ist zurückgenommen: explizite feine Ableitungen lösen den Fehler nicht.
Als Nächstes feste Mip-Stufen als temporäre Kontrolle und Oberflächenzuordnung
vergleichen, um Filter- von Draw-/Interpolationsänderungen zu trennen; keine
dauerhafte Mip-Abschaltung oder Lockerung der Gleichheitsprüfung.

Die Instanz-PNGs (instanced/clustered) wurden geöffnet: jeweils drei erwartete,
getrennte Dreiecke; die Prüfung überlebt die Freigabe der ersten Gruppe bei
weiter sichtbarer Nachbargeometrie. Das ist eine Übergabeprüfung, kein Nachweis
für fertigen Wald oder fotorealistische Places. Die Schach-PNG wurde ebenfalls
geöffnet; die kleinen Kanalabweichungen sind damit noch nicht ursächlich erklärt.

## Feste Mip-Stufe als Negativkontrolle, 2026-09-08

`build/shared-piece-fixed-lod-probe.log`: ausschließlich den Unlit-Farbtap auf
textureLod(..., 0) gestellt, Occlusion aktiv gelassen. Schachbrett: dreimal null
abweichende Farbkanäle und Tiefenwerte. Die Suite endet dennoch korrekt rot
(Exit 2, 17/18 PASS): NativeMipImagesReachTheRenderer verliert das geforderte
Pixelintegral. Beide PNGs wurden geöffnet; der minifizierte Checker zeigt wieder
Streifen. Die feste Stufe ist zurückgenommen, keine Produktlösung.

Damit ist der Wiederholungsfehler mipabhängig. Die bisherigen Messungen erklären
noch nicht, ob veränderte Ableitungen oder die nachgelagerte Abtastung entscheidend
sind. Nächste Probe: UV-Ableitungen selbst als Float-Ausgabe vergleichen, bevor
Interpolation, Texturzugriff oder Occlusion geändert werden.

## Ableitungs- und Identitätsausgaben, 2026-09-08

Temporär die vier Komponenten dFdxFine(mappedUv)/dFdyFine(mappedUv) zusätzlich
zum unveränderten Farbtap ausgeben; kein Austausch der Farbabtastung:

- `build/shared-piece-gradient-output-probe.log`, RGBA16F-Normalanhang:
  3.686.400 Ableitungskomponenten wiederholen gleich; Farben dreimal 423
  Abweichungen, max. 0.00268555. Exit 2, 17/18 PASS. Die Half-Quantisierung
  begrenzt die Aussage über die ursprünglichen Float-Ableitungen.
- `build/shared-piece-gradient32-output-probe.log`, RGBA32F-Identitätsanhang:
  Ableitungen, Tiefe und Farben wiederholen exakt. Exit 0, 18/18 PASS.
  Diese Instrumentierung verändert den beobachteten Fehler und beweist daher
  keine unveränderten Ableitungen des ursprünglichen Shaders.
- `build/shared-piece-identity-attachment-control.log`: ursprünglicher Shader,
  derselbe zusätzliche Identitätsanhang. Oberflächen-IDs und Tiefe wiederholen
  exakt, Farben erneut dreimal 420 Abweichungen, max. 0.00268555.
  Exit 2, 17/18 PASS. Der Anhang allein löst nichts. Im Log heißt der allgemeine
  Vergleichspuffer noch `gradient`; hier enthält er die regulären Identitätswerte.

Alle Diagnoseänderungen zurückgenommen. Die ID-Gleichheit grenzt wechselnde
Oberflächenzuordnung ein; die konkrete Shader-/Samplerursache bleibt offen.
Nächste kausale Untersuchung: Auswertung/Präzision der Ableitungen und deren
Compilerübersetzung. Keine permanente Diagnoseausgabe als vermeintliche Reparatur.
