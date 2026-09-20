Type: defect
State: active
Area: generators, render
Parent: 2111
Depends:

# Crown capture will keep leaf geometry instanced within the target memory

Erster produktiver Koerbersee-Bake: 31 Arten angefragt, erste Art ash, kein Atlas
nach mehreren Minuten. Prozessprobe: Validierung expandierter Geometrie; Physical
footprint 8.2G, Peak 15.5G, siehe 2111 und build/world-crowns-prepare-sample.txt.
Ein Artefaktgenerator, der 8 GB bereits allein überschreitet, ist nicht tragfähig.

Vor Implementierung: TreePrototype liefert zusätzlich native Rinde, ein natives
Blatt-/Verbundblatt-Mesh und die vorhandenen Blattinstanzen als Modellmatrizen.
Dieselben Card-Frames und Materialien wie GeometryAt, keine Blattreduktion und
kein gröberer Rank. Bounds aus denselben transformierten Blattpunkten bestimmen,
ohne diese Punkte sämtlich zu speichern. CrownAtlas zeichnet Rinde und instanzierte
Blätter über vorhandenen Core::RuntimeScene/PieceMesh-Pfad; pro Kamera frische Capture-
Residenz, wie bisher. Keine eigene zweite Shader- oder Capture-Geometrie.

Unreal/RAGE-Benchmark: Instanzen teilen Vertex-/Indexströme; abgeleitete Assets
werden mit begrenzter Arbeitsmenge erzeugt. Hier existieren Rank.Cards und der
bewiesene Rendererpfad bereits. Die Lücke ist ihre Verbindung vor dem Atlasbake.

Beweis: instanzierte Blattpositionen/Normalen/Materialien stimmen innerhalb
abgeleiteter float-Rundungsgrenzen mit dem bestehenden vollständig expandierten
Birken-Feinmesh überein; der Atlas bleibt relightbar und die echte World-Crown-
Coverage-Prüfung gilt weiter. Falsche Blattorientierung als Negativkontrolle.
Danach echte ash-/Koerbersee-Vorbereitung mit beobachtetem Speicher, kein Wechsel
auf leichtere Arten. PNGs und Framekosten nach warmem Laden getrennt abnehmen.

## P0: vorhandene Atlasprojektion bereinigen

Randfüllung über nächste bedeckte Texel von Geometrie-/Materialausgabe trennen.
RGBA8-Grenzen aus uint8_t ableiten, Deklarationen eindeutig halten. Unabhängiges
3x3-Artefakt prüft Farbtransfer, Alpha-Erhalt, Normalraum und MR-Kanäle ohne GPU.
Keine geänderte Darstellung und kein Arten-/Vegetationsausbau. Cache-Helfer aus
2210 borgen den Atlas statt vertauschbarer Zähler; Prädikate vollständig prüfen.

Nachweis: beide CrownAtlas-Tests grün. Das unabhängige 3x3-Artefakt prüft
Farbtransfer, unveränderte Coverage, Tangentennormalen und MR-Packing; vertauschte
MR-Kanäle scheitern an allen neun Texeln. Randfüllung und Bytequantisierung bleiben
unverändert. Vollständiges Lint: 187/187 Units, 63 Befunde; Writer bleibt rot.
