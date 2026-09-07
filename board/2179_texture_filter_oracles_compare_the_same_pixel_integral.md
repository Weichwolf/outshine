Type: bug
State: open
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
