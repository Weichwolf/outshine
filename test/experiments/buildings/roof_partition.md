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
Kontaktknoten werden aus den beteiligten Ebenen bestimmt. Eine Quellkante kann
mehrere Frontfragmente besitzen; Herkunfts-ID ist keine Fragmentidentität.

GEOS nodet die Kontur; bekannte Kontaktpunkte teilen die Abschnitte ausdrücklich.
Winding klassifiziert Material und Höfe. Bei Punktkontakten verbindet eine
bijektive gerichtete Kantenfolge die Front neu: die nächste ausgehende Kante liegt
im Uhrzeigersinn vom rückwärts gerichteten Eingang. Beim Kontakt Hof↔Außenrand
entsteht dadurch eine vorübergehend schwach einfache Kontur mit zwei Ecken am
gleichen Ort, deren verschiedene Nachbarschaften anschließend auseinanderlaufen.
Getrennte GEOS-Ringe einfach weiterzubewegen würde reale Dachfläche verlieren.
Eine Flächenbilanz verhindert deren stilles Verschwinden.

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

Geprüfter Linux/i5-1135G7-Lauf unter interaktiver Last: neuer Pfad14,70s für48
Aufbauten, maximal1,16s; reiner Python-Backendpfad2,38s, alter Auto-Pfad1,77s.
Letzterer enthält kompiliertes SFCGAL und Python-Rückfälle. Der neue Python-Pfad
ist hier langsamer; dies ist kein nativer Gleichsprachvergleich oder Speedup.
Maximal95 Frontvertices,308570 Kollisionspaare und4722 Stücke je Quelle.
Die drei benannten Kollisionsmatrizen erzeugen über alle48 Fälle mindestens
56880096B Arraypayload; das sind weder gesamter Traffic noch gleichzeitig residente Bytes.
Gespeicherte geometrische Diagnosearrays aller48 fertigen Pläne:197552B.

Ein separater Wiederholungslauf mit `tracemalloc` am95-Traufen-Fall misst867783B
Python-seitig erfassten Peak/1848 verbleibende Blöcke. Nur diese Allokationsdiagnose
verwendet15s wegen ihrer Instrumentierung; die48 Qualitätsläufe behalten2s.
Weld-Belege enthalten tatsächliche explizite Kopier-/Ausgabepayloads.
Native/GEOS-Allokationen, vollständige Kopien und native Assetbytes sind unbekannt.
Der gesamte Vergleichsprozess meldet112984064B RSS vor der privaten Packausgabe einschließlich Bibliotheken,
Originalfixture, Vergleichsbackends und Checks; kein App-/Gerätebudget daraus.

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
PYTHONPATH=lib/src:test/experiments/buildings .venv/bin/python -B test/experiments/buildings/plot_roof_partition.py --output /tmp/outshine-roof-partition-proof
```

Der Vergleich blockiert beide optionalen Skeleton-Imports im Kandidaten,
prüft gegen vorhandene Pins und schreibt keine Orakel. Quellen-/Codehashes,
Zeiten, Arbeit und unbekannte Kosten stehen im Report. 13 lokale Kontrollen prüfen auch den vorhandenen Desktop-Worker und die direkte
Verwendung fertiger Pläne mit UV0–9 ohne erneute Konstruktion. Die lokale3D-Diagnose
vergleicht vier Originaldächer bei gleichem30° Winkel/physikalischem Achsenmaß;
sie muss geöffnet werden und ist kein PBR-/Weltbild. Keine Bildveröffentlichung.

SIGGRAPH zuerst: Müller et al.2006, *Procedural Modeling of Buildings*, lokales
Autoren-PDF S4/Fig5–8 optisch geprüft: allgemeine Grundrisse/Straight-Skeleton-Dächer
und nachfolgende Flächenregeln. Held/Palfrader2016, [gewichtete Skeleton-Dächer](https://arxiv.org/abs/1604.03362),
lokales PDF S4–5/Fig2 optisch geprüft: selbstparallele Fronten, Kollaps-/Split-
Ereignisse und metrisches Dach. Beide sind Verfahrenkontext, kein Beweis unserer
Floating-point-Implementierung oder Gerätebudgets. Kein Fremdcode übernommen.
