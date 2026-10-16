# Ausdrücklicher Besitzer des Sonnenatlas (#406)

Der Sonnenatlas gehört genau zu einer Lichtquelle. `Count.z` in der vorhandenen
Lighting-Uniform ist 0 ohne Zuordnung, sonst der Lichtlistenindex plus 1. Der frühere
Boolwert machte jedes gerichtete Licht zum Empfänger desselben fremden Schattens.
Material und Performance in #406 bestätigen den Übergang.

RuntimeScene hängt sein Schlüssellicht hinter `Shaped.Lamps` an und reicht dessen
Index über `SetShadowFrame`, SceneState und `ShadowedBy` bis `PackedLights` weiter.
Fehlende oder nichtgerichtete Quellen werden ohne Zuordnung gepackt. Nur die
zugeordnete gerichtete Quelle verwendet die vorhandene Sichtbarkeit vor der BRDF.
Licht-/Materialparameter, Item64/Ubo1344, Bindings, Atlas/Regionen und PCF bleiben.
Einlichtindex 0 behält seinen bisherigen Wert 1. Weitere gerichtete Quellen erhalten
hierdurch keine eigenen Schattenkarten; viele-Lichter-/Mehrschattenplanung bleibt #4.

Der native Versuch rendert den tatsächlichen `shadeRow`-Kern bei 1280×720 auf einem
kontrollierten geneigten Empfänger vor einer konstanten 8×8-Schattenkarte. Der rote
Sonnenbeitrag wird links verdeckt, rechts sichtbar; ein unabhängiges blaues Licht
muss auf beiden Seiten wirken. Opaque und DiffuseThinSheet verwenden ideale diffuse
Kontrollen ohne andere Loben. Eine analytische Referenz bestimmt Lambertantwort,
Punkt-/Spotabfall und den exakten PCF-Übergang dieser deklarierten Ebene.

Quellindex 0/1/7/15, 1/2/16 Lichter, kein Atlas, Punkt und Spot verwenden denselben
RGB-Absolutfehler von 2e-6. Einlicht-/atlasfreie/Punkt-/Spotbilder bleiben bytegleich.
Die Vorher-Uniform führt den alten Boolwert; die neue Uniform den Quellenindex.
Ein zusätzlicher alter-Producer/neuer-Shader-Gegenfall muss scheitern. Beide Teile
müssen gemeinsam integriert werden; gleiche Binärgröße ersetzt keine Semantik.

```sh
python test/experiments/render/shadow_source_native.py \
  --baseline 5939782ec --output /tmp/outshine-light-shadow-source-<commit> \
  --api-root <clean-device-archive-producer> --archive <regular-liboutshine.a> \
  --archive-receipt <producer-receipt.json>
```

Die installierbare öffentliche Engine-API wird getrennt über
`test/outshine/include/Outshine/OnlyDeclaredSunConsumesShadowAtlas.cpp` geprüft:
reale Geometrie, Würfelschatten und 1/15 authored Fülllichter vor dem Runtime-Key.
Kontrollpixel bestätigen echtes Fülllicht außerhalb und tatsächliche Verdeckung
innerhalb des Sonnenschattens. Dort muss der zusätzliche blaue Anteil innerhalb 1 %
HDR-Genauigkeit erhalten bleiben; Rot/Grün bleiben exakt. Ein optionales CLI-Präfix
speichert die drei linearen Bilder, normale Tests schreiben keine Referenzbilder.

`receipt.json` trennt Archiv-/Headerpaar, Quellen-/Capturehashes, terminale Befehle
und Prozess-/IO-Wallzeiten. Letztere sind keine GPU-/Framezeiten. Der native
Device-Versuch verlangt das passende neue Device-Archiv mit Submission-GPU-Zeit.
Er misst die Vorbereitung getrennt sowie eine erste und drei wiederholte identische
Draw-/Readback-Submissions. Dieser GPU-Scope umfasst Raster und Bildkopie, keine
Pipelineerzeugung, CPU-Arbeit oder vollständigen Frames. Fehlende Timestamps
führen zum Fehler; Fence-Wartezeit ersetzt sie nicht. Die wenigen Wiederholungen
belegen kein p99 oder Framebudget. Gleichzeitige Host-/GPU-Last wird im Beleg genannt.
Der Engine-Hosttest verwendet den jeweils vollständig passenden Bibliotheks-/
Header-/Shaderstand; seine vorhandene Renderer-Backendversion wird ausdrücklich
benannt. Der native Rendererport #328 wird nicht parallel umgesetzt.

Quellenarbeit: höchstens eine quellenbezogene Schattenabfrage pro Fragment, gegenüber
allen gerichteten Lichtquellen zuvor. Das ist eine Aussage über den Shaderquelltext;
Compiler-Hoisting und tatsächliche Texturarbeit folgen daraus nicht. Die gemessene
GPU-Submission ist ein eigener, größerer Scope.
Keine neuen Produktpuffer/-sampler/-pässe. Volle Places, Frame/p99, A18 und sämtliche
Schatten anderer Lichtquellen bleiben eigene Abnahmen.

## Gemessener Stand

Linux, RTX 3060; native Versuchsquelle 33e4718ba, gepaartes unverändertes
Device-/Archivpaar 3d362c4c8. Der reguläre Engine-/Clientbau 33e4718ba ist getrennt
vollständig aufgebaut; seine öffentliche Regressionsquelle entspricht 314a2552c.
Keine Mischung privater SceneRenderer-Header mit einem älteren Engine-Archiv.

| Beleg | Ergebnis |
|---|---|
| Tatsächlicher nativer Kern, 33 Captures 1280×720 | 58 Befehle terminal 0; 33×69 Checks ohne Fehler |
| Maximaler RGB-Absolutfehler, 16 Fälle | 0,127323963 zuvor; 5,81982e-8 nachher; Grenze 2e-6 |
| Gültige Einlicht-/atlasfreie/Punkt-/Spotkontrollen | Opaque und Thin jeweils bytegleich |
| Alter Bool-Producer mit neuem Shader | Gewollt falsch; RGB-Absolutfehler 0,127323956 |
| Öffentliche Engine, gleiche Quelle mit Capture | Vorher 64 Checks/2 Fehler; nachher 64/0 |
| Öffentlicher Einlichtfall | Vollständiges lineares Bild bytegleich |
| Zusätzlicher Blauanteil in 140 Schattenpixeln | Fehler 100 %→0,0089942 %; Grenze 1 % |
| Shadervertrag/Metal 2.3-Übersetzung | Material 384/Lighting 1344 vor/nach identisch; drei SPVs übersetzt |

Die wiederholten neuen GPU-Submissions liegen über alle kontrollierten Fälle bei
1,284832–3,892704 ms einschließlich Raster und 14.745.600 B Readbackkopie. Drei
Wiederholungen je Fall, gleichzeitige GPU-/CPU-Arbeit: keine Beschleunigungs- oder
Framebudgetbehauptung. Die 11 begleitenden NVML-Stichproben erfassen nur globale
Last/Speicherbelegung, keinen isolierten Prozesspeak. Die Produktänderung erzeugt
keine zusätzlichen GPU-Puffer, Sampler, Pässe oder Uniformbytes; zwei skalare
Hostindizes führen den Besitz. Der kontrollierte Versuch benötigt ein 14.745.600 B
Renderziel, einen ebenso großen Readback, 1.088 B Quelltexturen und 1.728 B Uniformdaten.

Dreiseitiger PDF-Vergleich optisch geprüft, lokal im Haupt-Checkout unter
`build/shots/reference/shadow-source-314a2552c/comparison.pdf`. Rohbilder, Hashes,
GPU-Zeiten und terminale Befehle stehen im zugehörigen System-Tempversuch unter
`outshine-light-shadow-source-results-5939782ec/frozen-33e4718ba/`.
