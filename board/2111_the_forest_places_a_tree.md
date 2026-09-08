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

## Native Karten aus den Kronendaten

Nächster Schritt: GeometryAt(view) liefert eine ausgerichtete Karte mit nativen
Farb-, Normal- und MR-Maps. Grundfarbe wird sRGB kodiert; MR und Normalen bleiben
linear. Texel-Normalen in die Kartenbasis transformieren: U nach rechts, V nach
unten, daher Tangentenhandedness -1. Metalness/Roughness-Faktoren stehen auf eins,
die Texturkanäle liefern die Materialwerte. Alpha bleibt echte Bake-Bedeckung.
Farb-/Normal-/MR-Randwerte deterministisch aus dem nächsten bedeckten Texel
fortsetzen, ohne Alpha zu vergrößern; Mips bleiben eingeschaltet.

Beweis: Alphabedeckung am Bake-Kamerastand, Normalen-Rücktransformation gegen die
erfassten Normalen, MR-Kanäle gegen die Ursprungsmaterialien. Karte unter anderer
Beleuchtung rendern und PNGs ansehen. Ebene Karten reproduzieren die gespeicherte
Tiefe noch nicht: Parallaxe, Winkelwechsel, minifizierte Coverage und Wald-Framerate
bleiben offen. Unreal/RAGE-Impostorprinzip übernommen, nicht dessen fertige Qualität
behauptet; die reichere nahe Geometrie bleibt erhalten.


## Native Kronenkarten geprüft, 2026-09-08

`CrownAtlas::GeometryAt(view)` erzeugt zwei Dreiecke mit eigenen nativen RGBA8-
Farb-, Normal- und MR-Bildern. Alpha bleibt unverändert; Randattribute werden durch
Mehrquellen-Breitensuche (Manhattan-Nachbarschaft) fortgesetzt. Keine Beleuchtung
im Farbbild, keine deaktivierten Mips. Die Karte bleibt eine Ebene in Baumkoordinaten.

`make suite SUITE=outshine/conventions`, `build/crown-card-restored-proof.log`:
Exit 2, 18/19 PASS; ausschließlich der bekannte Schach-Wiederholungsfall (2179)
rot. Kronenfall 92/92 Checks bestanden. Vier Ansichten × zwei Lichtrichtungen:
Bedeckung exakt gleich der feinen Aufnahme, maximale Normalenabweichung 0,00653804.
Grenze aus der Kodierung: je dekodierter Normalkomponente höchstens 1/255,
Vektorfehler höchstens sqrt(3)/255; Normalisierung und GPU-Rundung konservativ
mit Faktor vier, somit 4*sqrt(3)/255. MR-Abweichung höchstens ein 8-Bit-Code.

Negativkontrolle ausschließlich Tangentenhandedness -1 → +1:
`build/crown-card-handedness-negative.log`, Exit 2, 18/19 PASS; Kronenfall
84 Checks, genau acht Normalenfehler. Abweichung bis 1,99764, Bedeckung weiterhin
exakt. Quelle wiederhergestellt und obiger Abschlusslauf danach ausgeführt.
Der Schachfall bestand zufällig im Negativlauf; das behebt seine Intermittenz nicht.

Alle acht `build/crown-atlas/card-{0..3}-light-{0..1}.png` geöffnet, außerdem
`fine-0-light-{0,1}.png` unter identischer Kamera und Beleuchtung. Visuell nahezu
gleiche dünne, punktförmige Birkenkrone; ihre geringe optische Dichte ist bereits
in der feinen Quelle vorhanden. Relative lineare RGB-L1-Abweichung der Karte:
sum(abs(Karte-Fein))/sum(abs(Fein)) = 0,00333157 bzw. 0,00720716 für die beiden
Lichter. Das ist eine Diagnose dieses Blickpunkts, kein Fotorealismus-Orakel.
Die Karte reagiert auf Lichtwechsel und bewahrt helle Rinde sowie Blattnormalen.

Rohatlas unverändert 4*128*128*sizeof(Texel) = 1.310.720 Bytes; Kartenbilder
zusätzlich 4*128*128*3*4 = 786.432 Bytes ohne Mips, GPU-Kopien und Geometrie.
Capture, Prüfungen und PNG-Export einschließlich zweier Feinreferenzen zusammen
22.005,161 ms. Keine isolierte Bake-Zeit, Peak-Heap- oder Welt-Framerate-Abnahme.

Places sind von diesem privaten Exporter noch unberührt. Nächste Anbindung:
PieceMesh besitzt noch keinen Tangentenstream; PlacePiece setzt nur Normal/UV/
Colour. Vor Weltinstanzierung müssen Tangenten samt Handedness durch diesen
geteilten Pfad, mit derselben Normalenprüfung für mehrere rotierte Instanzen.
Weiter offen: Blickrichtungswahl, Tiefenreprojektion/Parallaxe, Alpha-Coverage bei
Minifizierung, Cache und asynchrone Vorbereitung, World.Instances-Verbrauch,
Streaming mehrerer Regionen und vollständige bewegte Places-Abnahme. WI bleibt aktiv.


## Nächster Schritt: Tangenten geteilter Instanzen

Vor Implementierung: vorhandenen SubjectResidency::Tangent-Stream auch in
PieceMesh/PlacePiece nutzen; kein zweiter Material- oder Shaderpfad. Optionaler
float4-Stream muss genau einen Tangentenvektor je Vertex tragen und UVs voraussetzen.
Einmaliger Upload pro Prototyp, vorhandene Instanzmatrix dreht N und T im litVertex.
Das folgt dem gemeinsamen Mesh-/Instanzprinzip von Unreal und RAGE; deren interne
Streamdetails werden nicht behauptet. Gesucht: PieceMesh trägt bislang nur Colours
zusätzlich zu StoredVertex, der normale Subject-Pfad hat Tangenten bereits.

Beweis erweitert PieceInstancesShareTheirGeometry: ausgeschnittene Karten mit
konstanter schräger Normalmap, zwei Instanzen, direkte und geclusterte Zeichnung,
Vorder- und um 180 Grad gedrehte Rückseite. Erwartete Weltnormale aus dekodiertem
Texel, UV-Basis, Rotation und doppelseitiger Normalenkonvention berechnen; alle
sichtbaren Stichproben müssen innerhalb der Half-Readback-Rundung liegen.
PNGs öffnen. Negativkontrolle entfernt ausschließlich das Tangenten-Layoutbit;
die Normalenprüfung muss rot werden. Ungültige Streamlängen vor Upload ablehnen.


## Tangenten geteilter Instanzen nachgewiesen, 2026-09-08

PieceMesh trägt optional float4-Tangenten; PlacePiece prüft Länge und UV-Voraussetzung,
lädt den Stream einmal pro Prototyp und wählt das vorhandene gemappte Vertexlayout.
Der Tangentenbuffer wächst nur bei tatsächlichem Tangenteninput bis zum benötigten
Vertexbereich; keine zusätzliche Vollreservierung für ungemapptes Terrain.
Die bestehenden Shader drehen Tangente und Normale über die jeweilige Instanzmatrix.

`make suite SUITE=outshine/conventions`, `build/piece-tangent-restored.log`:
Exit 2, 18/19 PASS, ausschließlich bekannter Schachfall 2179 rot. Instanzfall
90/90 Checks bestanden: zwei Instanzen, direkt/geclustert, Vorder-/Rückseite,
Alpha-Ausschnitt und dekodierte schräge Normalmap. Erwartung aus RGB(191,159,231),
Dekodierung RGB/127,5-1, Bitangente -Y und Rotation diag(-1,1,-1), anschließend
Rückseitenumkehr. Half-Readback-Grenze sqrt(3)/1024 aus konservativ einem ULP je
Komponente bei Einheitsnormalen. Ungültige Tangentenlänge und fehlende UVs abgelehnt.

Negativkontrolle nur `carried.Tangent=false`: `build/piece-tangent-negative.log`,
Exit 2, 18/19 PASS; Instanzfall 90 Checks, acht Normalenfehler, Alpha unverändert.
Original wiederhergestellt und Abschlusslauf danach ausgeführt. Vier positive
`build/instance-native/masked-{direct,clustered}-{front,back}.png` geöffnet;
beide Pfade zeigen identische ausgeschnittene Rechtecke mit korrekt unterschiedlicher
Vorder-/Rückseitenbeleuchtung. Negativbild ebenfalls geöffnet.

`make shots`, `build/piece-tangent-places.log`, Exit 0. Alle neun PNGs geöffnet,
Digests gegenüber dem letzten vollständigen Audit identisch, auch Feldkirch bei
5fa234c1. Koerbersee-/Malcesine-Webcam erneut geöffnet: weiter kahle Hänge,
aufgeblähte bzw. vorhangartige Felsflächen, keine Waldmasse, flaches dunkles Wasser.
Keine visuelle Verbesserung der Places durch diesen noch ungenutzten Tangenteninput.

| Place | Digest | p99 ms | Peak Heap MB |
|---|---|---:|---:|
| DarmstadtWest | e72d1925 | 2,90 | 356 |
| Wien | 8ff2d96d | 5,79 | 483 |
| Rosenheim | 7da2e093 | 7,42 | 386 |
| Husum | d60b18a7 | 3,16 | 232 |
| Olympiaturm | 07985050 | 4,21 | 607 |
| Graz | f93ff5b9 | 4,94 | 568 |
| Koerbersee | c99cdbe7 | 7,11 | 487 |
| Malcesine | 46e4db5c | 4,86 | 399 |
| Feldkirch | 5fa234c1 | 5,80 | 393 |

Je 120 Standframes, null über 16,67 ms. Kein bewegter Wald-/Streamingnachweis;
Schwankungen gegenüber früheren Einzelmessungen sind kein isolierter Kostenbeweis.
Nächster Schritt bleibt die tatsächliche Kronen-/Materialregistrierung für
World.Instances mit geographischer Platzierung, Blickrichtungswahl und Cache.
Tangenten für reine Rotationen und uniforme Baumskalierung abgedeckt; beliebige
nichtuniforme oder spiegelnde Transformationskonformität wird hier nicht behauptet.


## Katalogidentität vor Welt-Cache

Vor Implementierung: Shipping::Stands liest TreeSpecies in einen lokalen Vektor,
vergibt Cluster-IDs nach dessen sortierter Reihenfolge und verwirft danach die
Generatorparameter. Der Render-Cache kann die gezeichnete Cluster-ID deshalb noch
nicht in dieselbe Baumvorlage auflösen. Vorlagen im vorhandenen Shipping-Katalog
halten und ausschließlich per ClusterId als const TreeSpecies* zugänglich machen;
Gebäude-ID und unbekannte IDs liefern nullptr. Keine zweite Verzeichniseinlesung
im Renderpfad und keine erneute ID-Vergabe. Gemeinsamer unveränderlicher Assetkatalog
wie bei Unreal-/RAGE-Prototypinstanzierung; keine Behauptung über deren private API.

Beweis: shipped vegetation/species laden, alle sortierten Quelldateien gegen die
Katalogzuordnung prüfen, Platzierungen über Shipping.Placing/Drawing erzeugen und
jede Flora-ID samt relativer Höhe gegen ihre Quelle prüfen. Zugriff bleibt nach
wiederholtem Stands identisch. Negativkontrolle löst alle IDs auf die erste Art auf;
Identitätsprüfung muss rot werden. Keine neue Places-Bildwirkung vor Cache-Verbrauch.


## Shipped-Katalog behält Baumvorlagen, 2026-09-08

Shipping hält jetzt die eingelesenen TreeSpecies über seine Lebensdauer. TreeFor
löst dieselbe Cluster-ID auf, die ForestDraw vergibt; Gebäude-ID, unbekannte ID
und unvorbereiteter Katalog liefern nullptr. Erneutes Stands behält die Adressen.
Kein Baumwachstum oder Atlas-Bake im Framepfad hinzugefügt.

Fixturepräzisierung: jede Quelldatei wird einmal als gezielter Solid mit Variant
und 1,5-facher Quellhöhe über Yield.Place in einen echten RegionPool-Sink gesetzt.
Shipping.Drawing führt diese Platzierungen über die registrierten Generatorränge.
Das isoliert die Katalogzuordnung; zufällige Occupy-Verteilung und ökologische
Artenwahl sind ausdrücklich keine Abnahme dieses Tests.

`make suite SUITE=outshine/conventions`, `build/species-catalogue-restored.log`:
Exit 2, 18/19 PASS, ausschließlich bekannter Schach-Wiederholungsfall 2179 rot.
ForestInstancesKeepTheirSpecies: 159/159 Checks. Alle 31 Arten behalten Name,
Prototyphöhe, Cluster-Rückverweis und relative Skalierung im realen DrawSet-Pfad.
Negativkontrolle TreeFor zeigt für jede gültige ID auf Species_[0]:
`build/species-catalogue-negative.log`, Exit 2, 18/19 PASS; genau 60 Fehler
(30 falsche Arten × Katalog- und Instanzidentität), 159 Checks insgesamt.
Quelle wiederhergestellt, positiver Abschlusslauf danach.

Vorher zwei Compile-Befunde: Shipped.h zog den kompletten Corridors-Header samt
Fit.h herein; Header verwendet nun Vorwärtsdeklaration, Implementation inkludiert
Corridors. Der Test qualifiziert Generators::Ground, da die Shipped-Header zusätzlich
den gleichnamigen Welt-Namespace sichtbar machen. Logs species-catalogue-proof.log
und species-catalogue-header-proof.log enthalten die roten Compile-Befunde.

Noch kein Render-Cache-Verbrauch dieser Zuordnung, deshalb keine Places-Bildänderung
und keine neue visuelle Weltabnahme. Nächster Implementierungsschritt: nur tatsächlich
benötigte Cluster vorbereiten, CrownAtlas pro Vorlage teilen, Material-/Bildregistrierung
an Live anbinden und geografische Instanzen mit kameraabhängiger Ansicht zeichnen.
Der vorhandene CrownAtlas::Bake ist eine synchrone Referenzproduktion mit eigenen
Engines; unverändert im Updates-Frame aufgerufen wäre er ein mehrsekündiger Stall.
Vor Live-Anbindung braucht die Produktion einen begrenzten Vorbereitungs-/Cachepfad.


## Vor Live-Cache: parallelen GPU-Bake messen

Vor Experiment: Tasks bietet bereits Post/Done/Wait mit Besitz bis zum Abschluss.
CrownAtlas::Bake erzeugt jedoch pro Ansicht eine Engine und liest synchron zurück.
Direktes Verschieben auf Tasks beseitigt den CPU-Wait des Aufrufers, beweist aber
kein GPU-Budget. Zuerst parallel zum Bake einen separaten nativen Renderer zeichnen,
Linearpixel-Stabilität prüfen und p50/p95/p99 samt schlechtestem Frame messen.
Das ist eine Rate, kein 60-fps-Gate; Quelle der Krone und Bildorakel bleiben gleich.
Der triviale Kontrollrenderer ist ausdrücklich keine vollständige bewegte Welt.

SDL_CreateGPUDevice-Dokumentation (https://wiki.libsdl.org/SDL3/SDL_CreateGPUDevice)
und lokaler Header benennen keine explizite Thread-Garantie für die komplette
Engine-Produktion. Offscreen-Experiment auf dieser Zielplattform ersetzt keine
plattformübergreifende Thread-Abnahme. Daher noch keine produktive Worker-Anbindung.
Unreal/RAGE-Assetvorbereitung bleibt Architekturziel; ob dieser vorhandene komplette
Referenzrenderer als Hintergrundproduzent taugt, entscheidet die Messung.


## Paralleler Referenz-Bake: funktionsfähig, nicht als Live-Budget belegt

`make suite SUITE=outshine/conventions`, `build/crown-concurrent-recovery.log`,
Exit 2, 18/19 PASS; nur bekannter Schachfall 2179 rot. Kronenfall 97/97 Checks:
Bake auf Tasks(1), Vordergrundrenderer auf Aufruferthread, danach unveränderte
Kronen-Bedeckungs-/Normalen-/Materialorakel. Vordergrund-Linearpixel über sämtliche
Messungen exakt gleich. PNG concurrent-foreground.png und Kronenkarten geöffnet:
weißes Kontrolldreieck stabil, bekannte dünne Birkenkrone unverändert.

| 720p Kontrollszene, Render + synchroner Linear-Readback | n | p50 ms | p95 ms | p99 ms | worst ms | über 16,67 ms |
|---|---:|---:|---:|---:|---:|---:|
| vor Bake | 120 | 3,833 | 3,982 | 5,664 | 6,403 | 0 |
| während Bake | 1474 | 8,797 | 9,317 | 10,961 | 22,512 | 4 |
| nach Bake | 120 | 3,940 | 4,249 | 4,393 | 4,426 | 0 |

Kronencapture im Worker 14.098,837 ms, vollständige Beobachtung innerhalb des
30-s-Limits. Mediananstieg währenddessen 8,797-3,833 = 4,964 ms; danach nur noch
3,940-3,833 = 0,107 ms. Diese Rückkehr widerspricht einer bloß bleibenden Drift.
CPU-/GPU-/Readback-Konkurrenz sind damit NICHT getrennt. Ein Kontrolldreieck mit
Readback ist weder eine bewegte Welt noch ein reiner GPU-Timer. Rate, kein Gate.

Erster unabhängiger Lauf `build/crown-concurrent-run.log`: gleiche 97 Checks,
Worker 13.861,582 ms; allein p99 6,569 ms, währenddessen 10,371 ms, schlechteste
21,439 ms, 2/1306 über 16,67 ms. Auch dort gleiche Linearpixel. Vorgelagerter
Compile-Fehler wegen Test-Namensüberschattung in crown-concurrent-proof.log behoben.
Die Schlusszeit 25.692,410 ms umfasst den gesamten Versuch einschließlich
Kontrollrenderer, Vergleichsbildern und PNG-Export; sie ist keine reine Bake-Zeit.

Entscheidung: keine ungebremste Instanziierung des vollständigen Referenz-Bakes im
Updates-Pfad und keine 60-fps-Abnahme aufgrund eines Worker-Threads. Nächster Schritt
ist ein wiederverwendbares, versioniertes Kronenartefakt mit Quell-/Generatoridentität
und geprüfter vollständiger Material-/Normalen-/Tiefen-Rücklesung; Vorbereitung kann
außerhalb des aktiven Framebudgets erfolgen, Streaming liest das Ergebnis.
Cache-Invalidierung bei Generatoränderungen gehört dazu. Ein dynamischer Cache-Miss
braucht weiterhin begrenzte Produktion und reichere Nahgeometrie; diese Messung
rechtfertigt weder dauerhaft fehlende Vegetation noch eine feste Offline-Welt.
Keine produktive Weltänderung in diesem Experiment, WI bleibt aktiv.


## Kronenartefakt: Formatentscheidung vor Implementierung

Vorhandener glTF-Emit verweigert Bilder und Transmission und schreibt nicht alle
Materialerweiterungen. Kein geeigneter unveränderter Kronen-Cache. Eigenes internes
Little-Endian-Format mit Magic/Version, Quellidentität (FNV64 über vom Aufrufer
übergebene Provenienz), Shape/Bounds/Viewrichtungen, Kernmaterialdaten und allen
Normal-/Depth-/Surface-Texeln. Abschließende FNV64-Prüfsumme über gesamten Inhalt.
FNV ist Integritäts-/Invalidierungsdiagnose, keine kryptographische Authentisierung.

Kernmaterialserialisierung umfasst RGBA, Metalness, Roughness, Emission, Alpha,
CoverageCut, DoubleSided, Unlit. Gleichheit mit daraus rekonstruiertem Material
verweigert jede unbehandelte Erweiterung/Map statt stiller Verluste; defaulted
Material-Gleichheit betrachtet automatisch auch zukünftige Felder. Gesamtlänge und
bestehendes Texelbudget vor Allokation prüfen; nichtfinite oder ungültige Samples
abweisen. Materialerweiterungen können mit einer neuen Version ergänzt werden.

Beweis: echter gebackener Birkenatlas → Bytes → Atlas; Materialien, Richtungen,
Bounds und jeder Texel exakt gleich. Vorhandene Kartenrender aus zurückgelesenem
Atlas prüfen. Verkürzung, Bytekorruption, falsche Version und geänderte Provenienz
müssen abgewiesen werden. Das Artefakt schreibt noch keine Cache-Datei im Frame;
Quelle plus Generator-/Shaderidentität muss der spätere Cache-Aufrufer liefern.
Unreal/RAGE-Prinzip abgeleiteter Assetdaten übernommen, kein Anspruch auf deren
privates Dateiformat. Disk-Cache, atomische Veröffentlichung und Live-Verbrauch folgen.


## Kronenartefakt Version 1 implementiert und zurückgerendert

CrownAtlas::Encode/Decode kodieren explizite Little-Endian-Skalare, keine Struct-
Rohspeicherblöcke. Header 64 Bytes, Kernmaterial 52 Bytes, Ansichtsrichtung 24 Bytes,
Texel 20 Bytes, abschließende Prüfsumme 8 Bytes. Birke: 64 + 2*52 +
4*(24 + 128*128*20) + 8 = 1.310.992 Bytes, geschrieben nach
`build/crown-atlas/birch.crown`. Kein Verlust gegenüber 1.310.720 Bytes Rohtexeln;
272 Bytes sind Metadaten und Materialien.

Defaulted Material-Gleichheit vergleicht sämtliche Felder; Encode verweigert eine
Oberfläche, deren Eigenschaften nicht durch das rekonstruierte Kernmaterial
repräsentiert werden. Texturen/Materialerweiterungen werden dadurch nicht still
entfernt. Das ist eine Einschränkung des Cacheformats, keine Änderung der Render-
Materialfähigkeiten. Aktuelle untexturierte Baum-Quellmaterialien passen vollständig.

`make suite SUITE=outshine/conventions`, `build/crown-artifact-restored.log`:
Exit 2, 18/19 PASS; ausschließlich bekannter Schachfall 2179 rot. Kronenfall
114/114 Checks: Bounds, Richtungen, sämtliche Materialien und jeder Normal-/Depth-/
Surface-Texel exakt nach Decode; erneutes Encode byteidentisch. Geänderte Provenienz,
fehlende Provenienz, vier Kürzungen und Bitkorruption abgelehnt. Ungültige Version,
überhöhte Dimensionen und NaN-Material werden auch mit neu berechneter Prüfsumme
verweigert. Dekodierte Daten ersetzen im Test den Originalatlas vor den bestehenden
Karten-/Beleuchtungs-/Normalenprüfungen; diese Orakel bleiben unverändert.

Negativkontrolle ausschließlich Prüfsummenvergleich ausgesetzt:
`build/crown-artifact-checksum-negative.log`, Exit 2, 17/19 PASS (zusätzlich Schach).
114 Checks, genau ein Fehler: Nutzinhalt-Bitkorruption wird fälschlich akzeptiert.
Quelle wiederhergestellt, obiger Abschlusslauf danach durchgeführt.

Karten-PNGs nach Rücklesung geöffnet und mit feiner Quelle verglichen: unveränderte
Silhouette und Beleuchtung, weiterhin dünne Krone. Keine neue Welt-/Places-Bildwirkung;
Cacheformat und Referenzfixture besitzen noch keinen produktiven Live-Verbrauch.
Die Testprovenienz besteht aus realem Speziestext und expliziter Test-Revisionskennung.
Automatischer Fingerprint der tatsächlichen Generator-/Shaderquellen, Dateicache,
atomisches Schreiben, asynchrones Lesen und begrenzte Veröffentlichung an Live bleiben
als nächste Schritte offen. FNV64 ist keine Authentisierung untrusted Fremdartefakte.


## Automatische Producer-Provenienz vor Dateicache

Vor Implementierung: Make erzeugt nach Strip und Shaderbau einen stabilen Header
unter build/. SHA-256 umfasst sortierte Pfade und Inhalte aller Code-/Headerdateien
unter src/ und include/, Tier-Reaches, Makefile, Buildskript, Fingerprintskript,
Shader-Toolchain-Pin sowie tatsächlich erzeugtes SPIR-V. C++-Version, Plattform und
SDL/Shadercross-Paketversionen ergänzen die Identität. Header nur bei geändertem
Inhalt ersetzen. CrownAtlas bindet den Header ein: Identität gehört zum kompilierten
Producer und wird nicht aus einer möglicherweise inzwischen erneuerten Laufzeitdatei
bezogen. Species-JSON und Pixels/Views ergänzen sie für die einzelne Krone.

Das ist bewusst eine konservative Abhängigkeit (auch manche irrelevante Codeänderung
invalidiert), keine fragile handgepflegte Auswahl von Baumgenerator-Dateien. Prinzip
abgeleiteter Daten wie Unreal/RAGE; keine Behauptung über deren privaten Fingerprint.
Beweis: Make zweimal ohne Änderung erhält Kennung; tatsächliche Codec-Quelländerung
ändert sie; Wiederherstellung erhält Originalkennung. Bestehender Artefakttest nutzt
die automatisch erzeugte Provenienz und prüft geänderte Species/Shape. Dateicache,
atomische Veröffentlichung und asynchroner Verbraucher bleiben danach offen.


## Producer-Fingerprint wird mitgebaut, 2026-09-08

`make crown-provenance` erzeugt build/CrownBuild.h nach Strip und Shaderbau;
`make` und `make db` hängen davon ab. SHA-256 über längenpräfixierte Pfade/Inhalte,
sortiert und eindeutig; SPIR-V und Werkzeug-/Paketversionen wie oben beschrieben.
Header wird bei gleichem Inhalt nicht neu geschrieben, sonst atomisch ersetzt.
CrownAtlas::ProvenanceFor bindet den kompilierten Fingerprint an Species-JSON und
Pixels/Views. Der Codec-Test verwendet keine handgesetzte Generatorrevision mehr.

Kontrollen (je Make-Gate beendet): crown-provenance-first.log/repeat.log zeigen
dieselbe Kennung 6ad990cb…; Codec-Konstante Version 1 → 2 im echten Quellfile ergibt
fc23c099… (crown-provenance-mutation.log). Quelle wiederhergestellt, Kennung wieder
6ad990cb… (crown-provenance-restored.log), Header per cmp identisch zur Sicherung.
Damit werden auch uncommittete Quelländerungen erfasst. Board-/Commitänderungen
allein sind kein Fingerprint-Eingang.

`make suite SUITE=outshine/conventions`, build/crown-provenance-suite.log:
Exit 0, 19/19 PASS; Kronenfall 115 Checks. Shape- und Species-Änderungen ergeben
unterschiedliche Provenienz; vorhandene Cache-Verweigerungen, Rundlauf und
Normal-/Bedeckungsprüfungen bleiben grün. Zwei Karten-PNGs unter gegensätzlicher
Beleuchtung erneut geöffnet, unveränderte dünne Krone. Der sporadische Schachfehler
2179 bleibt offen trotz dieses grünen Einzelruns.

Anschließend db-Abhängigkeit auf den generierten Header ergänzt;
`make db`, build/crown-provenance-db.log, Exit 0: Bibliotheken/Client gebaut,
compile_commands.json für 171 Einheiten erzeugt. Makefile ist selbst Eingang;
diese Ergänzung erzeugt erwartbar die endgültige Kennung f3e7aa56… und baut den
Producer damit neu. Externe Bibliotheken sind über Paketversionen identifiziert,
nicht über einen vollständigen transitiven Hash jedes System-Binaries.

Nächster Schritt: Artefaktdateien atomisch veröffentlichen, begrenzt asynchron
lesen und ihren Inhalt über diese automatisch erzeugte Provenienz prüfen.
Noch kein produktiver Dateicache/World.Instances-Verbrauch; WI bleibt aktiv.


## Begrenzter asynchroner Kronen-Dateicache

Vor Implementierung: vorhandenen ContentStore nach Korrektur 2181 verwenden.
CrownCache besitzt Store und begrenzte Pending-Liste, leiht Tasks (muss länger leben).
Provenienz → SHA256-Dateiname; Decode prüft zusätzlich die eingebettete Provenienz.
Request koalesziert identische offene Schlüssel und verweigert bei voller Liste;
Take prüft Tasks::Done und übergibt fertige Ergebnisse ohne IO auf dem Aufrufer.
Default [SET]: zwei offene Reads, je höchstens 16 MiB Dateipayload; keine unbegrenzte
Jobliste. Konstruktion/Publikation sind Vorbereitungsarbeit, nicht Updates-Arbeit.
Destruktor wartet auf eigene Jobs vor Store-Freigabe, niemals regulär pro Frame.

Beweis nutzt echten gebackenen Atlas: publizieren, über Worker lesen und existierende
Kartenprüfungen aus dem geladenen Ergebnis ausführen. Pending-Grenze, Duplikat,
fehlende/geänderte Provenienz und übergroße Datei prüfen. Worker mit Test-Latch
anhalten: Request und Take müssen zurückkehren, ohne den Latch abzuwarten.
Unreal/RAGE shared derived-data cache als Prinzip; dieser Schritt baut noch keinen
GPU-Produzenten in den aktiven Weltframe und ersetzt nicht den geplanten Live-Verbrauch.


## Asynchroner Dateicache implementiert; 2181 geschlossen

CrownCache nutzt ContentStore statt eines zweiten Dateischreibers. Publish kodiert
und veröffentlicht vollständig, Read stellt begrenzte Tasks-Aufträge ein, Take
übergibt abgeschlossene Ergebnisse. Identische offene Provenienzen werden koalesziert.
Default: zwei offene Aufträge, je 16 MiB Dateiobergrenze. Dateiname SHA256(Provenienz),
zusätzliche Codec-Provenienzprüfung nach IO. Fehler/Miss liefern ein Ergebnis ohne
Atlas samt Fehlertext. Tasks muss den Cache überleben; Destruktor drainiert eigene
Handles vor Store-Freigabe. Konstruktion, Publish und Destruktion gehören nicht in
reguläre Updates. Keine Behauptung eines generell lockfreien Aufrufpfads: Tasks
verwendet bereits kurze Mutex-Operationen für Post/Done.

ContentStore::Read akzeptiert jetzt ein explizites optionales Byte-Limit und prüft
Dateigröße vor Payload-Allokation. CrownCache setzt dieses Limit immer.
Keep liefert Erfolg/Fehlschlag und öffnet temporäre Namen exklusiv, mit maximal
64 Kollisionsversuchen. Bestehende temporäre Dateien werden nicht abgeschnitten.
SourceSet behandelt Cache-Schreibfehler weiterhin über vorhandene Zähler als nonfatal.
Atomische Sichtbarkeit durch rename; keine neue Crash-Durability-/fsync-Garantie.

`make suite SUITE=outshine/conventions`, build/crown-cache-restored.log:
Exit 0, 19/19 PASS; Kronenfall 131 Checks. Echter Atlas wird publiziert, über Worker
zurückgelesen und für sämtliche vorhandenen Kartenrender-/Normalenorakel verwendet.
Worker per Latch blockiert: Read/Take kehren vor dessen Freigabe zurück, Duplikat
belegt keinen zweiten Auftrag, dritter offener Auftrag wird abgelehnt. Stale Bytes
unter falschem Provenienzschlüssel abgewiesen. Exact-byte-Readlimit, Temp-Kollision,
zweite Store-Instanz, fehlgeschlagene rename-Veröffentlichung auf ein Verzeichnis
und Destruktion mit offenem Auftrag geprüft. Karten-PNGs geöffnet: unveränderte
dünne Birkenkrone mit gleicher Beleuchtung; noch keine Weltintegration.

Negativkontrolle nur fopen-Modus wbx → wb in ContentStore:
build/crown-cache-exclusive-negative.log, Exit 2, 17/19 PASS (zusätzlich bekannter
Schachfall). Kronenfall 131 Checks, genau ein Fehler: reservierte fremde temporäre
Datei wird zerstört. Quelle wiederhergestellt, obiger Abschlusslauf danach.
Der grüne Schachfall im Abschlusslauf schließt seine Intermittenz 2179 nicht.

2181 ist damit für explizite Lesegrenzen und isolierte Veröffentlichung erledigt.
Nächster Schritt ist der produktive Verbraucher: Materialien/Bilder geladener Kronen
bei Live registrieren und World.Instances auf geteilte Kronengeometrie abbilden.
Cache-Miss-Produktion/Vorbereitung und Blickrichtungswahl bleiben dabei erforderlich;
der Dateicache allein zeichnet noch keinen Wald. WI 2111 bleibt aktiv.


## Nächster Handoff: Materialien an residente Draws anhängen

Vor Implementierung: SetMaterials ersetzt derzeit Slots, Batches und Indexzahl;
das ist ein vollständiger Aufbau und kein Ankunftspfad für Kronen. BindSurface
kann bereits genau eine Oberfläche hochladen. Diesen vorhandenen Pfad über
AppendSubjectMaterials zugänglich machen: vollständige Pass-Kompatibilität vor
Mutation prüfen (auch beim Glaspass), danach nur neue Slots binden. Bestehende
Slot-IDs, Batches, Geometrie und Bilder erhalten. SetMaterials bleibt Ersetzung.
Keine neue öffentliche Engine-API und kein verstecktes Hilfsmesh.

Unreal/RAGE sind hier Benchmark für residente, geteilte Foliage-Ressourcen;
übernommen wird das Prinzip inkrementeller Asset-Ankunft, keine behauptete private
API. Prüffall PieceInstancesShareTheirGeometry: erst vorhandene Geometrie zeichnen,
dann Texturmaterial anhängen; alter Draw bleibt pixelgleich, neuer instanzierter
Draw nutzt den neuen Slot. Ein Batch mit kompatibler erster und unzulässiger
transmissiver zweiter Oberfläche muss ohne Teilmutation scheitern. Negativkontrolle:
Append auf den ersetzenden Set-Pfad umlenken; bestehende Draws/Slot-IDs müssen rot
werden. PNG vor/nach Ankunft öffnen. Diese private Renderer-Funktion ist Voraussetzung
für Live-Registrierung; allein erzeugt sie noch keine Places-Vegetation.

## Residente Material-Ankunft und Unlit-Pieces implementiert

AppendSubjectMaterials prüft den kompletten Batch für beteiligte Renderpässe vorab
und bindet nur neue SurfaceSlots über das vorhandene BindSurface. Bestehende
Texturen/Slots/Geometrie bleiben erhalten; Draw-Tabellen werden bei Material-Ankunft
neu gebunden. SetMaterials bleibt der vollständige Ersetzungspfad. Noch keine
öffentliche Engine-API, kein Live-Materialregister, kein produktiver Kronenverbrauch.
Die API garantiert Pass-Kompatibilität vor Mutation, keine neue GPU-OOM-Transaktion.

Dabei gefunden und repariert: 2182, Material::Unlit wurde bei Pieces ignoriert.
Retable wählt jetzt für Unlit das Layout ohne Normal/Tangent, erhält UV/Farbe und
schreibt den BaseColour-Faktor in den vorhandenen Emitted-Stream. Dieser wird nur
bis zum benötigten Piece vergrößert; der pro Piece gemerkte Faktor verhindert
erneuten Upload bei unveränderten Tabellenaufbauten. Prototypdaten bleiben geteilt.
Die erste reine Layoutkorrektur ergab Schwarz, weil Pieces den Emitted-Stream nicht
befüllten. Nach Upload ergaben sich korrekt halbe Texturwerte: Die Fixture hatte
fälschlich Weiß als nativen Materialdefault angenommen. Nun deklariert sie
BaseColour=(0.5,1,0.5) und prüft sRGB-Dekodierung mal Faktor; kein Oracle abgesenkt.

`make suite SUITE=outshine/conventions`, build/material-append-restored.log:
Exit 0, 19/19 PASS; PieceInstancesShareTheirGeometry 105 Checks. Alter Draw bleibt
pixelgleich nach Material-Ankunft; neuer Draw nutzt den angehängten Texturslot.
Ein Batch mit zulässiger erster und unzulässiger transmissiver zweiter Oberfläche
wird vollständig verweigert. Unlit-RGB stimmt innerhalb 1/4096 linear mit dem
berechneten Texturwert mal Faktor überein. Bestehende Mask-/Normal-/Instanzprüfungen
bleiben grün. Gesicherter Fall: build/material-append-restored-case.log.

Kontrollen, jeweils eigenes beendetes Make-Gate:
- material-append-replace-negative.log: Append auf Set umgeleitet; SIGSEGV durch
  ungültige alte Draw-Slots, zusätzlich bekannter Schachfall 2179 rot. Das ist ein
  breiter Zerstörungskontrolllauf, kein gezielter Pixeloracle-Nachweis.
- material-append-slot-negative.log: bei vollständiger Slot-Tabelle ersten/letzten
  Slot vertauscht. 105 Checks, sechs gezielte Fehler: bestehende Pixel und neuer
  Materialwert falsch. Exit 2, 17/19 PASS, zusätzlich 2179. Fall separat gesichert.
- material-append-unlit-negative.log: nur Unlit-Zweig ausgesetzt. 105 Checks, vier
  Fehler am neuen Materialwert; Exit 2, 18/19 PASS, alle anderen Fälle grün.
- Quellen wiederhergestellt; obiger Abschlusslauf danach. Keine Mutation verblieben.

PNG vor/nach Material-Ankunft und vier Mask-/Rückseiten-PNGs geöffnet: alter Draw
bleibt, grüner Draw kommt hinzu; Maskhälften stimmen. Fixture setzt Exposure=1,
damit die Unlit-Fläche im technischen Bild sichtbar ist. Automatische Belichtung
für KeyLux=20000 machte sie zuvor im PNG fast schwarz; Linear-Readback war korrekt.
Das ist eine Diagnoseaufnahme, keine fotorealistische Beleuchtungsabnahme.

`make shots PLACE='--measures Koerbersee'`, build/material-append-koerbersee.log,
Exit 0: weiterhin c99cdbe7. PNG und Webcam geöffnet: keine Bildänderung, weiterhin
fehlende Bäume, weiche Felsformen/Klassenflecken und flaches dunkles Wasser.
120 residente Standframes: p50 6.96, p95 7.18, p99 7.28 ms; 0/120 über 16.67 ms;
Peak Heap laut Instrument 495 MB. Kein bewegter Wald-/Streaming-/720p60-Nachweis.
2182 ist geschlossen; 2179 bleibt trotz grünem Abschlusslauf wegen Intermittenz offen.

Nächster produktiver Übergang: Live muss geladene Kronenmaterialien mit stabilen
Piece-Zuordnungen und geklärter Lebensdauer registrieren; World.Instances müssen
geografisch korrekt geteilte Kronen-Draws erreichen. Cache-Miss-Vorbereitung,
Blickrichtungswahl und begrenzte Tile-Residency bleiben Teil der Umsetzung, nicht
Abnahme durch diesen privaten Renderer-Test ersetzen. WI 2111 bleibt active.

## Live-Handoff mit stabilen Piece-Materialreferenzen

Vor Implementierung: Native Geometriematerialindizes und registrierte Prototyp-
materialien sind verschiedene Namensräume. PieceSurface unterscheidet sie explizit;
der Renderer löst beide über kompakte Slot-Vektoren auf. Keine Hochbit-Magie und
keine große sparse Tabelle. Vorhandene native Surface-Aufrufer behalten ihre Bedeutung.
Live::RegisterPieceSurfaces übernimmt eine native Geometry samt Bildern, löst deren
Texturbindungen auf und nutzt AppendSubjectMaterials. Registrierung liefert den
ersten fortlaufenden Handle, kein flüchtiges GPU-Slot-Index. Live hält Quellen und
aufgelöste Oberflächen bis zu seinem Ende; Registrierung erfolgt pro Prototyp,
nicht pro Tile/Instanz. Die kommende Kronenresidenz begrenzt den Prototypkatalog.

Bei Build/Restand werden registrierte Materialien nach den neu aufgelösten nativen
Materialien wieder in die Tabelle eingefügt und ihre stabilen Handles neu auf
Renderer-Slots abgebildet. Ein normaler Register-Aufruf darf weder Grundgeometrie
neu bauen noch alte Texturen neu hochladen. Vollständiger Restand behält dagegen
seinen bestehenden vollständigen Aufbau; dessen Beseitigung gehört zu 2124.
Unreal/RAGE dienen als Benchmark für stabile Assetreferenzen trotz Streaming;
hier vorhandene Geometry-Eigentümerschaft und Materialauflösung wiederverwenden.

Beweis im GPU-Piece-Fall: Geometry mit Textur ohne Parts registrieren, Quelle aus
dem Aufrufer entfernen, zwei Welt-Pieces über den Handle zeichnen; zusätzliche
Registrierungen dürfen nichts überschreiben. Native Geometrie mit geändertem
Materialbestand neu aufbauen, ohne Piece-Neuanlage; Textur/Handles müssen weiter
stimmen. Fehlendes Bild/fehlende Oberflächen/ungeeigneter Pass verweigern und den
Handlezähler erhalten. Negativkontrolle: registrierte Handles durch native
Materialindizes auflösen; gezielte Farb-/Persistenzprüfungen müssen rot werden.
PNG vor/nach Restand öffnen. Anschließend muss der Kronenverbrauch diese Registry
erreichen; die Registry allein schließt 2111 nicht.

## Live besitzt registrierte Prototypmaterialien und bindet sie erneut

Implementiert: PieceSurface unterscheidet Geometry- und Registered-Referenzen;
SubjectDraw löst sie über zwei kompakte Slot-Vektoren auf. Native uint32-Aufrufer
bleiben native Materialindizes. RegisterPieceSurfaces übernimmt Geometry/Bilder,
löst native Texturen auf, hängt GPU-Slots an und liefert den ersten stabilen
Registrierungsindex. Keine zusätzlichen Mesh-Parts zur Materialregistrierung.
Die im Test lokale Quellen-Geometry ist beim späteren Zeichnen bereits zerstört.

Live hält Quellen und aufgelöste Oberflächen. Build/Restand fügt sie nach den neu
aufgelösten nativen Materialien ein und erneuert die Handle→Slot-Zuordnung.
Die Zuordnung erfolgt nach Planaufbau; die nächste neue Oberfläche erzwingt keinen
Grundgeometrie-Neuaufbau. Vollständiger Restand behält seinen vorhandenen Neuaufbau.
Bei leerer Grundgeometrie ist die zusätzliche Materialbindung auf vorhandene
Registrierungen beschränkt; Szenen ohne solche Registrierungen behalten ihren Pfad.

`make`, build/live-piece-surfaces-build.log: Exit 0.
`make suite SUITE=outshine/conventions`, build/live-piece-surfaces-restored.log:
Exit 2, 18/19 PASS; ausschließlich bekannter intermittenter Schachfall 2179 rot.
PieceInstancesShareTheirGeometry: 130 Checks, keine Fehler. Zusätzliche Prüfungen:
leere Quelle, fehlendes natives Bild und fehlender Transmissionspass verweigert,
ohne Registrierungsindex zu verbrauchen. Zwei Instanzen mit übernommenem Bild;
weitere Registrierung verändert kein Pixel. Zwei weitere native Materialien und
echter Restand: dieselben Piece-IDs, alle Pixel identisch. Zweiter registrierter
Handle zeichnet anschließend seine eigene blaue Textur. Fall separat gesichert
in build/live-piece-surfaces-restored-case.log.

Negativkontrolle nur in RestorePieceSurfaces: registrierte Handles nach Neuaufbau
auf gleichnamige native Slot-Indizes abgebildet. build/live-piece-surfaces-rebind-
negative.log, Exit 2, 18/19 PASS; 130 Checks, genau zwei Fehler: bestehende Pixel
ändern sich nach Restand, zweiter Handle verliert Blau. Sämtliche anderen Fälle
einschließlich Schach in diesem Lauf grün. Quelle wiederhergestellt; obiger
Abschlusslauf danach. Gesicherter Fall live-piece-surfaces-rebind-negative-case.log.
Registrierungs-PNGs vor/nach Restand und mit zweitem Prototyp nach Abschluss erneut
geöffnet: zwei identische grüne Instanzen bleiben, blauer Prototyp kommt hinzu.

`make shots`, build/live-piece-surfaces-places.log: Exit 0. Alle neun PNGs geöffnet;
Digests unverändert gegenüber dem bisherigen Audit. Keine behauptete neue Vegetation.

| Place | Digest | p99 ms | Peak Heap MB |
|---|---|---:|---:|
| DarmstadtWest | e72d1925 | 2.82 | 356 |
| Wien | 8ff2d96d | 5.93 | 474 |
| Rosenheim | 7da2e093 | 7.82 | 385 |
| Husum | d60b18a7 | 3.15 | 232 |
| Olympiaturm | 07985050 | 4.08 | 553 |
| Graz | f93ff5b9 | 4.76 | 608 |
| Koerbersee | c99cdbe7 | 7.56 | 483 |
| Malcesine | 46e4db5c | 5.10 | 392 |
| Feldkirch | 5fa234c1 | 5.35 | 410 |

Jeweils 120 residente Standframes, jeweils 0 über 16.67 ms. Kein bewegter
Wald-/Streaming-/Gesamtzielnachweis. Visuell weiterhin kahle Hügel/fehlende Bäume,
vereinfachte Gebäude, dunkles flaches Wasser; Malcesines regelmäßige Felsvorhänge,
Husums weiße Kaibänder und Feldkirchs übertiefer Flusseinschnitt unverändert.

Nächster Schritt: geladene Kronen tatsächlich mit RegisterPieceSurfaces verbinden
und World.Instances auf gemeinsame Crown-Pieces abbilden, einschließlich korrektem
geografischen Frame und Blickrichtungswahl. Die Registry hält einen Katalog über
die Live-Lebensdauer; sie ist noch keine budgetierte Prototyp-Eviction. Nicht pro
Tile erneut registrieren. Vorbereitung fehlender Artefakte und begrenzte Katalog- /
Tile-Residency bleiben erforderlich. 2111 bleibt active.

## Geladener Atlas als gemeinsam residente Crown-Pieces

Vor Implementierung: CrownPieces registriert jede vorhandene Atlasansicht genau
als ein zweidreieckiges natives Piece mit dessen echten Colour/Normal/MR-Bildern
über Live. Update gruppiert Modellmatrizen nach der zum Auge gerichteten Ansicht;
Richtung im Modell mit dem transformierten Kronenzentrum und Basisvektoren bestimmen.
Instanzmatrizen stammen später aus WorldPlacement im World-Renderframe; dieser
Schritt erfindet keine Platzierungen. Vorhandene Mat4-Transformationen wiederverwenden.

SubjectDraw erhält SetPieceInstances: Matrixzeilen ändern, Geometriebereiche erhalten,
leere Gruppen deaktivieren. Optionales MaxInstances beim PlacePiece begrenzt und
reserviert Matrixspeicher. CrownPieces reserviert seine Gruppen bis zum deklarierten
Maximum. Update darf weder Karten neu erzeugen noch Materialien neu registrieren.
Vorhandene Retable-/Placement-Uploads benutzen; deren gesamte Frameallokation ist
weiter 2124 und wird dadurch nicht pauschal für erledigt erklärt. Live muss den
CrownPieces-Owner überleben. Unreal/RAGE-Benchmark: ein residentes Kronenmodell pro
Ansicht, geteilte Instanzen, Blickwechsel als Instanzdelta statt Asset-Neuaufbau.

Beweis am tatsächlich gebackenen und aus dem Dateicache geladenen Birkenatlas:
mehrere Instanzen mit denselben acht residenten Dreiecken (vier Ansichten mal zwei),
Blickrichtungswechsel und leere Gruppen. Im 384x128-Orthobild je 128 Pixel breite
Krone links/rechts, mittleres Drittel frei. Coverage und Normalen an den gewählten
Atlasansichten gegen vorhandene Rohtexel prüfen. PNG selbst öffnen. Mehr Instanzen
als deklarierte Kapazität verweigern; Entleeren und Wiederbefüllen darf keine
Prototypen vervielfachen. Negativkontrolle: immer erste Ansicht wählen; Gegenblick
muss Coverage-/Normalenprüfung verletzen. Dies schließt den GPU-Handoff des echten
Atlas, noch nicht Cache-Miss-Vorbereitung oder ringweite World.Instances-Anbindung.

### Crown-Pieces: GPU-Handoff und Blickwechsel geprüft

CrownPieces übernimmt den tatsächlich geladenen Atlas über RegisterPieceSurfaces;
pro Ansicht ein residentes Piece. Vier Ansichten × zwei Dreiecke = acht residente
Dreiecke, unabhängig von den zwei gezeichneten Instanzen. Update gruppiert nach
transformiertem Kronenzentrum/Blickrichtung und übergibt nur Instanzmatrizen.
Leere Gruppen zeichnen nichts; Zerstörung gibt alle vier Pieces frei. Materialien
bleiben im bestehenden Live-Katalog. Kapazität wird vor Mutation geprüft.

Der neue GPU-Test entfernte anfangs die native Referenz mit einem leeren SubjectMesh
und setzte damit dessen Anker auf null, während Live::Stand/Aim den Anker
(kWgs84A,0,0) behielt. GPU-Readbacks bestätigten korrekte Geometrie, Indizes und
Matrizen; die View-Uniform zeigte die falsche Verschiebung um den Erdradius.
Wiederholter Frame und deaktiviertes Backface-Culling änderten den Fehler nicht.
Korrigiert wurde ausschließlich die neue Fixture-Übergabe: Referenzgeometrie leer,
Live-Anker erhalten. Coverage-/Normalenorakel unverändert. Sämtliche temporären
Readbacks, Diagnoseausgaben, Zusatzframes und DoubleSided-Overrides entfernt.

`make suite SUITE=outshine/conventions`, build/crown-pieces-restored.log:
Exit 2, 18/19 PASS, ausschließlich bestehender Schachfall 2179 rot.
CrownAtlasRetainsGeneratorSurfaces: 174 Checks, null Fehler. Vier Blickrichtungen
mit jeweils einer um 180 Grad gedrehten und einer ungedrehten Instanz: Coverage
exakt, mittleres Bilddrittel leer; maximaler Normalenfehler 0.011985 / 0.012501
gegen bestehende Grenze 4*sqrt(3)/255 (RGBA8-Quantisierung). Fall gesichert als
build/crown-pieces-restored-case.log. PieceInstancesShareTheirGeometry: 144 Checks,
null Fehler; direkte/geclusterte Pieces verweigern Überkapazität und unbekannte /
freigegebene IDs. Entleeren und Wiederbefüllen reproduziert das vollständige
Tiefenbild exakt, einschließlich unveränderter nachfolgender Piece-Platzierung.
Fall gesichert als build/crown-pieces-instances-restored-case.log.

Negativkontrolle: Auswahl absichtlich auf Ansicht null festgehalten.
build/crown-pieces-view-negative.log, Exit 2, 17/19 PASS; vier gezielte
Coverage-Fehler im Kronenfall, zusätzlich bekannter Schachfall rot. Abweichende
Pixel je Blickrichtung: 2189 / 4198 / 2189 / 4198. Fall gesichert als
build/crown-pieces-view-negative-case.log. Korrekte Quelle vor Abschlusslauf
wiederhergestellt. Keine Orakeländerung zur Herstellung eines grünen Ergebnisses.

Alle vier build/crown-atlas/pieces-{0,1,2,3}.png nach Abschluss geöffnet.
Zwei getrennte Kronen, richtige Blick-/Rotationsabhängigkeit, freier Zwischenraum.
Direkter Karten- und feiner Generator-PNG ebenfalls verglichen: dünne, punktförmige
Birkenkrone bereits im Generator. Das ist keine botanische oder fotorealistische
Abnahme; Kronendichte, artspezifische Morphologie und Mip-Coverage bleiben offen.

`make shots PLACE=Koerbersee`, build/crown-pieces-koerbersee.log: Exit 0,
Digest c99cdbe7 unverändert. 120 residente Standframes: p50 6.89, p95 7.16,
p99 7.27 ms, 0 über 16.67 ms; Peak Heap 496 MB. PNG und Webcam geöffnet:
weiterhin keine Bäume, abgerundete Felsen ohne Schichtung, flächige Klassen,
einfache Gebäudekästen und dunkles flaches Wasser. Keine neue Weltvegetation und
kein bewegter Wald-/Streaming-Nachweis aus diesem Lauf ableitbar.

Nächster wirksamer Schritt bleibt die reale World.Instances-Anbindung: vorhandenen
TangentFrame/RenderFrame für geografische Modellmatrizen verwenden, lokale
Erdkrümmung/Up-Richtung, Yaw und Scale erhalten; Arten gruppieren, geladene Kronen
residieren lassen und Blickwechsel aus der Weltkamera übergeben. Cache-Miss-
Vorbereitung und begrenzte Residency dürfen keinen ungebremsten Bake im laufenden
Frame auslösen. Der aktuelle Handoff prüft statische starre/gleichförmig skalierte
Prototypen; Wind, bewegte Körper und deren vorherige Instanztransformation sind
hierdurch nicht abgenommen. 2111 bleibt active.

## Geografische Modellmatrizen für Crown-Pieces

Vor Implementierung: WorldPlacement::ModelIn(TangentFrame) übergibt Platzierungen
in den bestehenden World-Renderframe. Vorhandene TangentFrame::Place/Turn und
RenderFrame::Of übernehmen ECEF/ENU und East-Up-South; keine zweite Geodäsie.
Lokale Basis am Standort des Körpers, nicht am Auge: X=East, Y=Up, Z=-North.
Positive Yaw dreht um lokale +Y, also +X nach -Z. Gleichförmiger Scale multipliziert
nur die drei Basisvektoren. Translation bleibt double und verwendet denselben
AslM->GeoToEcef-Höhenvertrag wie das Terrain; dies löst keinen Geoid-Datumfehler.

Unreal/RAGE-Benchmark: gemeinsame Weltmatrizen für residente Prototypinstanzen,
lokale Ursprünge und gemeinsame Geländeausrichtung statt pro Baum gebackener
Weltgeometrie. Der planetare Teil folgt der bereits vorhandenen geodätischen Basis.
Beweis im bestehenden ForestInstancesKeepTheirSpecies-Fall: geschlossene Lösungen
am Äquator (Ursprung, Vierteldrehung um die Erde), positive Yaw, Maßstab,
Submillimeter-Translation und derselbe Standort aus benachbarten Tile-Frames.
Negativkontrolle: lokale Up-Basis absichtlich durch die Weltanker-Basis ersetzen;
der Viertelkreis-Standort muss seine Aufrichtungsprüfung verlieren. Kein visueller
Fortschritt behauptet, bevor dieser Transform im World-Crown-Consumer erreicht wird.

### Geografische Modellmatrix: geprüfter Stand

WorldPlacement::ModelIn übernimmt geografische Translation in double und die
lokale East-Up-South-Basis mit positiver Y-Drehung und gleichförmigem Maßstab.
Keine neue Geodäsie und keine pro Instanz gebackene Geometrie.

`make suite SUITE=outshine/conventions`, build/world-placement-model-restored.log:
Exit 2, 18/19 PASS; ausschließlich bestehender Texturfilterfall 2179 rot.
ForestInstancesKeepTheirSpecies: 214 Checks, null Fehler (bisher 159 plus
55 Transformprüfungen). Geprüft: 16 Komponenten für benachbarte Tile-Frames,
16 für skalierte Äquatorbasis mit separat erhaltener Wurzelhöhe, 16 für
Viertelkreis-Standort mit lokaler Aufrichtung, sechs Richtungskomponenten für
positive Yaw und eine Höhendifferenz von 0.0001 m. Letztere bleibt nach ECEF-
Subtraktion innerhalb 2e-9 m erhalten. Fall gesichert unter
build/world-placement-model-restored-case.log.

Negativkontrolle: ausschließlich lokale Up-Achse durch frame.UpEcef ersetzt.
build/world-placement-model-up-negative.log, Exit 2, 18/19 PASS, genau der
Wald-Fall rot. 214 Checks, genau zwei Fehler der Viertelkreis-Up-Komponenten.
Alle anderen Fälle einschließlich Schach in diesem Lauf grün. Fall gesichert
unter build/world-placement-model-up-negative-case.log. Quelle vor obigem
Abschlusslauf wiederhergestellt; keine Änderungen am Orakel.

Die vier vom Abschlusslauf erzeugten Kronen-PNGs erneut geöffnet: unverändert
zwei dünne Birken, leerer Zwischenraum. Diese Fixture verwendet weiterhin ihre
lokalen Testmatrizen. ModelIn ist noch nicht im produktiven World-Crown-Consumer;
der Schritt behauptet weder neue Place-Pixel noch Wald-Framezeiten. Der letzte
geöffnete Koerbersee-/Webcam-Vergleich bleibt maßgeblich.

Für die unmittelbar folgende Cache-/Welt-Anbindung: Shipping hält aktuell nur
geparste TreeSpecies; CrownAtlas::ProvenanceFor erwartet den Profilquelltext.
Die Zuordnung Cluster -> geparste Spezies -> gültiger Artefaktschlüssel muss aus
der vorhandenen Katalogladeoperation kommen, nicht aus einer zweiten unabhängig
sortierten Verzeichnisabfrage. Parse verwendet teilweise bestehende Parameter
als Defaults; ein künftiger Quelltextschlüssel darf deshalb keinen anderen
geparsten Zustand repräsentieren. Beides gehört zur noch offenen Katalogübergabe
in 2111. Modellmatrizen bei Platzierungsänderung vorbereiten, nicht pro Kamera-
Frame erneut Geodäsie für jeden Baum rechnen. 2111 bleibt active.

### Definition und Generatorzustand gemeinsam an den Cache übergeben

Vor Implementierung: 2183 schließt die beim Cache-Handoff erkannte Parse-Lücke.
TreeSpecies hält nach erfolgreichem Parse seine eigene Definition; Shipping trägt
sie bereits pro Cluster. CrownAtlas::ProvenanceFor erhält im realen Atlasfall diese
Definition statt separat gehaltenen Testtext. Katalogtest prüft Definition und
Spezies zusammen. Dies macht die Katalog-/Artefaktzuordnung erreichbar; asynchroner
Welt-Consumer und vorbereitete Cache-Misses bleiben danach erforderlich.
