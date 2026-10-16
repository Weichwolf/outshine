# Ausdrücklicher Besitzer des Sonnenatlas (#406)

Der Sonnenatlas gehört genau zu einer Lichtquelle. `Count.z` in der vorhandenen
Lighting-Uniform ist0 ohne Zuordnung, sonst der Lichtlistenindex plus1. Der frühere
Boolwert machte jedes gerichtete Licht zum Empfänger desselben fremden Schattens.
Material6205978425 und Performance6206048827 bestätigen den Übergang.

RuntimeScene hängt sein Schlüssellicht hinter `Shaped.Lamps` an und reicht dessen
Index über `SetShadowFrame`, SceneState und `ShadowedBy` bis `PackedLights` weiter.
Fehlende oder nichtgerichtete Quellen werden ohne Zuordnung gepackt. Nur die
zugeordnete gerichtete Quelle verwendet die vorhandene Sichtbarkeit vor der BRDF.
Licht-/Materialparameter, Item64/Ubo1344, Bindings, Atlas/Regionen und PCF bleiben.
Einlichtindex0 behält seinen bisherigen Wert1. Weitere gerichtete Quellen erhalten
hierdurch keine eigenen Schattenkarten; viele-Lichter-/Mehrschattenplanung bleibt #4.

Der native Versuch rendert den tatsächlichen `shadeRow`-Kern bei1280×720 auf einem
kontrollierten geneigten Empfänger vor einer konstanten8×8-Schattenkarte. Der rote
Sonnenbeitrag wird links verdeckt, rechts sichtbar; ein unabhängiges blaues Licht
muss auf beiden Seiten wirken. Opaque und DiffuseThinSheet verwenden ideale diffuse
Kontrollen ohne andere Loben. Eine analytische Referenz bestimmt Lambertantwort,
Punkt-/Spotabfall und den exakten PCF-Übergang dieser deklarierten Ebene.

Quellindex0/1/7/15,1/2/16Lichter, kein Atlas, Punkt und Spot verwenden denselben
RGB-Absolutfehler von2e-6. Einlicht-/atlasfreie/Punkt-/Spotbilder bleiben bytegleich.
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
reale Geometrie, Würfelschatten und1/15authored Fülllichter vor dem Runtime-Key.
Kontrollpixel bestätigen echtes Fülllicht außerhalb und tatsächliche Verdeckung
innerhalb des Sonnenschattens. Dort muss der zusätzliche blaue Anteil innerhalb1%
HDR-Genauigkeit erhalten bleiben; Rot/Grün bleiben exakt. Ein optionales CLI-Präfix
speichert die drei linearen Bilder, normale Tests schreiben keine Referenzbilder.

`receipt.json` trennt Archiv-/Headerpaar, Quellen-/Capturehashes, terminale Befehle
und Prozess-/IO-Wallzeiten. Letztere sind keine GPU-/Framezeiten. Der native
Device-Versuch liefert keine Raster-/Submission-Timestamps auf dem gepaarten Host.
Der Engine-Hosttest verwendet den jeweils vollständig passenden Bibliotheks-/
Header-/Shaderstand; seine vorhandene Renderer-Backendversion wird ausdrücklich
benannt. Der native Rendererport #328 wird nicht parallel umgesetzt.

Quellenarbeit: höchstens eine quellenbezogene Schattenabfrage pro Fragment, gegenüber
allen gerichteten Lichtquellen zuvor. Das ist eine Aussage über den Shaderquelltext;
Compiler-Hoisting/ tatsächliche Texturarbeit und GPU-Dauer bleiben getrennt zu messen.
Keine neuen Produktpuffer/-sampler/-pässe. Volle Places, Frame/p99, A18 und sämtliche
Schatten anderer Lichtquellen bleiben eigene Abnahmen.
