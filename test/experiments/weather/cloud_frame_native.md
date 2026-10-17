# Nativer C++-Wolkenframe (#447)

`src/generators/weather/CloudFrame` erzeugt die mit Master, Light und Performance
vereinbarten112B. Sieben16B-Felder besitzen statisch geprüfte Größen, Alignment und
Offsets; die beiden ganzzahligen Gitterbasen bleiben `uint32_t`. Das ist ein
Produzent für die Anwendungsintegration, noch keine Umstellung aller Shader.

Wind mal Phase wird in FP64 berechnet. Breites Feld und3.1-faches Detail erhalten
je eine ganze Basis und einen kleinen FP32-Rest. Ein auf1 gerundeter Rest trägt
in die vorzeichenlose Basis über. Negative Zellen verwenden definiertes Modulo
2^32. Nichtfinite oder nicht darstellbare Eingaben scheitern über `std::expected`,
bevor `floor`/Integerkonversion unzulässig werden. Die2^52-Zellgrenze schützt die
Konversion; sie garantiert keine beliebig lange30/60Hz-Genauigkeit.

Der Feldanker wird zuerst in das tatsächliche FP32-Paket konvertiert. Genau dieser
Wert geht wieder als FP64 in die Offsetberechnung ein. Andernfalls kompensieren
Produzent und GPU bei nicht exakt darstellbaren Ankern verschiedene Koordinaten.
Physische Position, Sphärenhöhe und Licht-/Shellstrahl werden nicht verschoben.

## Tatsächlicher Verbraucher

Zuerst die normale Modulprüfung und die explizite instrumentierte Variante:

```sh
JOBS=2 nice -n 10 make suite SUITE=outshine/src/generators/weather/CloudFrame
JOBS=2 nice -n 10 make test-sanitizers SUITE=outshine/src/generators/weather/CloudFrame
```

`cloud_frame_native.py` erhält `--module-command` mit der tatsächlichen normalen
`.cmd`-Datei dieses Testprofils. Der Treiber ersetzt nur dessen Testmain durch
`cloud_frame_pack.cpp`; er linkt dasselbe einzelne Produktionsobjekt mit denselben
zentralen Compilerflags. Keine private Engine-/Generatorarchivmischung.

`--gpu-root` bezeichnet den erhaltenen nativen #456-Verbrauch samt `receipt.json`;
Host und Split-SPIR-V müssen dessen Hashes entsprechen. `--control-root` enthält
die unveränderten48B-Standardfälle samt ursprünglichem Beleg. `--reflector` ist
`spirv-cross`, `--output` ein expliziter System-Temp-Pfad. Kein Renderer-Neubau.

Der tatsächliche C++-Produzent liest13 FP64-Werte und schreibt sein112B-Objekt
direkt. Der vorhandene native Vulkanhost liest genau diese Bytes mit28 Uniform-
Wörtern. Tatsächliche SPIR-V-Reflexion prüft alle sieben Offsets, Typen und112B.
Feld-, Langzeit-, Wind-, Zellgrenz-, Anker- und Dichteschnittfälle verwenden den
unabhängigen FP64-Weltfeldbezug aus #456; Grenzen bleiben1e-4 normierter Dichtefehler
und5% relativer Windschritt-L2-Fehler. Zusätzlich werden nicht-FP32-exakte Anker
bei±32Tagen und30/60Hz geprüft. Die acht alten Standardfälle bleiben bytegleich.

`cloud_frame_cost.cpp` ersetzt analog nur den Testmain. Es misst100000 tatsächliche
Produzentaufrufe je Sample, fünf Wiederholungen, zwei Profile, zwei Anker und drei
Phasen. Prüfsummen konsumieren die Ergebnisse. Ausgabe/Prozessstart/Datei-IO und
GPU-Arbeit liegen außerhalb der gemessenen Schleife. DHAT läuft separat; dessen
instrumentierte Zeiten sind keine normalen CPU-Kosten. Arbeit ist pro Frame
konstant: drei Achsen, sechs Splits/`floor`,112B Produkt, keine weltweite Schleife.
Objektimporte und DHAT-Aufrufstapel ergänzen den Allokationsbeleg; ein leerer
Retained-Heap-Zähler allein beweist keine fehlenden temporären Allokationen.

`cloud_frame_report.py OUTPUT --costs COSTS.jsonl` erzeugt ein lokales dreiseitiges
PDF mit tatsächlichen Mess-/Dichtebildern. Das PDF auch optisch prüfen.
Es handelt sich um einen Dichteschnitt, kein Sky-/Placebild. Produktionsshader,
Medium80, gemeinsame Radiance-/Transmittanzkomposition und #328 bleiben erhalten.
Produktiver Anschluss, Bewegung/History, Upload/Lebensdauer, ganze Gerätebudgets
und Wetter-Gesamtabnahme bleiben offen. Konkrete Ergebnisse stehen im PR.

## Gemeinsamer Shader und tatsächlicher Himmel

Mit `cloud_frame_native.py --shared-layer` wird derselbe112B-Produzent gegen
`cloudExtinction` aus dem gemeinsamen `cloudLayer.glsl` geprüft; der frühere private
Split-Dichtealgorithmus entfällt. `cloud_view_native.py --wind-sky` verbindet
`MakeCloudFrame` mit dem bestehenden nativen Raster-/Himmel-Verbraucher. Dessen
privater WeatherFrame umfasst160B:112B Wolken plus unverändert48B Kamera/Licht.
Medium80 und MediumView224 bleiben erhalten. Die private `CloudFixtureInput`-
Konvertierung erhält nur kleine alte Diagnoseeingaben/SSBO-Verträge; sie ist kein
produktiver Advektionsproduzent und besitzt keine Langzeitgenauigkeitszusage.

`cloud_wind_raster_check.py FOLDER` prüft alle Pixel gegen die unabhängige FP64-
Strahl-/Quellenintegration,±32Tage,30/60Hz und Feldanker bei gleichen physischen
Strahlen. `cloud_wind_raster_cost.py --legacy OLD --current NEW --output TEMP`
misst dieselben Fälle A/B/B/A in frischen Prozessen: echte GPU-Submit-Zeit mit
Scene/Sky/Readback, beide Draws einschließlich erster Samples, keine Fence-Zeit.
Der alte Driftpfad ist eine ausdrücklich falsche Zeitgegenprobe, kein zweiter
Produktpfad. Sein Instrumentierungspatch und Originalinputs bleiben lokal erhalten.
`cloud_wind_raster_report.py TEMP` erzeugt den tatsächlichen Vorher/Nachher-PDF.
