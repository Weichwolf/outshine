# Nördliche gemeinsame Geh-/Radbrücken (#517)

Die [feste Eingabe](fixtures/stair-north-bridges-vienna.json) erweitert den
[vorigen 28-Quellen-Verbund](stair_west_bridges_comparison.md) um vier ganze
ursprüngliche Teile desselben PBF: 138663000:10 (131,883 m), 1574593390:2
(46,0 m), 273682380:0 (433,2 m) und 852893530:0 (215,4 m), zusammen rund 826 m.
Zwei Quellen sind echte Brücken; Quelle/Tags/IDs und bisherige Achsen/Priors
bleiben erhalten. Neue Achsen sind vollständige Originale. Keine gemessenen
Deckhöhen oder vollständige OSM-/native Rekonstruktion behauptet.

## Funktionales Profil und wirklicher Körper

Die beiden als `path` gelieferten Teile haben ausdrücklich `bicycle=designated`
und `foot=designated`. Nur eine zusätzlich gewählte gemeinsame Geh-/Radrolle
verwendet deshalb das bestehende Radprofil: 2 m Breite/6 % maximale Neigung.
Der generische Pfad mit 1,5 m/20 % ist kein gleichwertiger funktionaler Vergleich.
Unmarkierte Pfade, fehlende/falsche Bezeichnungen, Indoor/Tunnel oder unpassende
Ebenen werden damit nicht zugelassen. Originale werden nicht umetikettiert.
Gewählte echte gemeinsame Enden erhalten ganze Profilhüllen; alte Kontakte und
getrennte Brückenebenen bleiben bestehen. Bloße Nähe erzeugt keine Verbindung.

Die ursprüngliche 10-nm-Erstpunkt-Kanonisierung erzeugt am neuen Brückenkopf
von 273682380:0 eine vierfach belegte vertikale Körperkante. Zwei Randpunkte liegen
13,3 nm auseinander; eine zentrierte Variante mit weiterhin höchstens 10 nm
Verschiebung behebt diesen ersten Fehler, aber nicht den ganzen Fall. Ein zweiter
Punktspalt von 24,7 nm lässt sich innerhalb dieser Verschiebung nicht vollständig
kanonisieren. Beide Varianten bleiben ganze Gegenbeispiele. Kein defekter Körper
wird durch einen positiven Solverstatus zugelassen.

Der neue Fall wählt ausdrücklich eine interne Konstruktionsschwelle von 100 nm.
Der erste Eingangspunkt bleibt kanonisch; tatsächliche Verschiebung höchstens
24,745 nm im Körperversuch. Die bisherigen tatsächlichen Quellflächen-, Profil-,
Kontakt-, Klassen-/Neigungs- und Float32-Gates bleiben unverändert. Die interne
Schwelle wird nicht als unverändert ausgegeben. Alte 10-nm-Defaults bleiben.
Der installierbare [numerische Kern](../../../lib/welding.md) zählt Radiuspaare
vor deren Arrayallokation, begrenzt Vertices/Paare und weist zu lange transitive
Ketten ab. Er allein garantiert weder Topologie noch gültige 3D-Exporte.

## Begrenzte Pfeilerwahl

142 nominale 12-m-Stationen überschreiten den bisherigen 128-Stationen-Pool.
Der alte Default bleibt ablehnend. Die neue ausdrücklich gewählte verteilte
Auswahl behält erste/letzte Station jedes Brückenteils und verteilt die übrigen
Plätze über die verbleibenden Kandidaten. Höchstens 128 physische Stationen,
640 Versuche und acht Sekunden je Variante. Jede nicht gewählte Station behält
Quell-ID, Nominale und Grund `support_station_budget`. Keine pauschale globale
Optimalität oder strukturelle Tragfähigkeitsberechnung behauptet.

## Quellen und Reproduktion

Zuerst lokale SIGGRAPH-Quellen: [Chen et al. 2008, S. 2/Fig. 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
Graph und Geometrie getrennt; [Hoppe et al. 1993, S. 3–4/Abschnitt 4](../../../doc/references/transport/siggraph/1993-mesh-optimization.pdf),
Konnektivität gegenüber geometrischer Einbettung und Prüfung legaler Änderungen.
Hoppe S. 3 und 4 einschließlich Pseudocode/Gleichungen optisch geprüft. Unsere
numerische Kanonisierung ist keine Implementierung ihres Optimierungsverfahrens.
Die tatsächliche Körper-/Kontaktprüfung bleibt eine unabhängige Pflicht.
[SciPy count_neighbors](https://docs.scipy.org/doc/scipy/reference/generated/scipy.spatial.cKDTree.count_neighbors.html)
zählt Radiuspaare; [query_pairs](https://docs.scipy.org/doc/scipy/reference/generated/scipy.spatial.cKDTree.query_pairs.html)
liefert ungeordnete Indexpaare ohne Diagonale. Beim Selbstvergleich wird die
Diagonale vom Zähler abgezogen und durch zwei geteilt. Primärdokumentation
2026-10-16 geprüft; lokal verwendete Version steht in den Messbelegen.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest test_welding stair_north_bridge_checks -q
python test/experiments/transport/stair_cycle_experiment.py \
  --fixture test/experiments/transport/fixtures/stair-north-bridges-vienna.json \
  --base-network /tmp/outshine-transport-west-bridges-full-14b0a8f75 \
  --base-terrain /tmp/outshine-transport-west-bridges-full-14b0a8f75 \
  --output /tmp/outshine-transport-north-bridges-<commit>
```

Danach derselbe Aufruf mit `stair_cycle_verify.py`; Pfeiler mit
`stair_support_experiment.py`/`stair_support_verify.py`, derselben Fixture und
den neuen Produkten. Frische volle Gelände-/Pfeiler-/Bild-/Kosten-/Wheelbelege
stehen zum Zeitpunkt des ersten Quellencommits noch aus; kein vollständiger
neuer Geländeverbund allein aus den Körperversuchen behauptet.
