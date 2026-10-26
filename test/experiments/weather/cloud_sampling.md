# Wolkenstreifen: Blickzellen und Lichtquelle trennen

[#264](https://github.com/Weichwolf/outshine/issues/264) besitzt den Bildfehler.
Die native Diagnose verwendet dieselbe deklarierte Schicht, Kamera bei1772m,
Windphase37s und feste Luft-LUTs aus #300. Sie ist kein realer Wetterbericht oder
Place. Alle Bilder haben1280×720 Pixel und bleiben lokal im Systemtemp.

Mehr Sonnenproben bei unverändert16 Blickproben beseitigen die Streifen nicht.
Ohne den direkten Wolken-Sonnenterm verschwinden sie weitgehend. Mehr Blickproben
verbessern den oberen Bildbereich deutlich; selbst128 Blick-/32 Sonnenproben
erhalten jedoch feine horizontale Strukturen. Weitere reine Abtasterhöhung darf
diese verbleibenden Modell-/Quellen-/Flächenfragen nicht als gelöst ausgeben.

## Gemessene Vergleiche

`cloud_sampling_compare.py` prüft1200 Pixelrichtungen aus einem1280×720-Bild:
acht Spalten, alle Zeilen236–385. Fälle: unter/in/über der Schicht, Windtick
bei30Hz, Kamera um1m versetzt, eine Minute Wind und5° Drehung.512→1024 Blickproben
bei derselben6-Tap-Lichtquelle liefern eine endliche Verfeinerungsreferenz;
sie ist kein exaktes Integral oder Pixel-Flächenmittel. Unter der Schicht beträgt
deren maximaler RGB-Unterschied7,61e-5; andere Fälle erreichen3,30e-4.
Kleinere Unterschiede sind damit nicht eindeutig aufgelöst.

| Blickproben / Verteilung | RGB-RMSE unter der Schicht | T-RMSE |
| --- | ---: | ---: |
|16 linear |0,004520|9,00e-6|
|16 quadratisch |0,002605|5,56e-5|
|32 quadratisch |0,000810|3,78e-6|
|64 quadratisch |0,000216|1,29e-6|

Ein einzelner Beer-Schwerpunkt als neuer Quellpunkt verschlechtert16 linear auf
0,005723 RGB-RMSE. Dieser Quellpunkt wird verworfen: Dichte und Quelle dürfen in
groben, variierenden Zellen nicht als unabhängig behandelt werden. Die Option
bleibt ausschließlich als Python-Gegenversuch ausführbar.

Eine zweite Sparhypothese setzt28 quadratische Blick-/3 Gauss-Sonnenproben gegen
16 lineare Blick-/6 Mittelpunkt-Sonnenproben: bei einem Sonnenintervall jeweils
112 nominelle Dichteabfragen. Der gekoppelte Kohortenfehler sinkt auf0,001103.
`cloud_sampling_source.py` verwirft die Regel dennoch als allgemeine Lichtlösung:
An3242 nichtleeren Dichtepunkten steigt bei Sonnen-Cos0,8 der Cloud-T-RMSE gegen
128 Schattenproben von0,001645 auf0,004584. Bei flacher Sonne sind selbst64→128
Referenzen teils unkonvergiert. Planetensichtbarkeit ist ausgeschlossen; negative
Sonnenhöhe behauptet kein sichtbares Direktlicht. Der native Gauss-Gegenversuch
lebt nur in `cloud_sampling_gauss3.glsl`; Produktionsshader/Defaults bleiben.

Vollständige native Bilder gegen128 quadratische Blick-/32 Sonnenproben bestätigen
einen Teilgewinn: RGB-RMSE0,001330 bei16 linear,0,000322 bei32 quadratisch.
Diese Aufnahme ist kein konvergiertes Flächenorakel.32/6 benötigt mehr Arbeit;
ohne Dispatch-/Frame-/A18-Beleg wird es kein neuer Runtime-Default. Gemeinsame
Wolken-/Schatten-/Psi-Komposition bleibt #193/#266.

## Reproduktion und Grenzen

NumPy und Pillow erforderlich; private Ausgaben explizit ins Systemtemp legen.

```sh
sampling_out="${TMPDIR:-/tmp}/outshine-cloud-sampling-$(git rev-parse --short HEAD)"
PYTHONDONTWRITEBYTECODE=1 nice -n 10 python3 -W error \
  test/experiments/weather/cloud_sampling_compare.py --output "$sampling_out/cohort"
PYTHONDONTWRITEBYTECODE=1 nice -n 10 python3 -W error \
  test/experiments/weather/cloud_sampling_source.py --output "$sampling_out/source"
python3 -W error test/experiments/weather/cloud_air_native.py \
  --air-root "$AIR_ROOT" --probe "$AIR_PREFIX_PROBE" --output "$sampling_out/native32" \
  --warp 2 --view-samples 32 --light-samples 6
```

Der [native Replay](cloud_air_prefix.md) behält ursprüngliche numerische Grenzen.
Arbeitsobergrenzen folgen den ausdrücklich gewählten Probenzahlen;16/6 behält32
Wolkenzellen/68 Treiberschritte.256 Zellen müssen das ungültige Packed-Counter-Bit
setzen: Überlauf darf kein kleiner Aufwand werden. Bildbelege speichern logische
Zellsummen und Spitzen, keine GPU-Zeit. Queue-/Upload-/Readback-Wall ersetzt keine
Dispatchzeit; deren konkreter nativer Verbrauch ist unter #243 angefragt.
Keine neue History, Bindung oder Placefreigabe.

Grundlagen: Schneider/Vos, SIGGRAPH2015, lokale Horizon-PDF-Seiten83/94 erneut
optisch geprüft: längere Horizontstrahlen und temporale Amortisierung. Letztere
benötigt gültige Bewegung/History und wird hier nicht vorgetäuscht.
Gauss-Knoten/-Gewichte stammen aus der
[NumPy-Primärimplementierung](https://numpy.org/doc/stable/reference/generated/numpy.polynomial.legendre.leggauss.html);
die exakte Polynomregel ist kein Genauigkeitsbeleg für das geclippte Rauschfeld.
