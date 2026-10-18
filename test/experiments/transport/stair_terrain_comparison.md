# Terrain-Gegenfälle am Wiener Treppenverbund (#434)

Die erste Geländeaufnahme ist **kein abgenommenes Terrainprodukt**. Der bisherige
18-Quellenkörper bleibt unverändert. Ganze Stützwände schließen zwar den Bodenkörper,
lassen aber Teile der langen Fußbrücke im Gelände verschwinden. Ein zusätzlicher
Aushub legt sie sichtbar frei; harte Geländeinseln und Float32-Kontaktprobleme
bleiben. Geschlossene Körper, Kernelstatus und grüne Gegenfalltests bedeuten hier
ausdrücklich keine funktionale oder visuelle Terrainfreigabe. #434 bleibt aktiv.

## Quelle und endliche Modelle

Eingang ist der gespeicherte und vorab unabhängig geprüfte
[Außenversuch](stair_outer_comparison.md): Körper-SHA256
`104f44633cfebac880108761cbb571366cf9fe2ea3b60ce07ebe3a566d428ef2`.
Alle18Quellen, Profile, Original-IDs/Properties, vier früheren Körperstände und
sämtliche Straßen-Darstellungsarrays bleiben erhalten. Kein neues räumliches
Verbraucherprodukt; `admitted_source_ids=[]`. Die vollständige Quellenbilanz des
Außenversuchs bleibt maßgeblich; dessen übrige Linien/Flächen werden nicht freigegeben.

Originale PBF14/8937/5678, SHA97489e62, und Mapterhorn-WebP67a97b8f bleiben maßgeblich.
Das versionierte231×260-DEM-Fenster0f726d83 wird mit derselben511-Post-Konvention
abgefragt. Das Versuchsgelände ist eine stückweise affine Approximation dieser
bilinearen Quelle, keine identische Darstellung aller512×512Samples.
3770Zellen mit höchstens12m Abstand,15m Außenrand,5455Vertices/10662Dreiecke;
ganze nicht überlappende Unterseiten aller gewählten Bodenwege werden ausgespart
und eingefügt. Bereich534152.907883m², kein vollständiger Place oder Stadtbau.

Galin et al., Eurographics2010, §6.2/Fig.20 auf S.9 wurde erneut optisch geprüft:
Profilkontakt und umliegende Bodenanpassung gehören zusammen. Ausgangspunkt bleibt
die SIGGRAPH2008-Trennung von Netz und Geometrie;
[Primärquellen und frühere Gegenfälle](../../../doc/references/transport/README.md).
Eigenes kleineres Versuchsverfahren, keine Implementierung des vollständigen Papers
und keine Übernahme dekompilierten Fremdcodes. Keine SDK-/Runtime-Abhängigkeit.

## Tatsächliche Gegenfälle

1. **Sanfte feste Triangulation:** echte Unterseiten und äußere DEM-Knoten bleiben
   fixiert; ganze Bodenfacetten sollen Gradienten≤2/3 haben. Eine vollständig
   fixierte Facette neben Treppe7 hat bereits Norm1.504784787. Clarabel meldet
   PrimalInfeasible; keine Oberfläche freigegeben. Das beweist nur den Konflikt
   dieser Triangulation/Kontakte, keine generelle Unmöglichkeit einer sanften Böschung.
2. **Explizite Stützwände:**18 wirkliche Nullstellen wechseln zwischen Cut und Fill.
   Ganze Kontaktflächen bleiben Double gleich; gerichtete geschlossene Hülle und
   alle11856Vertexlinks bestehen,23708Flächen, keine Nullflächen auch nach Float32.
   Trotzdem liegt das affine Gelände bis1.440344m über der Brückenunterseite.
   Der entscheidende positive XY-Überlapp hat0.379318m²: kein bloßer Randtreffer.
3. **Zusätzlicher geschützter Aushub:**1156 affine Brücken-Facettenprismen mit2cm
   seitlicher Erweiterung und10cm Unterseitenabstand werden vom Terrain abgezogen.
   Ganze ursprüngliche Boden-Unterstützung bleibt im Double-Modell geschützt.
   Manifold3.5.4, vereinfachte Bodenhülle mit50µm Toleranz; zusätzliche modellierte
   Abtragsmenge103.470135m³. Das ist weder der gesamte Cut/Fill noch ein Erdvolumen
   relativ zum vollständigen bilinearen Roh-DEM. Gemeinsame Höhen-/Material-/Statik-
   oder Abnahmeverträge werden daraus nicht abgeleitet.

Beim dritten Fall kollabieren beim bloßen Float32-Cast kleine Facetten trotz
erfolgreichem Manifoldstatus. Separater tatsächlicher Float32-Reimport baut die
Topologie neu:11322Vertices/22640Flächen, keine Nullflächen, alle Links geschlossen.
Brückenabstand im unabhängigen affinen Test mindestens−3.64e−12m, numerisch null.
Der ganze Straßen-/Terrain-Körpervergleich misst dennoch0.001417143m³ Überlappung.
Dieser Volumenvergleich nutzt denselben Kernel, keine unabhängige exakte Arithmetik.
Ein Boden-Randüberlapp hat nur5.1894e−7m², liefert aber−4.046069m signierten Abstand
zwischen den zwei verschiedenen Höhenflächen. Das ist kein vier Meter tiefer
flächiger Wegefehler; eine winzige Randfläche darf ebenso wenig als vollständiger
Kontakt durchgehen. Sie bleibt samt tatsächlicher Exporttopologie als Gegenfall erhalten.

Unabhängige Roh-DEM-Gegenprobe bei XY284.593020083/166.269811248m:
Quelle229812060:2, tatsächliche Unterseite165.377929189m, bilineares DEM166.859550910m.
Der ursprüngliche vollständige WebP-Decode und das versionierte Fenster stimmen
exakt überein; hier liegt die Quelle1.481621721m höher. Das ist ein einzelner
geografischer Zeuge, kein globales DEM-Maximum und keine Identität zu nativen Drapierpriors.

## Bild, Aufwand und Reproduktion

Geometriefreeze723a8bf3004f9dc87a25123229cb28f09d159053; die vollständige
Reproduktion49ff4e734 liefert alle sechs NPZ-Dateien bytegleich. Zehn Kontrollen
bestehen einschließlich echter Eingänge, Nullstellen/ganzer Wandkanten und des
unzulässigen naiven Exports. Gespeicherter Reader ohne Solver/Remesh/Aushub besteht
und meldet weiterhin `approved_terrain=false`.

Vier tatsächliche1280×720-Bilder geöffnet: vorher/nachher, gesamte Bodenplatte und
110m-Ostansicht. Die ersten Gesamtansichten scheiterten korrekt am unveränderten
Kameragate, weil die rechteckigen Bodenecken über die frühere Straßenkamera reichen.
Gemeinsam angepasste1138.247707m-Gesamtansichten bestehen; Pose und Inhalt bleiben
gleich. Zwei bereits erfolgreiche lokale Bilder werden über bytegleiche Meshes
und semantisch gleiche Szenen belegt wiederverwendet. Alle vier erfolgreichen
Renderprozesse terminal0, frische vollständige PNG-/Szenen-/Importbindungen geprüft.
Keine Fehlversuche als erfolgreiche Bilder gezählt; keine Galerie veröffentlicht.
Die Bilder zeigen weiterhin harte Inseln/Gräben und grobe Terrainfacetten.

Python3.13.5, NumPy2.2.4, SciPy1.18.1, Shapely2.2.0, Clarabel0.11.1,
Manifold3.5.4. Erste Freeze-Phase:3.588630s für Partition/sanften Gegenfall/Hülle,
1.838488s für Aushub,10.149632s einschließlich Audit und Körperausgaben;
207696KiB Prozess-Peak. Neue Aufnahme10.817389s/205764KiB. Beide neben eigenen
Kontrollen auf dem geteilten Host, keine isolierte oder siebenfache Laufzeitabnahme.
Explicit-Kopien zur Manifold-Eingabe1012848B; gesonderte Körper-Orakelkopien755568B.
Kernel-Allokationsanzahl/interne Kopien bleiben ungemessen. Hüllen569040B und543408B
in tatsächlichem Double-Array-Speicher; letztere Positionen sind exakt Float32.
Grenzen120CPU-s/3GiB, Solve8s, je Abstandsprüfung20s/1M-Kandidaten. Kein Frame-/GPU-/
Gerätenachweis und keine Wahl eines freigegebenen Runtimeverfahrens aus diesen Kosten.

[Aufrufe](README.md) erzeugen alles aus versionierten Quellen und einem expliziten
Außenproduktpfad. Kanonisch `/tmp/outshine-transport-stair-terrain-49ff4e734/`;
die zwei belegten lokalen Bilder unter `/tmp/outshine-transport-stair-terrain-723a8bf30/`.
`render-completeness.json`, `reproduction-identity.json`, `independent-dem-witness.json`
und gespeicherte Körper halten die Einzelbelege. Der nächste Versuch muss die ganze
Geländeform zwischen Profilen gemeinsam planen und die Exportkontakte erhalten;
weitere nachträgliche Schnitte allein gelten hier nicht als Bildgewinn.
