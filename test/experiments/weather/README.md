# Begrenzte Wolkenschicht

Die Compute-Proben verwenden den gemeinsamen nativen Device-/Commands-Vertrag aus
[#48](https://github.com/Weichwolf/outshine/issues/48) mit dem Produktionsbackend.
Dichtekern und Extinktionsmomente sind backendfrei; die Probe benötigt kein SDL.

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

Erforderlich: NumPy, Pillow, C++23-Compiler, pkg-config und ein passendes reguläres
`build/liboutshine.a` aus `make all`. Native Linkflags stammen aus dem Produktions-
Build; der vorhandene gepinnte glslang-Compiler erzeugt SPIR-V. Alle privaten
Ausgaben explizit ins System-Tempverzeichnis legen. CPU-Arbeit läuft mit nice10;
Parallelität nach tatsächlichen Ressourcen wählen. GPU-Captures laufen mit normaler Priorität.

```sh
cloud_out="${TMPDIR:-/tmp}/outshine-weather-cloud-layer-$(git rev-parse --short HEAD)"
export PYTHONDONTWRITEBYTECODE=1
mkdir -p "$cloud_out"
python3 -W error test/experiments/weather/cloud_controls.py
python3 -W error test/experiments/weather/cloud_composition.py
nice -n 10 python3 -W error \
  test/experiments/weather/cloud_comparison.py --output "$cloud_out"
nice -n 10 python3 -W error \
  test/experiments/weather/cloud_countercases.py --output "$cloud_out"
"$GLSLANG" -V --target-env vulkan1.0 -Isrc \
  test/experiments/weather/cloud_probe.comp -o "$cloud_out/cloud_probe.comp.spv"
nice -n 10 make build/cloud-native-probe
build/cloud-native-probe \
  "$cloud_out/cloud_probe.comp.spv" "$cloud_out/broken-native.f32" 240 135 0.65
nice -n 10 python3 -W error \
  test/experiments/weather/cloud_comparison.py --output "$cloud_out" \
  --gpu-result "$cloud_out/broken-native.f32"
```

Die Compute-Probe verwendet die Produktions-GLSL-Datei und den 48-Byte-Uniform mit
geprüften std140-Offsets. Sie vergleicht echte Vulkan-Float32-Ergebnisse mit Float64,
schreibt genau einen angeforderten Ergebnisbuffer und meldet Encode-/Submission-/
Fence-Beobachtung getrennt. Diese Zeiten enthalten CPU/Dispatch/Download und sind
keine GPU-Passzeit. Die optische Diagnose ist kein Place-Bild.

## Nativer Probeverbraucher

`cloud_native_probe.cpp` konsumiert den gemeinsamen Device-/Commands-Vertrag aus
[#48](https://github.com/Weichwolf/outshine/issues/48). Die Eingaben und der
128-Byte-Uniform entsprechen der bisherigen Probe. Result-SSBO set1/binding0 und
Uniform set2/binding0 werden über ihre tatsächlichen Werte gebunden; Workgroup
8×8×1 und Uniformgröße werden gegen die reflektierte Pipeline geprüft.

```sh
nice -n 10 make build/cloud-native-probe
```

Der Builder kompiliert nur die beiden fachlichen Verbraucher-Units und linkt das
reguläre Enginearchiv. Er erzeugt keine zweite Backend-/ARC-/Quellenauswahl.
`test/run.sh --native-link-flags` liefert dieselben nativen Linkflags wie der
Produktionsbuild, einschließlich Homebrews statischem SPIRV-Cross-C-Paket.
Temporäre Objekte werden nach dem Link entfernt. `CLOUD_PROBE_ARCHIVE` kann ein
explizites eingefrorenes Archiv mit denselben nativen Headerverträgen auswählen.
Linux/Vulkan ist tatsächlich ausgeführt; Master hat den stabilen Verbraucher
0d561dc66 auf A18 Pro gegen das reguläre Metal-Archiv geprüft. Der korrigierte
Builder benötigt noch dessen eigenen Darwin-Gegenlauf.
Kanalzahl1 gehört zur optischen,4 zur RGB-Probe und muss zum Shader passen.

Dispatch kopiert den Uniform sofort; Storage-Result und Readback bleiben bis zur
Completion im Besitz des Verbrauchers. Compute→Copy wird über den gemeinsamen
Commandpfad synchronisiert, MapRead erst nach begrenztem Wait auf Ready aufgerufen.
Abgebrochene Aufzeichnung wird ohne Submit verworfen. Encode, Submit und
Completion-Warten werden getrennt benannt; ohne Timestamp-API bleibt GPU-Zeit
ausdrücklich ungemessen. Die ersetzte SDL-Probe und ihr Buildpfad sind entfernt;
ihre weiter benötigten Referenzinputs bleiben bei den verlinkten Prüfbelegen.

## RGB- und Faltungsdiagnose

### Pixelfläche und Horizont

[#264](https://github.com/Weichwolf/outshine/issues/264) trennt Strahlintegration,
Pixelabtastung, Lichtquelle und Planetenkante. `cloud_footprint.py` berechnet eine
unabhängige Float64-Kohorte von 64 tatsächlichen Pixeln der 1280×720-Ansicht.
Gemittelt werden zusammengesetztes lineares RGB und Wolken-T je Teilstrahl; die
Planetenkante bleibt mit ihrem Hintergrund gekoppelt. Boxraster und zentrierte
Hammersley-Proben sind ausdrücklich endliche Quadraturen, keine allgemeine
Konvergenz- oder TAA-Abnahme. Die unveränderte Standardprobe verwendet einen Strahl.

```sh
python3 -W error test/experiments/weather/cloud_footprint_native.py \
  --output "$cloud_out/footprint-controls" --sides 1 2 4 \
  --view-samples 256 --light-samples 16
nice -n 10 python3 -W error test/experiments/weather/cloud_footprint.py \
  --output "$cloud_out/footprint-python" --sides 1 2 4 \
  --native-samples-dir "$cloud_out/footprint-controls"
python3 -W error test/experiments/weather/cloud_footprint_native.py \
  --output "$cloud_out/footprint-fine" --sides 16 32 64
python3 -W error test/experiments/weather/cloud_footprint_native.py \
  --output "$cloud_out/footprint-image" --mode image --sides 8 \
  --view-samples 512 --light-samples 32 --independent-rays
```

Die native Referenz verteilt Teilstrahlen auf unabhängige Invocations. Große
serielle Referenzschleifen sind nach einem tatsächlich beobachteten DeviceLost
gesperrt. Bilder entstehen aus 160×90 logischen Pixeln je Kachel mit unveränderter
Gesamtprojektion; unabhängige Teilstrahlen werden offline in Float64 gemittelt.
Kleine Kandidaten können stattdessen direkt im Shader mitteln. Temporäre Strahl-
ausgaben werden nach der Auflösung entfernt; fertiges Bild, Hashes, Befehle und
terminale Status bleiben erhalten. Der Renderer verwendet diese Referenz nicht.

SIGGRAPH-Grundlage: Horizon2015, PDF-Seiten83/94. Ergänzend Yang/Liu/Salvi,
*A Survey of Temporal Antialiasing Techniques*, CGF2020, Abb.2/3 und §3/3.1,
lokale PDF-Seiten2/3 optisch geprüft. Räumliche Integration ersetzt keine gültige
Wolkenbewegung/History; deren endliche Extinktionstiefe bleibt bei #239.

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
build/cloud-native-probe \
  "$cloud_out/cloud_picture.comp.spv" "$cloud_out/rgb.f32" 1280 720 0.65 4
python3 -W error test/experiments/weather/cloud_radiance.py --output "$cloud_out/rgb" \
  --input "$cloud_out/rgb.f32" --width 1280 --height 720 --cover 0.65 \
  --view-samples 1024 --light-samples 64
nice -n 10 python3 -W error \
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
build/cloud-native-probe \
  "$cloud_out/source.spv" "$cloud_out/source-clear.f32" 4 4 0 4
build/cloud-native-probe \
  "$cloud_out/filter.spv" "$cloud_out/filter-reference.f32" 128 64 0.65 4
build/cloud-native-probe \
  "$cloud_out/source.spv" "$cloud_out/source-256.f32" 258 258 0.65 4
build/cloud-native-probe \
  "$cloud_out/horizon.spv" "$cloud_out/horizon-256x256.f32" 258 258 0.65 4
nice -n 10 python3 -W error \
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

## Endliche Extinktionsmomente

[#239](https://github.com/Weichwolf/outshine/issues/239) untersucht die endliche
Wolkentiefe für Bewegungsrekonstruktion unabhängig vom Grafikbackend.
`cloud_moments.py` vergleicht Mittelpunktmomente und den analytischen Beer-Schwerpunkt
jeder konstanten Dichtezelle. Für optische Zelltiefe t und Länge l liegt dieser bei
`l * (1/t - exp(-t)/(1-exp(-t)))`; eine Reihe um t=0 vermeidet Auslöschung.
128 logarithmisch verteilte Fälle gegen 90-stellige Decimal-Rechnung prüfen dünne und gesättigte
Zellen; Teilung eines homogenen Intervalls erhält dieselben Momente.

`TraceCloudMoments` liefert M0 als `Weight` und das rohe M1 in Metern als
`WeightedDistanceM`. `VisitCloudCells` hält die Dichteabtastung identisch zum
bestehenden optischen Kern. Die analytische Integration gilt für dessen konstante
Zellen, nicht für das innerhalb einer Zelle weiter variierende Quellfeld.
Beide Momente gemeinsam filtern, erst danach dividieren; 0/0 bleibt der klare Fall.
M1 in Metern kann FP16 überlaufen. Kilometerdarstellung ist ein untersuchter
Gegenfall, noch keine vereinbarte neue Textur oder ein GPU-Packingvertrag. Bei sehr
dünnen Wolken kann FP16 selbst in Kilometern M0 auf0 quantisieren, während M1
positiv bleibt; beide Momente benötigen deshalb einen gemeinsam geprüften Bereich.

```sh
python3 -W error test/experiments/weather/cloud_moments.py --output "$cloud_out/moments"
```

Die Referenz verfeinert das echte deklarierte Feld mit 512/1024 Zellen pro
Schichtintervall. Probenzahl und Dichtearbeit bleiben im Methodenvergleich gleich.
Ein Zellschwerpunkt beseitigt weder Dichte-Unterabtastung noch die Mehrdeutigkeit
zweier Tiefen: Bei 100/10000 m, Gewichten 0,1/0,8, 960 Pixel Brennweite und
50 m/s Querbewegung bei 30 fps unterscheidet sich die Bewegung aus M1/M0 um
1,74 Pixel vom gewichteten Bewegungsmittel. Das ist ein Gegenbeispiel, keine
TAA-/Ghostingabnahme. Native Pässe, History, Ressourcen und Submission folgen dem
gemeinsamen nativen Device-/Commands-Vertrag. Yang/Liu/Salvi, *A Survey of Temporal Antialiasing
Techniques*, CGF 2020, §3.2/Gleichung 1, PDF-Seiten 3/4 als Bilder geprüft:
`doc/references/presentation/cgf/2020-temporal-antialiasing-survey.pdf`.

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
