Type: feature
State: active
Area: generators
Tags: webcam, measured
Depends: 2123

# Forests and urban trees populate suitable ground

## Aktueller Beleg

Die neun Render zeigen praktisch keine lesbare Baumvegetation, besonders auffällig in
Koerbersee, Wien, Olympiaturm und Feldkirch. Frühere Logs meldeten `flora placed 0`;
die damals vermutete Cover-/Region-Frame-Differenz ist keine neu bewiesene Ursache.

## Diagnose auf dev/codex, 2026-09-07

`make shots PLACE='--measures Koerbersee'`, `build/forest-diagnosis.log`, Exit 0.
Asking veröffentlicht jetzt sämtliche vorhandenen Yield-Notes und Full-Claims.
Koerbersee-53208246.png geöffnet: weiterhin keine lesbaren Baumkronen; p99 7,26 ms,
0/120 Standframes über 16,67 ms. Digestabweichung trotz rein diagnostischer Änderung
unterstreicht 2154; sie ist kein visueller Fortschritt.

| gemessen | Wert |
|---|---:|
| Gebäude platziert | 11 |
| Flora platziert | 4085 |
| Region gesamt | 11 + 4085 = 4096 |
| Flora Full-Claims | 1 |
| erzeugte Draw-Instanzen, alle Generatoren | 4096 |
| Flora noTemplate / zeroDensity / densityDraw | 516 / 29461 / 127462 |
| Flora noSpecies / aboveTreeline | 0 / 0 |

Damit ist die frühere pauschale Null-Platzierungsdiagnose für diesen aktuellen Ort widerlegt.
Quellpfad geprüft: `Asking.cpp` schreibt `World.Instances`; eine Suche über `src/` findet
keinen Übergabe-/Renderer-Leser dieses Vektors. `World.Instanced` zählt lediglich dessen Größe.
`Shipping::Stands` liest Arten, erzeugt aber nur Stem-Höhen und `ForestDraw(ClusterId{0},
stems.front().HeightM)`; kein TreePrototype-Mesh wird dort gebaut/registriert. Ein blindes
Weiterreichen dieser Cluster-ID wäre falsch. Species-Identität muss bis zur Instanz erhalten
bleiben. `Grows` bearbeitet nur den Tile am Auge und kehrt nach World.Placed > 0 zurück:
Ringweite Vegetation und Wiedereintritt fehlen unabhängig von der ersten Sichtbarkeitsreparatur.

Nächster Umsetzungsschritt: vorhandene TreePrototype-Ausgabe in echte registrierte
Geometrie/Materialien überführen, stabiler Prototypbezug pro platzierter Art, Instanzen im
korrekten Regionsframe an den Renderer. Danach begrenzte Tile-Jobs/LOD statt Regionskapazität
blind erhöhen. 2123/2124 bleiben Voraussetzungen der vollständigen Abnahme.

## Implementierung

1. `src/generators/flora/Forest.cpp`, Asking/Yield und ForestDraw: Kandidaten, Ablehnungsgründe,
   platzierte Instanzen, hochgeladene Instanzen, sichtbare Draws getrennt zählen. `noTemplate`,
   Dichte, Neigung, Treeline, Kapazität und Frame-Projektion bis zur GPU verfolgen.
2. Ursache dort reparieren, wo Zahl erstmals falsch wird; Unit-/Region-Frame nicht neu
   erraten. Positive Placement-Fixture und falsches Frame als Negativkontrolle.
3. OSM forest/wood/tree/tree_row/park plus Höhe/Neigung/Feuchte in plausible Bestände
   übersetzen; Artenmischung/Kronenform/Dichte aus deklarierten regionalen Verteilungen.
   Gebäude/Wege/Wasser aussparen, Waldkante unregelmäßig, keine Kopie einzelner Fotobäume.
4. Weltkoordinaten-Seed, geteilte Prototypen/Instancing, Nahgeometrie/Fernkronen mit LOD;
   Alpha-Cutout, Blatttransmission, Normalen und Shadows mit 2171/2128 integrieren.
   Wind und saisonale Änderung aus 2172 später konsistent einspeisen.

- [ ] Positive Counts bis zum Draw und sichtbare Baumkronen an den vier Referenzen.
- [ ] Platzierungsregeln über Tilegrenzen, Rückkehr und anderer Blickrichtung stabil;
      offenes Wasser/Straße bleiben frei. Keine stillen Abbrüche bei voller Kapazität.
- [ ] Overdraw, Schatten, Instanzen/Bytes und Frame-p99 nach 2092 mit dichter Vegetation.

Wahl: prozedurale Foliage/Instancing wie öffentliche Unreal-Konzepte; RAGE ist visueller
Dichte-/Distanzbenchmark. Die aktuelle leere Welt wird nicht durch manuell gesetzte Bäume repariert.

## Erhalt der Artidentität, dev/codex

`Solid::Variant` erhält den vom Forest gewählten Artenindex. Das Feld nutzt die bisherigen
vier Paddingbytes: sizeof(Solid) bleibt 48 Bytes. ForestDraw hält Prototyp-ID/Höhe pro Art
und skaliert relativ zu dieser Höhe. Shipping vergibt getrennte IDs für Arten und Gebäude;
das sind weiterhin Katalogreferenzen, noch keine registrierten GPU-Geometrien.

`ForestInstancesKeepTheirSpecies` prüft zwei Arten mit unterschiedlichen Prototyphöhen,
einen BodyRange mit Offset sowie Positions-/Höhenbeibehaltung. Echter Mutationslauf mit
`Prototypes_[0]` statt `Prototypes_[body.Variant]`: Art-ID und Maßstab rot, drei Checkfehler.
Wiederhergestellt: `make suite SUITE=outshine/conventions`, 10/10 PASS, Exit 0
(`build/forest-species-restored.log`; Mutation `build/forest-species-negative.log`).
Die neue Fixture benötigte vorab einen korrigierten Rasteraufbau; kein Produktoracle gelockert.

`make shots PLACE='--measures Koerbersee'`, Exit 0, `build/forest-species-place.log`:
54fe2b37, p99 7,82 ms, 0/120 Standframes über Budget; 4085 Flora und 4096 Instanzen.
PNG geöffnet: weiterhin keine lesbaren Baumkronen, keine beanspruchte Bildverbesserung.
Nächster Schritt bleibt Prototypgeometrie + MR-Materialien + echte instanzierte Übergabe,
danach räumlich vollständige begrenzte Tile-Platzierung. WI bleibt active/offen.

## Native Prototypgeometrie / neues Wachstumsfinding

TreePrototype::GeometryAt(rank) übergibt vorhandene Stammgeometrie und die erzeugte
Blattmorphologie als native Geometry in Metern, mit getrennten MR-Materialien. Der bisherige
20-Float-Tree-Parameterblock wird dafür ausdrücklich nicht als Materialzeile verwendet.
Dies ist die mesherseitige Übergabe, noch keine ringweite instanzierte Renderer-Anbindung.
Der erste echte Prototyprender fand den Wachstumsfehler 2177; dessen Reparatur lässt eine
Krone entstehen. Aktueller nativer Render: build/tree-native/birch.png, visuell geprüft.

Nächste notwendige Arbeit: Screen-error-LOD für Äste und räumlich gleichmäßige Blatt-/Kronen-
Coverage. Rank 3 hat 272934 + 80640 = 353574 Dreiecke pro Birke; für Wald nicht tragbar.
Die Expansion der Blattfächer in Geometry ist ein funktionsfähiger nativer Mesher, aber kein
Ersatz für geteilte Prototyp-/Blattdaten und budgetierte Instanzen. Diese großen Prototypen
nicht ungeprüft pro Baum in den Piece-Pool kopieren. Regionsframe/Tile-Streaming weiter offen.

## Aktueller Prototyp-LOD

2123 erhält jetzt alle Blattansätze und das deklarierte Maß statt 80-cm-Ersatzblättern.
Native Birke Rank 3: 2184 Rinden- plus 915500 Blattdreiecke. Die Blattkontur reduziert
112 auf 20 Dreiecke je Blatt unter geometrischem Fehlerbudget und zusätzlicher 2-%-
Flächenschranke (gemessener Verlust 0,18 %). Das ist gegenüber der
feinen Referenz billiger, gegenüber dem alten 716-Riesenblatt-Proxy teurer. Kleine Blätter
sind sichtbar, Kronendeckung/Licht bleiben unzureichend. Messung, Bilder und echte
Negativkontrollen in 2123. Keine Wald-Framezeit daraus ableiten.
Renderer-Handoff, gemeinsame Prototyp-Instanzen und ringweite Platzierung bleiben offen;
auch den aktuellen Prototyp nicht pro Weltinstanz in den Piece-Pool kopieren.

## Filtervoraussetzung für Kronenbilder

2171 übergibt native Bilder und baut nun flächenintegrierte Mips einschließlich ungerader
Ränder. GPU-Minifikation und residente Unlit-Schachbrettwiederholung sind nachgewiesen.
Alpha-Coverage/Bewegung/Normalvarianz bleiben offen; Corpus-Integralvergleich in 2179.
Koerbersee nach Mip-Schritt weiter c99cdbe7, keine lesbaren Kronen. Nächster bildwirksamer
Waldschritt bleibt eine budgetierte gemeinsame Kronenrepräsentation samt tatsächlichem
Instanz-Renderpfad; die hohe nahe Einzelbaumgeometrie nicht pro Weltbaum duplizieren.

## Gemeinsame Mesh-Platzierungen als nächste Renderer-Voraussetzung

SubjectDraw beherrscht Instanced Draws und liest gl_InstanceIndex aus der Matrix-Tabelle.
PieceMesh hält jedoch nur eine Row und PlacePiece kopiert für jeden Aufruf die Vertices/
Indices erneut. Unreal/RAGE teilen Prototypgeometrie und halten Platzierungen separat;
dieses bestehende Muster erweitern, keinen zweiten Wald-Renderer bauen.

PieceMesh erhält optionale Platzierungszeilen. Ohne Zeilen bleibt die vorhandene einzelne
Row gültig. Ein unclusterter Prototyp wird mit einer Instanced-Draw-Zeile gezeichnet;
bei Clustern müssen Jobs/Batches je Instanz deren eigene Matrix auswählen. Geometrie wird
in beiden Fällen einmal resident gehalten. Release muss alle Zeilen freigeben und die
nachfolgenden Matrixindizes korrekt nachführen.

GPU-Probe: ein Dreieckprototyp an mehreren getrennten Positionen, Pixelbelegung und
residenter Geometrieumfang prüfen; Release/Wiederbelegung darf keine alten Instanzen zeigen.
Echte Mutation: Anzahl Instanzen auf eins reduzieren → fehlende Pixel rot. Clusterpfad
separat prüfen, insbesondere erste Instanz außerhalb des Sichtfelds. Dieser Schritt allein
ist noch keine Kronengeometrie oder ringweite Waldplatzierung.

Abnahme der Übergabe: PieceInstancesShareTheirGeometry besteht im unclusterierten
und geclusterten Pfad einschließlich Freigabe, Slotwiederverwendung und Erhalt
der folgenden Einzelplatzierung. Die geöffneten positiven PNGs zeigen drei
getrennte Dreiecke. `build/shared-piece-single-placement-negative.log` begrenzt
temporär die übernommenen Zeilen auf die erste (außerhalb des Frustums): genau
die beiden Sichtbarkeitsprüfungen werden rot, 34 Checks/2 Fehler. Mutation
zurückgenommen. Der dabei getrennt geprüfte Schachfehler existiert bereits im
Renderer vor der Instanzänderung; Nachweis und Restbefund in 2179.

Der Weltanschluss benötigt weiterhin den Regionsframe: Scattered hält bisher
tilelokale Em/Nm als Float, Standing bewahrt keinen Tile-Frame. Nicht als globale
Koordinaten interpretieren; vor dem Renderhandoff Weltpositionen in Double
rekonstruieren und erst an der Kameragrenze verengen. Clusterjobs und kompaktierte
Indices wachsen trotz geteilter Quellgeometrie je Instanz; deren Budget bleibt
zu messen. Piece-Batches sind noch pauschal Opaque, daher ist Masked-Kronenmaterial
mit der Materialarbeit in 2171 zu verbinden.

2180 ist im selben Pfad repariert: Piece.IndexCount hält die tatsächlich
geschriebenen Indices, die Allocation-Range bleibt für die Freigabe erhalten.
Damit zeichnet ein Drei-Index-Mesh nicht die 4096er-Reserve und zieht beim
Freigeben nicht 1365 statt eines Dreiecks vom Zähler ab. Die ursprüngliche
Mutation lieferte den dokumentierten uint32-Unterlauf; der reparierte Pfad
behält Nachbargeometrie und kehrt nach beiden Freigaben auf null zurück.

`make shots` / `build/shared-piece-places.log`, Exit 0: alle neun PNGs geöffnet.
Digest / p99 ms / Peak-Heap MB für je 120 residente Frames:

| Place | Digest | p99 ms | Heap MB |
|---|---|---:|---:|
| DarmstadtWest | e72d1925 | 2.87 | 355 |
| Wien | 8ff2d96d | 5.73 | 473 |
| Rosenheim | 7da2e093 | 4.11 | 385 |
| Husum | d60b18a7 | 3.04 | 232 |
| Olympiaturm | 07985050 | 3.88 | 622 |
| Graz | f93ff5b9 | 5.03 | 568 |
| Koerbersee | c99cdbe7 | 7.19 | 507 |
| Malcesine | 46e4db5c | 4.87 | 387 |
| Feldkirch | 5fa234c1 | 5.02 | 427 |

Jeweils 0/120 über 16.67 ms; kein Nachweis für Bewegung oder fertige Weltlast.
Acht Digests entsprechen der vorigen Gesamtprüfung. Feldkirch entspricht der
bereits zuvor ohne Sourceänderung beobachteten Variante: gegenüber 63dcc99c
3126 andere Pixel, Box [0,1277)×[205,720). Ursache bleibt in 2154 offen.
Koerbersee/Malcesine/Feldkirch zusätzlich mit geöffneten Webcam-Referenzen
verglichen: Wälder fehlen, Fels bleibt glatt/gefaltet, Wasser flach und dunkel,
Gebäude ohne ausreichende Material-/Formvielfalt. Keine visuelle Verbesserung
behauptet; die nächste Arbeit muss diese Voraussetzung tatsächlich erreichen.

## Geografischer Instanz-Handoff als nächster Schritt

ForestDraw/BuildingDraw casten Solid.Em/Nm/BaseAslM derzeit auf Float; Instancing
legt anschließend lokale Werte ohne deren Region ab. Unreal hält Welttransforms,
RAGE trennt Streamingregion und Instanztransform; hier die bestehende Tile.Geo-
Umrechnung am Engine-Handoff nutzen. Generatoren behalten ihre lokalen Double-
Positionen, die Engine hält Longitude/Latitude und ausdrücklich ASL-Höhe in Double
plus Yaw/Scale. Kein zusätzlicher ECEF-/Ellipsoid-Datumwechsel in diesem Schritt
und keine volle Tile-Kopie je Instanz. WorldPlacement bleibt engineintern.

Nachweis vor Anbindung: sub-Float-Abstände bleiben beim Draw-Sink exakt erhalten;
zwei Regionen mit unterschiedlichen lokalen Koordinaten ergeben dieselbe
geografische Position. Die bisherige Float-Verengung muss die Positionsprüfung
rot machen. Diese Übergabe rendert selbst noch keinen Wald; sie beseitigt die
fehlende Ortsreferenz für dessen anschließenden gemeinsamen Mesh-Handoff.

Umgesetzt: Scattered hält drei Double-Positionswerte; ForestDraw und BuildingDraw
verengen sie nicht mehr. Der Engine-Sink erhält die erzeugende Region und speichert
WorldPlacement mit Longitude/Latitude, ASL-Höhe, Yaw und Scale. Tile.Geo ist die
bestehende Ortsumrechnung; ASL wird dabei nicht als Ellipsoid-Höhe umetikettiert.

`build/world-placement-precision-negative.log`: alter Renderer-Handoff verliert
die drei Source-Positionen in der verschärften Double-Prüfung, drei gezielte
Fehler; Suite Exit 2 (zusätzlich bekannter Schachfehler).
`build/world-placement-geographic-proof.log`: Exit 0, 18/18 PASS. Die erweiterte
Positionsfixture hat 26 bestandene Checks: beide Generatorpfade bewahren ihre
Double-Werte, verschiedene Tile-Frames treffen dieselben geografischen
Koordinaten innerhalb 1e-12 Grad; ASL/Yaw/Scale bleiben exakt. Die kleine Differenz
2^-30 m prüft Datenverlust, ausdrücklich keine DEM-Messgenauigkeit.
Die erneut erzeugten Instanz-PNGs geöffnet: weiterhin drei getrennte Dreiecke.
World.Instances hat weiterhin keinen Renderconsumer, daher noch keine bildwirksame
Places-Änderung. Nächster Schritt bleibt gemeinsame Kronengeometrie samt Render-
und Materialbindung; die geografischen Platzierungen dafür sind jetzt erhalten.

Korrektur des früheren Masked-Verdachts: SubjectDraw::Encode wählt Pipeline und
Culling über SurfaceSlot.Kind/CullsBack. Das pauschale DrawBatch.Kind bestimmt
diesen Draw nicht; daraus folgt kein belegter Masked-Fehler. Vor Änderungen
native ausgeschnittene Karten mit zwei Instanzen und gemeinsamer Geometrie
prüfen, jeweils mit/ohne Cluster und von beiden Seiten. Die vorhandenen
Khronos-Alpha-/DoubleSided-Deklarationen sollen greifen; Unreal/RAGE nutzen
dasselbe Grundprinzip für Kronenkarten. Tiefenpixel in der transparenten Hälfte
müssen frei bleiben. Eine temporär erzwungene Opaque-Pipeline muss diese
Prüfung rot machen. Noch keine Abnahme von gefilterter Kronen-Coverage oder Wind.

Nachgewiesen: `build/masked-piece-proof.log`, PieceInstancesShareTheirGeometry
66/66 Checks bestanden. Zwei geteilte Karten mit nativer 2×1-Alpha-Textur,
MASKED und DoubleSided, direkt/geclustert und Vorder-/Rückseite. Alle vier PNGs
geöffnet: nur die deckende Hälfte sichtbar. Positive Bilder gesichert unter
build/instance-native/masked-positive/. Suite 17/18 PASS, allein bekannter Schachfehler.

`build/masked-piece-opaque-negative.log`: nur die Pipelinewahl für Masked temporär
auf Opaque gesetzt. Genau acht Masken-Tiefenchecks werden rot (66 Checks/8 Fehler);
Suite Exit 2, 17/18 PASS. Negative PNG geöffnet: beide Karten sind volle Rechtecke.
Mutation zurückgenommen; Produktcode bleibt unverändert. Das pauschale Batch.Kind
war hier kein Funktionsblocker. Die Fixture verwendet Nearest/MipNone und beweist
ausdrücklich weder minifizierte Kronen-Coverage noch Blatttransmission, Wind oder
korrekte Karten-Normalmaps. Die gemeinsame Kronenrepräsentation kann den vorhandenen
Masked-/DoubleSided-Pfad nutzen.

## Gebackene Kronenansichten aus dem Generator

Nächste Implementierung: engineinterner CrownAtlas nutzt den bestehenden Renderer,
keinen zweiten Rasterizer. Der feine TreePrototype liefert Form und native Materialien;
mehrere orthografische Ansichten liefern Tiefe, Normalen und Material-ID. Grundfarbe
und MR-Werte stammen aus den zugehörigen Materialdeklarationen, nicht aus beleuchteten
PNG-Farben. Beleuchtung darf nicht in den Atlas eingebrannt werden. Unreal/RAGE nutzen
Impostor-/Billboard-LOD für fernes Laub; die Nahdarstellung bleibt echte Baumgeometrie.

Zuerst die Datengewinnung und deren Referenzbilder implementieren, danach die
Karten-/Normalmap-Anbindung und Auswahl am Instanzpfad. Auflösung und Ansichten
explizit deklarieren, Alpha aus tatsächlich bedeckten Samples, leere Pixel transparent.
Prototypen seriell vorbereiten und ihre feine Geometrie anschließend freigeben.
Abnahme: gleiche Referenzform aus dem bestehenden Generator, Materialien unabhängig
von der Bake-Beleuchtung, lesbare Krone in geöffneten Bildern; Speicher, Bakezeit
und Framezeit getrennt. Mip-Coverage, Winkelwechsel, Tiefenparallaxe und echte
Places-Frameraten bleiben bis zu ihren Messungen offen. Ein einzelnes Billboard
ist kein Nachweis für korrektes LOD unter Kamerabewegung.

Erster Datenschritt umgesetzt: CrownAtlas erfasst den feinen nativen TreePrototype
über Engine/Renderer. Je Ansicht ein eigener Renderzustand, ohne Tiefenhistorie
einer vorherigen Kamera. Orthografische gemeinsame Bounds mit einem Pixel Rand;
Near/Far und Kameradistanz folgen demselben Bounds-Maß. Materialien bleiben die
Originaldeklarationen. Gespeichert werden Normalen, Reverse-Z-Tiefe und Material-ID,
keine beleuchteten Farben. Die Konventionssuite deklariert die neue Quelldatei in
ihrer separaten Linkliste; liboutshine enthält sie bereits über den normalen Build.

Die erste Kamera ohne Licht wählte den bestehenden Flat-Pfad: alle 8577 bedeckten
Samples über vier Ansichten hatten Normalen null, bei korrekter Tiefe und ID.
`build/crown-atlas-data-probe.log`: vier gezielte Datenchecks rot, Exit 2.
Die Bake-Szene nutzt jetzt die Lichtdeklaration der vorhandenen Tree-Vorschau,
um den Normalenpfad zu wählen. Deren RGB-Ergebnis wird nicht gelesen. Das ist
eine Korrektur der Bake-Konfiguration, keine globale Änderung des Renderers.

`build/crown-atlas-normal-proof.log`: Exit 0, 19/19 PASS; neue Fixture 26 Checks.
Vier Birkenansichten zu 128×128: Rinde/Blatt-Samples 321/1869, 379/1719,
379/1810, 336/1764. Null Tiefe/ID-Widersprüche; quadrierte Normalenlängen
0.998478..0.999991 aus dem vorhandenen Half-Anhang, innerhalb der unveränderten
3e-3-Prüfung. Frühere Compiler-/Linkfehler sind nicht als Verhaltensnachweis gezählt.

Rohpayload: 4×128²×20 = 1.310.720 Bytes; keine Peak-Heap-Messung. Gemessene
12.300,644 ms umfassen Erfassung, Checks und PNG-Export, ohne vorheriges Grow;
der ursprünglich kürzere Logtitel wurde entsprechend präzisiert. Keine Framezeit.
[SET] Default 256 Pixel/8 Ansichten, maximal 4096 Pixel/64 Ansichten und insgesamt
2²⁴ Texel (bei aktueller Struktur 320 MiB Rohpayload); Grenzen sind Capture-Budgets,
keine Qualitätsabnahme. Feine CPU-/GPU-Quelldaten kosten zusätzlich Speicher.

Alle vier build/crown-atlas/birch-{0,1,2,3}.png geöffnet: gedrehte Generatorformen,
transparenter Hintergrund, dünne punktförmige Krone. PNG-Grundfarben werden direkt
aus Material-ID/BaseColour abgeleitet. Noch keine gefilterte Flächendeckung,
beleuchtbare Karten, Winkelwechselprüfung oder Wald in Places. Genau diese
Anbindung und der Vergleich mit der feinen Darstellung sind die nächste Arbeit.
