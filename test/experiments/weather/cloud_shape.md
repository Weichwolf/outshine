# Begrenzte native Wolkenkörper

#393 ergänzt das glatte Stratusrezept um ausdrücklich gewähltes
`CloudProfile::CumulusV1`. `CloudLayer` und `LowCloudRecipe` behalten Stratus
als Default. Ein positiver Low-Cloud-Snapshot benötigt weiterhin ein explizites
Geometrie-/Windrezept; die Form ist prozedural, keine beobachtete Wolkenart.
Bei positiver Bedeckung scheitern ungültige Rezeptversionen vor der Zustandsproduktion. Snapshot,
Quellbytes, Höhe und Zehnmeterwind werden dadurch nicht ergänzt oder verändert.

## Form und gemeinsamer Vertrag

Die neue Form vereinigt begrenzte, unterschiedlich hohe Halbellipsoide mit
gemeinsamem dünnem Bodenverlauf. Eine vorhandene dreidimensionale Noise erodiert
die Körper. Ein kubischer Dichteverlauf vermeidet die harten Knicke des ersten
Kandidaten. Dichte bleibt nichtnegativ, höchstens1,5×ExtinctionPerM und innerhalb
derselben gekrümmten Schale. Wind verschiebt Körper und Erosion gemeinsam.

Bei Cover0 bleibt die Schicht transparent. Ab Cover0,75 wächst ein Stratusboden
kontinuierlich dazu; bei Cover1 schließt er die Zwischenräume. Beide Anteile und
ihre maximale Dichte sind monoton in Cover. Das Rezept behauptet keine exakt
kalibrierte meteorologische Flächenbedeckung.

Neun lokale Zellen reichen vollständig: Feature-Jitter beträgt höchstens0,3
Zellbreiten, maximaler Radius0,805. Eine Zelle außerhalb des3×3-Fensters ist in
mindestens einer Achse wenigstens1,2 Zellbreiten entfernt. Diese Schranke betrifft
die endliche Nachbarschaft, keine globale Qualitäts-/Kostenoptimalität.

Die bestehenden48B-CloudLayer-/96B-Weather-Payloads behalten ihre Größe und
Bindings. `field.y=0` bezeichnet Stratus, `field.y=1` CumulusV1. CPU-Recipe und
GPU-Packing tragen dieselbe Form; Blick, Sonne und Momente konsumieren dieselbe
`cloudExtinction`. Der Selektor bezeichnet auch die Rezeptversion und gehört
zusammen mit Wetter-/Zeitparametern zur Gültigkeit von Quellenfeldern und History.
Light bestätigt dies unter6205369938, Performance unter6205660587 in #393.

## Vergleich und Auswahl

`cloud_shape_compare.py` vergleicht auf ursprünglichen #391-Kamerastrahlen
Stratus, zusätzliche fBm/Erosion, harte Zellkörper, fein erodierte Zellkörper und
das glatte Körperrezept. Kamera, Wind, Medium und Licht bleiben gleich. Der
Stratuskontrollfall muss die ursprünglichen nativen RGB-Werte innerhalb1e-4
reproduzieren. `cloud_shape_quality.py` trennt identische Dichtepunkte und endliche
Blick-/Sonnenverfeinerungen. Hohe Frequenz und harte Dichte sind bei16/6 verworfen;
die reduzierte Hashanzahl allein genügt nicht zur Auswahl.

Der neue native Bildvergleich verwendet32 Blick-/12 Sonnenproben. Ein zusätzlicher
Stratuslauf bei identischer Abtastung trennt den Formgewinn von Samplingeffekten.
Der bisherige16/6-Default bleibt erhalten. Eine endliche128/48-Referenz liefert
keine globale Konvergenz- oder Framebudgetgarantie; Grenzen bleiben #264/#193.

## Reproduktion

NumPy/Matplotlib, C++23, glslangValidator und reguläres passendes Enginearchiv
einschließlich Produktionsabhängigkeiten verwenden. Alle Ausgaben ins System-Temp.
Die neue CPU-Struktur benötigt ein frisch gebautes passendes Archiv/Headerpaar.

```sh
python3 test/experiments/weather/cloud_view_native.py \
  --archive-root /path/to/regular/checkout --cumulus-sky \
  --view-samples 32 --light-samples 12 --output /tmp/outshine-cumulus-native
nice -n 10 python3 test/experiments/weather/cloud_shape_raster.py \
  --captures /tmp/outshine-cumulus-native --view-samples 32 --light-samples 12 \
  --output /tmp/outshine-cumulus-checked
python3 test/experiments/weather/cloud_density_native.py \
  --archive-root /path/to/regular/checkout --output /tmp/outshine-density-native
```

`--stratus-sky` erzeugt dieselben27 deklarierten Kontrollfälle mit Profil0.
Der native Host kann anschließend mit1280/720 und `sky-broken` aufgerufen werden;
sein privater Modus bleibt ausdrücklich `cumulus-sky` beziehungsweise `stratus-sky`.
Beide verwenden den bestätigten Boundary-Sky-Vertrag aus `cloud_sky_raster.md`.

Die Dichteprobe prüft identische Punkte und speichert alle tatsächlichen
Dispatch-Timestamp-Paare einschließlich erster Ausführung. `--untimed` erlaubt
Wertgegenproben ohne Timestampunterstützung; keine Fence-Zeit als GPU-Zeit.
Weniger Hasharbeit bedeutet hier ausdrücklich nicht automatisch kürzere GPU-Zeit.
Die isolierte RTX3060-Probe mit1.048.576 identischen Punkten misst bei Cover0,45
rund96µs für CumulusV1 gegenüber73µs für Stratus. Der alte Shader ohne Profilzweig
benötigt rund67µs; der dynamische Selektor erhöht hier auch die Standardkosten um
rund9%. Zwei Ablauf-/Schleifenvarianten brachten keinen Gewinn und wurden verworfen.
Diese Mehrkosten bleiben ausdrücklich Teil der Integrationsentscheidung; die
reduzierten Hashzähler sind kein Beschleunigungsnachweis.
Die Probe misst weder Wolkenquellen noch Rasterpass, vollständigen Frame oder p99.

## Quellen und offene Integration

Schneider/Vos, SIGGRAPH2015: vorhandene PDF-Seiten31/38/78 mit Worley-/Körperformen,
Höhenprofilen und Rand-Erosion optisch geprüft. Eigene begrenzte analytische
Körperform, kein vollständiger Perlin-Worley-/Horizon-Nachbau und keine3D-Textur.
`doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf`.
Produktive Sky-/Opaque-/Thin-/Historyanbindung, vollständige Solar-/Psi-Beleuchtung,
UTC-/Wetterentwicklung, Places und Gerätebudgets bleiben offen. Diagnosen bleiben
privat; ausschließlich Master publiziert die integrierten Placebilder.
