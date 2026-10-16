# Native rohe Wolkenmomente

[#239](https://github.com/Weichwolf/outshine/issues/239) besitzt endliche
Extinktionstiefe. `src/generators/weather/shaders/cloudMoments.glsl` integriert
geordnete konstante Wolkenzellen: `cloudMomentCell(sum, beginM, lengthM, optical)`.
`cloudMomentValue(sum)` liefert gemeinsam M0 und M1 in Metern, ohne Division.
Der Aufrufer liefert gültige nichtnegative Zellgrößen und optische Tiefen.
Die optische Tiefe bleibt im Akkumulator; dünne Opazität wird nicht aus einem
bereits gerundeten Transmittanzwert zurückgewonnen. Das ist derselbe
Wolken-Extinktionsvertrag wie im bestehenden C++-Kern, mit T_cloud statt der
zusätzlichen klaren Luft. Ein allgemeiner gemeinsamer Medium-Historyvertrag
ist daraus nicht abgeleitet.

Für eine konstante Zelle ist die lokale mittlere Position
`1/tau - exp(-tau)/(1-exp(-tau))`. Im FP32-Kern verwenden dünne Zellen die
entsprechende Reihe bis tau^5 und die Opazitätsreihe bis tau^6; diese Wahl
verhindert Auslöschung. Die unabhängige Referenz integriert die ursprüngliche
erste Momentformel mit80 Dezimalstellen, ohne die Shader-Schwerpunktreihe.
Null, dünn, gesättigt, Teilzellen, Unterteilung und zwei getrennte Tiefen bleiben
Kontrollen. Der FP32-Zellvergleich verlangt vorab3e-6 relativen Fehler je positivem
Rohmoment und exakte0 für fehlende Extinktion. Fehler werden nicht weggeklammert.

## Tatsächlicher Verbrauch

`cloud_moments.comp` liest deklarierte konstante Zellen oder die vorhandene
CloudPrefix-/Dichteimplementierung. Physische volle Zellen entstehen vor der
Empfängerabfrage; eine Teilzelle behält ihren ursprünglichen Dichtepunkt.
Native GPU-Ausgaben enthalten auch die tatsächlich verwendeten Zellkoeffizienten.
Ein flacher Blick von4km Höhe trifft beide vollständigen Schalenintervalle;
der native Zähler muss dafür tatsächlich32 Dichteauswertungen erreichen.
Die private Probe nutzt SSBO0/0→1/0:608Byte je Query,528Byte je Antwort,
32 Zellen maximal. C++-Offsets/Alignment und SPIR-V-Strides/Mitgliedsoffsets
werden geprüft. Zwei unabhängige Aufzeichnungen müssen bytegleich sein;
Ressourcen werden bereits vor Submit freigegeben und durch Commands gehalten.
Nur die Offline-Probe wartet begrenzt; daraus entsteht kein Runtime-Warten.

Der vorhandene tatsächliche Scene/Depth-Verbrauch aus #344 besitzt zusätzlich
die private Option `CLOUD_AIR_MOMENTS=1`. Sie integriert dieselben bereits
ausgewerteten Wolkenzellen der gemeinsamen Luft-/Wolkenpartition; Dichte und
Sonnenquelle werden dafür nicht erneut abgefragt. Das bestehende Diagnose-MRT
liefert dann M0/M1 sowie Arbeitszähler. Default aus; vier bestehende FP32-MRTs,
MediumView224/Weather96 und alle Produktionsbindungen bleiben erhalten.
RGB, Luft-/Wolkenradiance, Transmittanz, Szene und Tiefe müssen bei beiden
Varianten bytegleich bleiben. Die unabhängige Float64-Feldreferenz prüft M0
und M1/getrennte endliche Wegschranke mit der vorab festgelegten dimensionslosen
1e-4-Grenze. Das ist ein Feld-/FP32-Vergleich, kein exaktes Pixelflächenintegral.
Bereits komponierter Himmel mit Tiefe0 erhält keinen weiteren Frontauftrag.
Ein dritter nativer Aufruf fragt zuerst die halbe Tiefe zweimal und danach
die volle Tiefe zweimal ab; vollständige Ausgabe und Arbeitszähler müssen mit
der direkten vollen Query bytegleich bleiben. Partielle Momente werden dazu
aus einer Kopie des bisherigen Präfixes gebildet und nicht zweimal akkumuliert.

## Reproduktion und Grenzen

NumPy/Matplotlib, LLVM mit C++23/libc++, glslang, SPIRV-Cross und ein echtes
Metal-/Vulkan-Gerät sind erforderlich; vorhandene Projekt-Toolchain verwenden.
Private Ergebnisse ausschließlich ins Systemtemp; keine `doc/`-Bilder.

```sh
python3 test/experiments/weather/cloud_moments_native.py \
  --archive-root "$ARCHIVE_ROOT" --output "$TEMP/moments"
python3 test/experiments/weather/cloud_moment_raster.py \
  --native-receipt "$CLOUD_VIEW_RECEIPT" --output "$TEMP/raster"
python3 test/experiments/weather/cloud_moment_raster.py \
  --native-receipt "$CLOUD_VIEW_RECEIPT" --output "$TEMP/raster720" \
  --width 1280 --height 720 --scenario declared-perspective
```

`ARCHIVE_ROOT` besitzt ein regulär gebautes Archiv und passende saubere Header;
der private C++-Verbraucher benutzt dessen Header. Native Backendquellen und
öffentliche Header werden gegen diesen Producer verglichen; andere Generatoren
gehören nicht zu diesem Device-Verbraucher. Die neue GLSL-Datei ändert dessen
C++-ABI nicht. Ein frischer CloudView-
Verbraucher entsteht mit `cloud_view_native.py`; ein bestehender passender
Verbraucher darf per gehashtem Originalreceipt wiederverwendet werden.
Alle Aufrufe, Originalbytes, Shader-/Archivhashes und terminalen Ergebnisse
stehen in den Ergebnisreceipts. PDF-Diagnosen ersetzen keine Place-Abnahme.

FP32 ist hier die private Messdarstellung, keine Ressourcenwahl für Produktion.
Das Native-Zweitiefenbeispiel erhält den mehr als1,7px großen Bewegungsfehler
einer einzigen M1/M0-Tiefe; gemeinsam gefilterte Rohmomente verhindern diese
Mehrschichtnäherung nicht. FP16-Grenzen aus #239 bleiben gültig.
Weder TAA/Velocity/History noch Wolken-Sky-/Thin-Komposition sind damit geliefert.
Produktiver Port und gemeinsame Ressourcen bleiben #328/#266; Frame/p99,
GPU-Zusatzzeit, vollständiger Gerätespeicher, Metal/A18 und Places bleiben offen.
Grundlage bleibt der bestätigte #239-Vertrag und die optisch geprüfte
TAA-Survey-PDF (CGF2020, Seiten3/4, §3.2); sie behauptet keine allgemeine
Wolken-Reprojektionsgarantie für eine einzelne repräsentative Tiefe.
