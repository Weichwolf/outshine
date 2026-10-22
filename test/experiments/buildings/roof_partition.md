# Begrenzte Walmdach-Partition ohne SFCGAL (#955)

`prepare_hip(polygon, method='contour')` liefert den bestehenden unveränderlichen
`HipPlan` mit Originaltraufe, Facetten, gemeinsamen Knoten und metrischen
Wavefront-Distanzen. NumPy/SciPy/Shapely aus den Basisabhängigkeiten genügen.
Der neue Pfad ruft weder SFCGAL noch `py_straight_skeleton` auf. Der bestehende
Auto-Pfad bleibt der Vergleich; die native Übernahme besitzt Master.

## Verfahren und Schutz der Geometrie

Jede orientierte Quellkante besitzt ihre ursprüngliche einwärts gerichtete
Einheitsnormale `n` und `c=-n·a`. Ihre Front erfüllt `n·x+c=t`.
Ecken zwischen zwei Fronten bewegen sich bis zum nächsten Ereignis affin.
Kantenkollaps wird ausdrücklich über die gerichtete Kantenlänge bestimmt;
Ecke-Kante-Kontakte prüfen den tatsächlichen bewegten Kantenabschnitt.
Kontaktknoten stammen aus der affinen Bewegung zum berechneten Ereigniszeitpunkt.
Die Winkelhalbierende `v=2(n_links+n_rechts)/|n_links+n_rechts|²` vermeidet
Differenzen fast gleicher Normalen. Ein erneutes absolutes3×3-Lösen von `(x,y,t)`
kann bei fast parallelen Ebenen einen gültigen Kontakt stark verschieben und
entfällt. Eine Quellkante kann
mehrere Frontfragmente besitzen; Herkunfts-ID ist keine Fragmentidentität.

GEOS nodet die Kontur; bekannte Kontaktpunkte teilen die Abschnitte ausdrücklich.
Winding klassifiziert Material und Höfe. Bei Punktkontakten verbindet eine
bijektive gerichtete Kantenfolge die Front neu: die nächste ausgehende Kante liegt
im Uhrzeigersinn vom rückwärts gerichteten Eingang. Beim Kontakt Hof↔Außenrand
entsteht dadurch eine vorübergehend schwach einfache Kontur mit zwei Ecken am
gleichen Ort, deren verschiedene Nachbarschaften anschließend auseinanderlaufen.
Getrennte GEOS-Ringe einfach weiterzubewegen würde reale Dachfläche verlieren.
Eine Flächenbilanz verhindert deren stilles Verschwinden.
Die Herkunftszuordnung prüft nach dem Mittelpunktfilter beide Endpunkte gegen
den tatsächlichen Quellabschnitt und dessen Richtung. Das trennt entgegengesetzte
Fronten beim Erlöschen und nahe kurze Reststücke. Endpunktprüfungen laufen nur
auf den vorgefilterten Paaren, nicht auf einer zweiten vollen Abstandsmatrix.

Jede Traufe sammelt ihre überstrichenen Trapeze direkt in `(Kantenstation,t)`.
Die gemeinsame Zeitkoordinate verhindert numerische Spalten zwischen benachbarten
Schritten. GEOS vereinigt nur Stücke derselben Quelle; ursprüngliche kollineare
Traufen bleiben getrennte Eigentümer. Die fertigen Facetten werden konform vernäht.
Die bestehenden Prüfungen verlangen vollständige disjunkte Deckung, Quellkante
genau einmal, positive Höhe, Ebenenkonsistenz und geschlossene Firste.

`geometry.welding.weld_vertices` wird wiederverwendet: Kandidaten vor ihrer
Allokation zählen, begrenzte Paare, keine lange Kette über der Verschiebungsgrenze.
Originalvertices bleiben exakt fixiert. Für zusammengehörige Schnittstellen gelten
die bereits vorhandenen maximal50µm Korrektur; nur kompatible inzidente Ebenen
werden verbunden. Die kombinierte Korrekturschranke wird erneut geprüft.
Der geprüfte Korpus benötigt maximal9,4nm. Keine neue Weld-/LOD-Welttoleranz und
kein räumliches Pflichtraster. Runde Nullflächen werden nur innerhalb eines aus
Double-Auflösung und Konturlänge abgeleiteten Flächenwächters ausgeschieden;
die unabhängige Endprüfung und sämtliche alten Pins bleiben unverändert.
Verfeinerung löst die Korrektur zum vorhandenen Punkt statt absoluter Koordinaten.
Bereits innerhalb des Double-Rundungswächters konsistente Punkte bleiben erhalten;
der gemeinsame bisherige Skeleton-Abschluss benutzt denselben Ansatz. Rang,
Ebenenresiduen und50µm-Verschiebungsgrenze werden weiterhin geprüft.

## Grenzen und Vergleich

Bestehende Limits: 256 Eingabevertices,1024 Ausgabeknoten,4096 kooperative Schritte,
2s einschließlich Abschlussprüfung. Zusätzlich höchstens2M Kollisionspaare,
16384 überstrichene Stücke und begrenzte Noding-/Weld-Kandidaten. Unzureichende
Konturfreiheit unter1µm folgt dem bestehenden nativen Gegenfall. Ungültige,
mehrdeutige oder erschöpfte Fälle liefern einen Grund, keinen unvollständigen Plan.
GEOS-Aufrufe bleiben kooperativ geprüft; ein abbrechbarer Asset-Worker gehört vor
den Frame. Rechtecke verwenden die vorhandene analytische Spezialisierung.

Alle48 unveränderten Offline-Pläne bestehen einschließlich8 Hof-Grundrissen,
des45-Traufen-Falls Vienna:1210000:12 und des nativen57-Traufen-Gegenfalls
Vienna:1137690:3. Der bisherige reine Python-Backendpfad besteht47/48,
der alte native-first Auto-Pfad mit Rückfall48/48. Andere Verfahren/Globalrobustheit
folgen daraus nicht. Ein GEOS-Miter-Inset ersetzt den Ablauf nicht: auf7 gleichen
Höhenschnitten weichen14/48 Fälle um mehr als1e-7m² ab, maximal14,6834m².
Das ist ein Gegenbeispiel, keine Zulassung der übrigen34 Fälle für alle Höhen.

Früherer Stand5826ddda0, Linux/i5-1135G7 unter interaktiver Last: eigener Pfad14,70s für48
Aufbauten, maximal1,16s; reiner Python-Backendpfad2,38s, alter Auto-Pfad1,77s.
Letzterer enthält kompiliertes SFCGAL und Python-Rückfälle. Der neue Python-Pfad
ist hier langsamer; dies ist kein nativer Gleichsprachvergleich oder Speedup.
Maximal95 Frontvertices,308570 Kollisionspaare und4722 Stücke je Quelle.
Die drei benannten Kollisionsmatrizen erzeugen über alle48 Fälle mindestens
56880096B Arraypayload; das sind weder gesamter Traffic noch gleichzeitig residente Bytes.
Nach der Robustheitskorrektur:48/48 Originale,23,56s gesamt/maximal1,95s unter
zusätzlicher CPU-Last; Vergleichsbackends2,65/1,82s. Kein isolierter Kostenvergleich.
Gespeicherte geometrische Diagnosearrays aller48 fertigen Pläne:197584B.

Ein früherer Wiederholungslauf an5826ddda0 mit `tracemalloc` am95-Traufen-Fall misst867783B
Python-seitig erfassten Peak/1848 verbleibende Blöcke. Nur diese Allokationsdiagnose
verwendet15s wegen ihrer Instrumentierung; die48 Qualitätsläufe behalten2s.
Weld-Belege enthalten tatsächliche explizite Kopier-/Ausgabepayloads.
Native/GEOS-Allokationen, vollständige Kopien und native Assetbytes sind unbekannt.
Der gesamte Vergleichsprozess meldet112984064B RSS vor der privaten Packausgabe einschließlich Bibliotheken,
Originalfixture, Vergleichsbackends und Checks; kein App-/Gerätebudget daraus.
Ein neuer vollständiger Speicherpeak nach der Robustheitskorrektur ist ungemessen.

## Koordinatengegenfälle

37° Drehung und Translation um `(100000,-200000)m` ließen am früheren Stand
vier Aufbaufehler und eine abweichende Quellflächen-Trennlinie entstehen.
Die obigen Änderungen beheben die vier Aufbaufehler, ohne Pins oder Grenzen
zu ändern. Alle17 lokalen Kontrollen bestehen einschließlich dieser fünf Fälle
und einer analytischen fast rangdefizienten Ebenenkonfiguration.

`check_roof_invariance.py` transformiert die48 festen Eingaben und ihre bestehenden
Referenzen nur im Arbeitsspeicher: Translation, Rotation und beides,144 Fälle.
Es prüft die komplette stückweise lineare Höhenfläche: überlagere alte/neue
Facetten und vergleiche ihre linearen Höhen an sämtlichen Schnittvertices.
Das Maximum einer affinen Differenz liegt dort; dies ist keine Punktstichprobe.
Deckung und beide Höhenmodi bleiben separat geprüft. Die unveränderten48
Originalpins behalten zusätzlich ihre strenge Quellflächenprüfung.

Unter gleichzeitiger CPU-Last bestehen143/144 Varianten im bestehenden2s-Limit;
der erste verschobene95-Traufen-Fall wird nach2,33s begründet abgelehnt. Ein separater
profilerinstrumentierter Geometrielauf mit explizit15s besteht dessen strengen
Pin und Höhenfläche, gilt aber nicht als2s-Zulassung. Von3,45s instrumentierter
Zeit entfallen1,14s auf erstmalige Imports. Kalte Prozesse weiterhin mitmessen.
Maximale Höhenabweichung der143 zugelassenen Varianten:0,76nm.

142 Varianten stimmen auch in der strengen Quellflächenzuordnung überein.
Bei `Tokyo:1367300462:0` mit Rotation+Translation verschiebt Rundung die Trennlinie
zweier koplanarer Quellflächen um mehrere Meter;10,79m² wechseln deren Zuordnung.
Die vollständige Höhenfläche weicht nur0,11nm ab, jede Originaltraufe bleibt erhalten.
Geometriegleichheit beweist hier keine stabile interne Teilung oder native UV-/
Textur-Seam-Abnahme. Dieser Unterschied bleibt ausdrücklich sichtbar im Report.
Die vier korrigierten Dächer wurden paarweise als3D-Diagnose geöffnet; keine
PBR-/Geräteabnahme und keine Bilder im Repository.

## Verbrauch vorbereiteter Game-Assets

`verify_roof_partition_consumer.py` prüft alle48 Originale in den tatsächlichen
intrinsischen Quellframes von `Shape`: ein vorbereiteter Konturplan, danach
`byHeight=3m` und `byAngle=30°` ohne erneute Konstruktion. Skeleton-Imports sind
gesperrt; ein Aufruf von `roof_hip.prepare_hip` schlägt ausdrücklich fehl.
96 Standarddächer ohne Überstand bestehen: orientiert geschlossene Dreiecksnetze,
aufgelöste Normalen, Volumen gegen das analytische Facettenintegral sowie
UV0–9 gegen die ursprünglichen nativen Interpolationsdreiecke.1.761.760 UV-Werte
an Ecken/Innenpunkten und Quellmaterial/Gruppen/Tags/Appearance geprüft.
Ein Linux/i5-Lauf benötigt20,06s Planaufbau und12,58s reinen Shape-Verbrauch,
maximal0,322s je Shape; Prüfungen separat. Kein nativer Zeit-/Speicherbeleg.
Even-/Inset-/Außenüberstände sind damit nicht zusätzlich zugelassen.

Eine unabhängige koplanare Kontrolle benutzt ein10×6m-Rechteck mit geteilter
Nordtraufe. Der innere Teilungsknoten wandert von `x=5` zu `x=6,5m` bei `y=3m`; jeweils2,25m²
wechseln ihre Quellfacette. Beide analytisch gültigen Pläne behalten alle Traufen
und dieselbe Höhenfläche; das Volumenintegral ist72m³ bei3m Höhe bzw.
`72*tan(30°)m³`. Beide Teilungen erhalten die absichtlich nicht global affinen
UV0–9 der gleichen Quelltriangulierung. Diese Python-Abnahme begründet den
nativen Vertrag: UVs stammen aus Quellinterpolationsdomänen, nicht aus der
zufälligen internen Quellkanten-Teilung. Sie ersetzt keine native Texturprüfung.
Die vorhandene Blender4.3.2-Workbench-Prüfung zeigt beide ausgegebenen Netze mit
derselben asymmetrischen Kontrolltextur bei1920×1080; tatsächliches UV-Binding
geprüft und Bild geöffnet. Flat-Texturdiagnose, kein PBR-/Gerätenachweis.

## Gemeinsamer Kern und CPU/GPU-Grenze

Wiederverwendet werden GEOS-Noding/Flächen und die vorhandene begrenzte numerische
Vertexkanonisierung. Zweiter realer Rezeptverbraucher derselben Welding-Funktion:
`transport/stair_module_soil.py` und `transport/stair_junction_layout.py`.
Traufen-/Frontregeln bleiben beim Dach. Weitere gemeinsame Operationen erst mit
einem zweiten tatsächlichen Verbraucher herauslösen, kein SFCGAL-Nachbau.

Topologische Ereignisse, robuste Quellzuordnung, Vereinigung und konforme
Triangulierung bleiben begrenzte CPU-Aufbereitung. Der fertige Plan liefert feste
Indizes und gemeinsame metrische Vertices: `z=distance*factor` und anschließende
Rahmenabbildung sind datenparallel. `byHeight` verwendet einen gemeinsamen
Maximalwert, `byAngle` einen Skalar; gemeinsame Firstvertices nur einmal auswerten.
Vorbereitete Pläne wiederverwenden statt die Topologie für Parameteränderungen
oder Cachehits neu zu lösen. Kleine GPU-Dispatches pro Konturereignis sind kein
gemessener Vorteil. Keine neue GPU-ABI, kein Produktcacheformat; native Shader,
Lifetimes, Metal/Vulkan-Zeiten und SFCGAL-Entfernung folgen der Integration.

## Reproduktion und Quellen

```sh
PYTHONPATH=lib/src:test/experiments/buildings OPENBLAS_NUM_THREADS=1 .venv/bin/python -B -W error -m unittest test_roof_partition
PYTHONPATH=lib/src:test/experiments/buildings OPENBLAS_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/buildings/verify_hip_fixture.py --method contour
PYTHONPATH=lib/src:test/experiments/buildings OPENBLAS_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/buildings/compare_roof_partition.py --output /tmp/outshine-roof-partition-proof --trace-allocations --check-offset-alternative
PYTHONPATH=lib/src:test/experiments/buildings OPENBLAS_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/buildings/check_roof_invariance.py --output /tmp/outshine-roof-partition-proof
PYTHONPATH=lib/src:test/experiments/buildings OPENBLAS_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/buildings/verify_roof_partition_consumer.py --output /tmp/outshine-roof-partition-proof --uv-inspection
PYTHONPATH=lib/src:test/experiments/buildings .venv/bin/python -B test/experiments/buildings/plot_roof_partition.py --output /tmp/outshine-roof-partition-proof
```

`--uv-inspection` erzeugt auch Mesh/Textur/Scene für den bestehenden Prüfrenderer.
Blender-Python muss ein kompatibles NumPy sehen; hier haben Blender und`.venv`
jeweils Python3.13.5:

```sh
PYTHONPATH="$(.venv/bin/python -c 'import sysconfig;print(sysconfig.get_path("purelib"))')" OPENBLAS_NUM_THREADS=1 nice -n 10 blender --background --factory-startup --python-use-system-env --threads 2 --python-exit-code 1 --python test/experiments/buildings/render_uv_inspection.py -- /tmp/outshine-roof-partition-proof/scene.json
```

Der Vergleich blockiert beide optionalen Skeleton-Imports im Kandidaten,
prüft gegen vorhandene Pins und schreibt keine Orakel. Quellen-/Codehashes,
Zeiten, Arbeit und unbekannte Kosten stehen im Report. 17 lokale Kontrollen prüfen auch den vorhandenen Desktop-Worker und die direkte
Verwendung fertiger Pläne mit UV0–9 ohne erneute Konstruktion. Die lokale3D-Diagnose
vergleicht vier Originaldächer bei gleichem30° Winkel/physikalischem Achsenmaß;
sie muss geöffnet werden und ist kein PBR-/Weltbild. Keine Bildveröffentlichung.

SIGGRAPH zuerst: Müller et al.2006, *Procedural Modeling of Buildings*, lokales
Autoren-PDF S4/Fig5–8 optisch geprüft: allgemeine Grundrisse/Straight-Skeleton-Dächer
und nachfolgende Flächenregeln. Held/Palfrader2016, [gewichtete Skeleton-Dächer](https://arxiv.org/abs/1604.03362),
lokales PDF S4–5/Fig2 optisch geprüft: selbstparallele Fronten, Kollaps-/Split-
Ereignisse und metrisches Dach. Beide sind Verfahrenkontext, kein Beweis unserer
Floating-point-Implementierung oder Gerätebudgets. Kein Fremdcode übernommen.
