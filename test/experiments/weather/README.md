# Begrenzte Wolkenschicht

[#193](https://github.com/Weichwolf/outshine/issues/193) besitzt den ersten deklarierten
Wolkenfall unter #7/#191. Der Vergleich verwendet die vorhandene Körbersee-Kamera,
eine ausdrücklich prozedurale Schicht von 2572 bis 3172 m und Wind aus 240° mit 8 m/s.
Die Wetterwerte sind keine Beobachtung. Der Radius 6360000 m entspricht für diesen
Vergleich der bestehenden klaren Atmosphäre; die gemeinsame Planetenabstimmung besitzt Light.

`cloud_field.py` ist die NumPy-Referenz für Kugelschicht, Dichte und optische Tiefe;
`CloudLayer.h`, `CloudShell.cpp`, `CloudDensity.cpp` und `cloudLayer.glsl` bilden den
nativen Kern. Die auf eins normierte vertikale Parabel wird von zwei glatten
Rauschbändern moduliert. Diese begrenzte Stratus-/Aufbruchannahme ersetzt keine
allgemeine Cumulusmodellierung. CloudCover steuert die Rezeptschwelle, keine exakt
gemessene Flächenbedeckung. Herkunft bleibt deklariert/prozedural.

## Verfahren und Quellen

Schneider/Vos, *The Real-time Volumetric Cloudscapes of Horizon Zero Dawn*,
[SIGGRAPH-Kurs 2015](https://advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf),
liegt unter `doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf`.
Optisch geprüfte PDF-Seiten: 31/33 (Rauschformen), 54/56 (Beer/Streuung), 76
(Kugelschicht), 84/86 (Lichtabtastung). Die begrenzte Schicht übernimmt die
weltverankerte Dichte, endliche sphärische Schnittintervalle und Beer-Transmittanz;
die volle Perlin-Worley-/Mehrfachstreuungs-/History-Pipeline ist hier noch offen.

`cloud_comparison.py` vergleicht auf denselben Feldern 1/8/16/32 gegen 256 Mittelpunkte.
`cloud_countercases.py` ergänzt 512 höhenstratifizierte Richtungen, Beobachter unter,
in und über der Schicht sowie dünne Wolken, mit 512/1024-Sample-Konvergenz.
`cloud_composition.py` prüft die vereinbarte Erst-Extinktionskomposition gegen exakte
getrennte Luft-/Wolkenschichten. Dabei ist die Wolkenquelle Radiance pro Extinktion;
klare Vorderluft bleibt vor absorbierenden Wolken erhalten.

## Reproduktion

Erforderlich: NumPy, Pillow; C++23-Compiler und SDL3 für die unabhängige Vulkan-Probe,
der vorhandene gepinnte glslang-Compiler für SPIR-V. Alle privaten Ausgaben explizit
ins System-Tempverzeichnis legen. Schwere Vergleiche am gemeinsamen Host-Lock ausführen.

```sh
cloud_out="${TMPDIR:-/tmp}/outshine-weather-cloud-layer-$(git rev-parse --short HEAD)"
export PYTHONDONTWRITEBYTECODE=1
mkdir -p "$cloud_out"
python3 -W error test/experiments/weather/cloud_controls.py
python3 -W error test/experiments/weather/cloud_composition.py
flock -n /var/tmp/outshine-heavy.lock python3 -W error \
  test/experiments/weather/cloud_comparison.py --output "$cloud_out"
flock -n /var/tmp/outshine-heavy.lock python3 -W error \
  test/experiments/weather/cloud_countercases.py --output "$cloud_out"
"$GLSLANG" -V --target-env vulkan1.0 -Isrc \
  test/experiments/weather/cloud_probe.comp -o "$cloud_out/cloud_probe.comp.spv"
make build/cloud-probe
flock -n /var/tmp/outshine-heavy.lock build/cloud-probe \
  "$cloud_out/cloud_probe.comp.spv" "$cloud_out/broken-native.f32" 240 135 0.65
flock -n /var/tmp/outshine-heavy.lock python3 -W error \
  test/experiments/weather/cloud_comparison.py --output "$cloud_out" \
  --gpu-result "$cloud_out/broken-native.f32"
```

Die Compute-Probe verwendet die Produktions-GLSL-Datei und den 48-Byte-Uniform mit
geprüften std140-Offsets. Sie vergleicht echte Vulkan-Float32-Ergebnisse mit Float64,
schreibt genau einen angeforderten Ergebnisbuffer und meldet Encode-/Submission-/
Fence-Beobachtung getrennt. Diese Zeiten enthalten CPU/Dispatch/Download und sind
keine GPU-Passzeit. Die optische Diagnose ist kein Place-Bild.

## RGB- und Faltungsdiagnose

`cloudRadiance.glsl` und `cloud_radiance.py` ergänzen einen untersuchten RGB-Pfad.
Die Quelle verwendet normalisierte Henyey-Greenstein-Einfachstreuung mit g=0,65,
Sonnen-Transmittanz derselben Dichte und einen ausdrücklich heuristischen
Mehrfachstreuungsterm. Die feste Himmelsfüllung und der Kugelboden der Bildprobe
sind Diagnoseeingaben, kein Ersatz für klare Atmosphäre, Terrain oder Client.
Die zusätzlich optisch geöffneten PDF-Seiten 69/70/72 zeigen die beabsichtigte
räumliche Lichtwirkung; die volle dortige Beleuchtungsmethode ist nicht umgesetzt.

```sh
"$GLSLANG" -V --target-env vulkan1.0 -Isrc \
  -DCLOUD_VIEW_SAMPLES=1024 -DCLOUD_LIGHT_SAMPLES=64 \
  test/experiments/weather/cloud_picture.comp -o "$cloud_out/cloud_picture.comp.spv"
flock -n /var/tmp/outshine-heavy.lock build/cloud-probe \
  "$cloud_out/cloud_picture.comp.spv" "$cloud_out/rgb.f32" 1280 720 0.65 4
python3 -W error test/experiments/weather/cloud_radiance.py --output "$cloud_out/rgb" \
  --input "$cloud_out/rgb.f32" --width 1280 --height 720 --cover 0.65 \
  --view-samples 1024 --light-samples 64
flock -n /var/tmp/outshine-heavy.lock python3 -W error \
  test/experiments/weather/cloud_environment.py --output "$cloud_out/environment"
```

RGB-Referenzen müssen Ansicht **und** Sonnenabtastung verfeinern. Der bisherige
16-Schritt-Nachweis für optische Tiefe belegt keine ausreichende RGB-Bildqualität.
Auch konvergierte Punktstrahlen benötigen eine eigene Prüfung der räumlichen
Filterung. `CLOUD_VIEW_WARP=2` untersucht quadratische Eintrittsabtastung bei
gleicher Schrittzahl. Keine dieser Varianten ist bereits als Bildpfad abgenommen.
`cloud_environment.py` vergleicht vorbereitete Octaeder-Quellen mit identischen
GGX-/Kosinusproben ausschließlich für Transmittanz; RGB, Bodenfüllung,
GPU-Kosten und native Source-Textur bleiben getrennte offene Nachweise.

`cloud_source.comp` wertet denselben RGB/T-Kern in einer vorbereiteten Quelle oder
auf den tatsächlichen 128 GGX-/Kosinusstrahlen der 64 Filterfälle aus.
`cloud_source_comparison.py` vergleicht FP32/FP16, Winkelinterpolation, gefaltetes
RGB/T und periodische Ränder. Der direkte Source gilt nur für freie Sky-Pixel;
die Kugelgeometrie bleibt tiefenbegrenzt. Die Kameraabweichung wird ausschließlich
über Sky-Pixel ausgewertet. Die Planetenkante trennt die Interpolationsbereiche.

Der Octaeder erhält einen Vergleich mit voller Azimutauflösung und quadratischer
Zenitabbildung um den physischen Planetenhorizont. Grundlage: Hillaire,
*A Scalable and Production Ready Sky and Atmosphere Rendering Technique*,
EGSR 2020, §5.3/Abb.5, lokale PDF-Seiten 4/5 optisch geöffnet:
`doc/references/atmosphere/egsr/2020-production-ready-atmosphere.pdf`.
Die Wolkenquelle verwendet keinen Sonnen-Symmetriefold; beide Azimutrichtungen
bleiben eigenständig. Kleiner gefalteter Fehler beweist keine ausreichende
direkte Ansicht. Eine höhere Quellauflösung ist ebenfalls kein Konvergenzbeweis.

```sh
"$GLSLANG" -V --target-env vulkan1.0 -Isrc \
  -DCLOUD_VIEW_SAMPLES=1024 -DCLOUD_LIGHT_SAMPLES=64 \
  test/experiments/weather/cloud_source.comp -o "$cloud_out/source.spv"
"$GLSLANG" -V --target-env vulkan1.0 -Isrc \
  -DCLOUD_VIEW_SAMPLES=1024 -DCLOUD_LIGHT_SAMPLES=64 -DCLOUD_SOURCE_FILTER_PROBE=1 \
  test/experiments/weather/cloud_source.comp -o "$cloud_out/filter.spv"
"$GLSLANG" -V --target-env vulkan1.0 -Isrc \
  -DCLOUD_VIEW_SAMPLES=1024 -DCLOUD_LIGHT_SAMPLES=64 -DCLOUD_SOURCE_HORIZON=1 \
  test/experiments/weather/cloud_source.comp -o "$cloud_out/horizon.spv"
flock -n /var/tmp/outshine-heavy.lock build/cloud-probe \
  "$cloud_out/source.spv" "$cloud_out/source-clear.f32" 4 4 0 4
flock -n /var/tmp/outshine-heavy.lock build/cloud-probe \
  "$cloud_out/filter.spv" "$cloud_out/filter-reference.f32" 128 64 0.65 4
flock -n /var/tmp/outshine-heavy.lock build/cloud-probe \
  "$cloud_out/source.spv" "$cloud_out/source-256.f32" 258 258 0.65 4
flock -n /var/tmp/outshine-heavy.lock build/cloud-probe \
  "$cloud_out/horizon.spv" "$cloud_out/horizon-256x256.f32" 258 258 0.65 4
flock -n /var/tmp/outshine-heavy.lock python3 -W error \
  test/experiments/weather/cloud_source_comparison.py --output "$cloud_out/source-report" \
  --input "$cloud_out" --camera-reference "$cloud_out/rgb.f32" \
  --sides 256 --horizon-sizes 256x256
```

## Deklarierte Clientfälle

`cases/koerbersee-{clear,broken,overcast}.json` sind generische Szenario-Overrides
für `src/assets/places/Koerbersee.scenario`, keine Wetterbeobachtungen. Kamera,
Uhrzeit, Sichtweite, Geometrie und Renderprofil bleiben im gemeinsamen Place.
Die drei Fälle variieren nur Gesamt-/Low-Bedeckung 0/0,65/1; Low/Mid/High bleiben
Metadaten. Alle verwenden 800 m AGL sowie Wind aus 240° mit 8 m/s. Die tatsächliche
absolute Wolkenbasis folgt der vorhandenen Referenzhöhe am Weltursprung und wird
nicht auf die 2572 m des unabhängigen Dichtevergleichs festgesetzt.

Quellenerwerb und Assetaufbau erfolgen bei fehlenden Eingaben vor dem Bildvergleich
mit `build/outshine-client prepare Koerbersee 1800` (bestehendes Vorbereitungsbudget).
Das verlängert weder die 30-Sekunden-Startgrenze des Capture-Pfads noch ein warmes
Ladegate. Erst nach vollständiger Vorbereitung ist ein Offline-Vergleich sinnvoll.

```sh
python3 - <<'PY'
from pathlib import Path
import subprocess
case = 'koerbersee-broken'
overrides = Path(f'test/experiments/weather/cases/{case}.json').read_text()
subprocess.run(['build/outshine-client', 'measures', '--view', 'station',
                '--at-seconds', '0', '--quality', 'refined', '--into', 'weather',
                '--stats', '--scenario-overrides', overrides,
                'src/assets/places/Koerbersee.scenario', case], check=True)
PY
```

Erster Erwerb verwendet die normalen Client-Quellen-/Assetpfade; ein nachfolgender
frischer Prozess kann `--offline` verwenden, sobald die vollständigen Eingaben
vorliegen. Bei aktiven Wolken müssen die Metriken für bekannte Referenzhöhe und
Schicht innerhalb der Atmosphäre wahr sein. Fehlende Welt-/Wetterprodukte bleiben
unvollständig; ein gespeichertes PNG allein besteht kein Gate. Dieser einzelne
Kameracapture mit Einschwingframes ist eine Bilddiagnose, kein Lade-/Framebudget-
oder Bewegungsbeleg. Vorher/Nachher verwenden dieselben Overrides und Eingaben;
die drei verschiedenen Wetterfälle ersetzen diesen Vergleich nicht.

## Grenzen der Lieferung

Der native Kern setzt endliche gültige Schichtparameter, normierte Richtungen,
einen gemeinsamen metrischen Ost/Oben/Süd-Frame und eine gemeinsame Windphase voraus.
Die optische Tiefe ist durch 16 Abtastungen je sichtbarem Intervall begrenzt; die
Geometriedistanz schneidet die Schicht vor der Integration ab. Null Bedeckung ist
exakt transparent. Es gibt keine IO, Allokation oder implizite Weltaktualisierung.

Ansicht und kohärentes Sonnen-/Himmelslicht über die öffentliche Client-API,
WeatherSnapshot/Open-Meteo, TAA-Invalidierung, Offline-Places und vollständige
CPU-/GPU-/Speicher-/A18-Pro-Gates bleiben in #193/#7 offen. Der Dichtekern allein
erfüllt die dortige Bildabnahme nicht. Entscheidungen und aktuelle Prüfbelege
stehen im Fach-Issue beziehungsweise im verlinkten PR.
