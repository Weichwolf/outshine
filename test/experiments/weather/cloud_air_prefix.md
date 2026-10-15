# Begrenzter gemeinsamer Wolken-/Luftpräfix

[#193](https://github.com/Weichwolf/outshine/issues/193) besitzt den Wolkenverbraucher;
[#266](https://github.com/Weichwolf/outshine/issues/266) die gemeinsame Komposition.
Light hat Einstieg, Einheiten und Quellenbesitz ausdrücklich bestätigt:
[Fachantwort](https://github.com/Weichwolf/outshine/issues/266#issuecomment-6194916672).
Der wiederverwendbare klare Kern ist separat mit
[#233](https://github.com/Weichwolf/outshine/pull/233) nach master gemergt.
Der Produktionsarchivbeleg verwendet den eingefrorenen Stand9290881d0. Der ältere
bb1addc45-Kern aus #259 wurde im ersten Teilreplay verwendet; die fertige Lieferung
konsumiert den integrierten Kern und dessen versionierte Replaydaten.

## Vertrag

`cloudAirPrefixBegin(layer, originM, direction, physicalExtentM, light,
clearPrefix, clearBeginM)` übernimmt einen frisch begonnenen klaren Präfix.
`cloudAirPrefixAdvance(inout prefix, queryM, medium, veil, multiScatter)` liefert
das vorhandene `AirTransfer{RadiancePerLux, Transmittance}`. Alle übergebenen
Positionen und Strecken sind Meter; der klare Kern arbeitet in Kilometern.
Richtung, Radien, Ursprung und Atmosphäreneintritt beschreiben denselben physischen
Planetenstrahl. Gültige endliche Parameter und monotone Abfragen zwischen0 und
physicalExtentM sind Voraussetzung. Vor dem Atmosphäreneintritt gilt L=0/T=1;
Wolken bleiben an ihrer tatsächlichen räumlichen Strecke begrenzt.

## Modulgrenze und tatsächliche Verbraucher

Die Wolkendichte, das deklarierte Quellrezept und ihr begrenzter Zelleniterator
liegen unter `src/generators/weather/shaders/`: `cloudLayer.glsl`,
`cloudRadiance.glsl` und `cloudPrefix.glsl`. Diese Dateien importieren keinen
Renderer. Das Quellrezept ist weiterhin der deklarierte Versuch; seine vollständige
Licht-/Schattenkomposition gehört Light unter #266.

`src/render/shaders/mediumCell.glsl` integriert ein Teilstück des vorhandenen
klaren Bernstein-Präfixes mit zusätzlichen konstanten Extinktions-/Emissionswerten.
Der Aufrufer liefert diese Koeffizienten; der Renderer kennt weder Wolkenparameter
noch ihre Dichtefunktion, Windphase, Schichtgeometrie oder Iteration.

Der gemischte `CloudAirPrefix`-Adapter liegt als `cloud_air_prefix.glsl` beim
vorhandenen Experiment und wird ausschließlich von `cloud_air_prefix.comp`
aufgerufen. Begin/Advance, Typen, Einheiten und feste Partition bleiben erhalten.
Dieser tatsächliche native Nachweis ist noch keine öffentliche Shader-/Generator-
API oder Engine-/Opaque-Anbindung. Diese Integration bleibt #193/#266/#9.
Für die Assembly sind die Include-Verzeichnisse `src/render/shaders/`,
`src/generators/weather/shaders/` und `test/experiments/weather/` explizit.

Der unveränderte Architekturtest `TheRendererNamesNoSubject` bleibt maßgeblich.
Der Strukturvergleich der zwölf Shadervarianten lässt ausschließlich `OpName`
und `OpMemberName` aus: Diese beiden Einträge sind
[SPIR-V-Debuganweisungen](https://github.com/KhronosGroup/SPIRV-Headers/blob/main/include/spirv/unified1/spirv.core.grammar.json).
Header, IDs, Operationen, Konstanten, Dekorationen, Layouts und Bindings werden
vollständig bytegleich verglichen. Dies ist kein Laufzeit-/Gerätenachweis;
die nativen Referenzkontrollen bleiben separat erforderlich.

Die physische Extent und beide Quellenpartitionen bleiben während aller Abfragen
fest. Zwei sichtbare Wolkenintervalle besitzen je16 Dichtezellen. Eine Tiefenabfrage
innerhalb einer Zelle integriert deren unveränderte Koeffizienten nur teilweise.
Dichte und Wolkenquelle werden erst beim Erreichen einer Zelle und dort einmal
ausgewertet. M1/M0 wird für diese Teilstrecken nicht benutzt.

Der Präfix vereinigt klare Zellgrenzen, Wolkenzellgrenzen und Vakuumgrenzen.
Positive quadratische Bernstein-Koeffizienten werden per de Casteljau auf das
Teilstück eingeschränkt. Wolkenextinktion und -emission werden lokal addiert;
Lights vorhandener `airIntegrateCell` integriert die gemeinsame Zelle. Keine
Division durch bereits integriertes Ta/Tc, kein eigener Luftsolver, keine neuen
Ressourcen oder Shaderbindungen. Bei Cover0 wird direkt der klare Kern benutzt.

Die Augenintegration konsumiert die vorgegebenen Quellkoeffizienten. Kohärente
Wolkenschatten auf den Sonnenweg im klaren Quellterm und die gemeinsame
Mehrfachstreuung/Psi benötigen noch ihre eigene Licht-/Wetterintegration. Der
Replay setzt die Luftbeleuchtung explizit fest und belegt diese Schatten nicht.
Sonnen-Lux wird erst beim Verbraucher einmal auf RadiancePerLux angewendet.

## Verfahrensvergleich

`cloud_mixed_cells.py` untersucht8450 homogene Quellenbasisfälle sowie die lokalen
Zelltiefen von384 echten deklarierten Wolkenrays bei Haze0/1/4. Es vergleicht
denselben exakten endlichen Luftpräfix und dieselbe konstante Wolkenquelle je Zelle.

| Verfahren | Klarpräfixabfragen/Wolkenzelle | Maximaler Fehler auf deklarierten Zelltiefen |
|---|---:|---:|
| analytischer Beer-Schwerpunkt |2|0,0009562|
|2 CDF-Gaussknoten |3|0,0027984|
|4 CDF-Gaussknoten |5|0,0006820|
|8 CDF-Gaussknoten |9|0,0001591|

Endpunktrekonstruktion ist homogen bis2,23e-16 genau, verliert aber bei einer
Quellenänderung innerhalb derselben Strecke0,063402 Radiance. Die Zellvereinigung
erhält solche bekannten Quellengrenzen. Ihre Genauigkeit im variierenden Feld
bleibt an die gemeinsame Koeffizientenapproximation gebunden.

Bei16 Wolkenzellen je Intervall ist die Treiberschleife auf68 Schritte begrenzt.
Höchstens32 Ansichtsdichten und32 Wolkenquellen werden benötigt; eine Wolkenquelle
besucht höchstens12 Schatten-Dichtepunkte. Zusammen sind das höchstens416
Wolkendichteauswertungen je Ray. Klare Quellen werden auf ihrer vorhandenen
Partition wiederverwendet. Diese Arbeitsgrenzen sind kein GPU-Zeit-/Speicherbudget.

## Reproduktion

Erforderlich: NumPy, gepinnter glslang, Lights vorhandener `air_prefix_gpu.cpp`
gegen das reguläre Produktionsarchiv und die festen RGBA16F-Replaydaten aus #233.
Der Verbraucher benutzt den gemeinsamen nativen Device-Vertrag mit Validation.
`AIR_ROOT` benennt den zu den Quellen passenden Checkout, `AIR_PREFIX_PROBE` den
bereits gebauten Verbraucher. Vulkan-/Metal-Auswahl erfolgt im regulären Backend.

```sh
prefix_out="${TMPDIR:-/tmp}/outshine-weather-cloud-prefix-$(git rev-parse --short HEAD)"
nice -n 10 python3 -W error test/experiments/weather/cloud_mixed_cells.py --output "$prefix_out/methods.json"
python3 -W error test/experiments/weather/cloud_air_native.py \
  --air-root "$AIR_ROOT" --probe "$AIR_PREFIX_PROBE" --output "$prefix_out/warp1"
python3 -W error test/experiments/weather/cloud_air_native.py \
  --air-root "$AIR_ROOT" --probe "$AIR_PREFIX_PROBE" --output "$prefix_out/warp2" --warp 2
```

Der vollständige Lauf enthält eine110-km-Kamera. Der Verbraucher akzeptiert dafür
endliche Radien ab dem Planetenboden; das Atmosphärendach begrenzt die Integration
im vorhandenen `airPathSpan/airPathRay`. Die frühere CPU-Zurückweisung oberhalb des
Dachs wird korrigiert. NaN, Unendlich und Kameras unter dem Planeten bleiben
nachweislich ungültig. `--subset ground` und `--subset orbit` dienen Teilreplays.

Jeder Lauf prüft Cover0/0,65/1, zwei sichtbare Intervalle, RGB-T, unabhängige lokale
RTE, tatsächliche Arbeitszähler und bitgleiche wiederholte Abfragen. Neue und
fortgeschriebene Präfixe werden zusätzlich als getrennte Shader-Spezialisierungen
verglichen. Deren unterschiedliche zulässige FP32-Optimierungen verlangen keine
Bitgleichheit; beide müssen die unveränderten numerischen Referenzgrenzen und
dieselben Zähler erfüllen. Auch der separate wolkenfreie Kontrollshader erfüllt
die unveränderten klaren Grenzen1e-5 Radiance/2e-6 RGB-T. Der Boden-Teilreplay war
dort bytegleich, die Orbitkonvertierung unterschied sich um höchstens5,97e-8.
Die anfängliche Annahme einer Bitgleichheit zwischen verschiedenen Shader-
Spezialisierungen ist damit ausdrücklich korrigiert. Bitgleiche Wiederholungen
innerhalb jedes Kernels bleiben Pflicht. Kein Szenen-, Raster-, Bewegungs- oder
Place-Nachweis daraus.
Ein zusätzlicher Lauf desselben Shaders prüft bitgleich, dass die letzte Abfrage
mit und ohne vorausgehende Zwischenabfragen denselben Transfer und dieselben
Arbeitszähler liefert.

Der Lauf prüft zusätzlich20868 vorhandene dichte Klarluftreferenzen mit variierenden
Koeffizienten und echter gemeinsamer Zellteilung bei Wolkenextinktion0. Lights
unveränderte1%-Radiance-/0,002-RGB-T-Schranken bleiben dafür maßgeblich.

`cloud_air_picture.py` rendert den tatsächlichen nativen Transfer über die ganze
physische Strecke bis Planet oder Atmosphärendach. Die feste Lookupquelle und der
deklarierte Wolkenquellterm bleiben Eingaben. Ein gleichfarbiger Kugelplanet ist
der Platzhalter für opaken Hintergrund; Terrain, Assets und transparente Raster-
Oberflächen sind in dieser Diagnose nicht enthalten. Private Ausgabe immer explizit:

```sh
python3 -W error test/experiments/weather/cloud_air_picture.py \
  --shader "$prefix_out/warp1/broken/probe.spv" --probe "$AIR_PREFIX_PROBE" \
  --fixtures "$AIR_ROOT/test/experiments/render/data/air-prefix/dense" \
  --output "$prefix_out/picture-broken"
```

Die Aufnahme hat1280×720 Pixel. Die weiterhin sichtbaren Horizontstreifen bleiben
ein Bilddefizit unter #264; der mathematisch korrekte Transfer ist keine Abnahme
der begrenzten Dichte-/Schattenabtastung oder des gesamten Wetterbilds.

Die [Blickzellen-/Quellengegenfälle](cloud_sampling.md) isolieren diese Fehler
mit verfeinerter Float64-Kohorte und nativen Vollbildern. Sie verwerfen einen
einzelnen neuen Quellpunkt und eine scheinbar gleich teure Dreipunkt-Lichtregel;
gemeinsame Produktionsdefaults werden dadurch nicht geändert.

Grundlagen: Schneider/Vos, SIGGRAPH2015, Wolkenkurs PDF-Seite54 (Beer), sowie
Hillaire, EGSR2020, PDF-Seiten2/3 (Strahlgeometrie, Gleichungen1–4 und gemeinsame
Extinktion). Beide PDFs liegen unter `doc/references/`; die genannten Seiten wurden
optisch geprüft. Die Zell-/Ereignisvergleiche sind eigene Herleitungen und Belege.
