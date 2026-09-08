Type: defect
State: active
Area: generators, render
Depends: 2111

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
Blätter über vorhandenen Core::Live/PieceMesh-Pfad; pro Kamera frische Capture-
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
