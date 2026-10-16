# Westlicher Radbrückenast und Brücke über Brücke (#513)

Die [feste Eingabe](fixtures/stair-west-bridges-vienna.json) ergänzt den bisherigen
[24-Quellen-Stand](stair_landing_comparison.md) um vier ganze ursprüngliche Teile:
`4949918742:0` (62,301 m), `258321810:4` (95,89 m), `1047312722:0` (57,01 m)
als echte Radbrücken und `39881120:25` (10,115 m) als Bodenanschluss. Gelieferte
Brückentags/Layer bleiben erhalten. Bisherige geplante Achsen/Priors, ursprüngliche
Quellen/Attribute/IDs bleiben unverändert; neue Achsen sind die ganzen Originale.
Bereits vorher deklarierte Umwegknoten gehören weiterhin zum geplanten Achsraum.

## Zwei Decks und derselbe gewählte Anschluss

Die Rampen `258321810:4` und `1047312722:0` besitzen einen echten gemeinsamen
Endknoten und schneiden sich außerdem mehrfach projiziert. Ihr positiver
Bandschnitt hat zwei Komponenten. Ein Layer-Hinweis ist keine gemessene Deckhöhe.
Die gewählte Interpretation hält die eine Brücke über der anderen und prüft die
ganze Durchgangsfläche. Die gemeinsame Verbindung am tatsächlichen Endknoten
bleibt erhalten; beide Quellen bleiben Brücken. Ein falsches Ground-Label wird
weder zur Solverordnung noch zur physischen Prüfung verwendet.

Eine gemeinsame Plattform wurde als alternative Hypothese aufgebaut. Sie erfüllt
die lokalen Gradienten/Körperbedingungen, verliert aber die verlangte ganze
getrennte Überführung. Die unabhängige Prüfung gegen das gewählte Ebenenrezept
lehnt sie ausdrücklich als verlorene/umetikettierte Überführung ab. Unter diesem
Vertrag ist sie kein gültiger Kostenkonkurrent. Die Daten beweisen allein weder
die ursprünglichen gemessenen Höhen noch eine vollständige OSM-Rekonstruktion.

`stair_branch_inputs` ergänzt höchstens acht ausdrücklich deklarierte äußere
Radbrücken-/Radweginstanzen bei höchstens 32 ganzen Quellen. Die bisherige
Ground-only-Auswahl mit höchstens sechs Zusatzquellen bleibt unverändert.
Indoor/Tunnel, falsche Bridge-/Ground-Rollen, Duplikate und unbekannte Quellen
bleiben abgelehnt. `stair_crossing_order` trägt die ausdrücklich gewählte
Brücke-über-Brücke-Ordnung in abgeleiteten Parametern; Originale bleiben erhalten.
Widersprüchliche/ungültige Deklarationen werden vor jeder Mutation zurückgewiesen.
Ohne gewählte Ordnung bleiben projizierte Brücke-/Brücke-Konflikte ungeplant.

## Flächenarbeit im unveränderten Budget

Die dichte Variante desselben ganzen neuen Astes erzeugt 813 Querschnitte und
überschreitet das unveränderte Arrangement-Segmentbudget 8192. Ein neues optionales
Verfahren verwendet bei Nichttreppen nur die beibehaltenen geplanten Achsknoten.
Original- und bereits deklarierte Umwegknoten bleiben erhalten; die eigentlichen
Quellflächen werden nicht verkürzt oder vereinfacht. Treppen behalten exakt ihre
vorherige feine Querschnittszerlegung. Ganze Kontaktmasken, Quellenränder und
Kreuzungen bleiben zusätzliche Unterteilungsgrenzen.

250 Querschnitte/3541 genodelte Segmente tragen den ganzen 28-Quellen-Körper.
Die LP hat 3868 Variablen/81515 Zeilen/241425 Nichtnullwerte. Alle 49 inneren
Endprofile und die bisherigen zwölf plus vier neue ausdrücklich geschützte
Paarprofile bestehen in Double und tatsächlichem Float32. Die bisherigen zehn
Überführungsflächen und die neue ganze Brücke-über-Brücke-Fläche bestehen;
sieben projizierte Achskreuzungen gehören zu dieser neuen Fläche. Kleinster
tatsächlicher Float32-Durchgang im Körperversuch 2,519989 m. Drei Treppen/24
Stufen bleiben erhalten. Das ist keine durchgehende C1-/Geh-/Fahrhüllenabnahme.

Die Diskretisierung ändert die endliche L1-Stichprobenzielgröße und damit die
gemeinsam neu geplanten Höhen. Gleiche Qualitätsanforderung meint ganze Flächen,
Normen/Kontakte und Durchgänge, keine identische Höhenlösung oder globales Optimum.
Der bisherige dichte Default bleibt unverändert; alte Referenzen werden nicht
auf diese neue Variante umgepinnt. Der unveränderte 24-Quellen-Vergleich mit
[Knotenunterteilung](fixtures/stair-landings-source-knots-vienna.json) trennt
Diskretisierungsarbeit vom neuen Inhalt.

Zuerst lokale SIGGRAPH-Primärquelle Chen et al. 2008, S. 2/Fig. 2,
[Graph/Geometrie-Trennung](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf).
Ergänzend Galin et al. 2010,
[S. 8/Fig. 17–18](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
auch optisch geprüft: Profile und terrainrelative Brücken-/Tunnelgeometrie.
Unsere gewählte Ebenenordnung und optionale endliche Diskretisierung sind eigene
kontrollierte Verfahren; keine vollständige dort belegte Kreuzungslösung oder
Übernahme fremden Codes.

## Reproduktion

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest stair_west_bridge_checks stair_landing_checks \
  test_junction_pads stair_corner_checks stair_corridor_checks stair_cycle_checks \
  stair_cycle_north_checks stair_cycle_south_checks stair_terrain_cells_checks -q
python test/experiments/transport/stair_cycle_experiment.py \
  --fixture test/experiments/transport/fixtures/stair-west-bridges-vienna.json \
  --base-network /tmp/outshine-transport-landings-full-fc98f1923 \
  --base-terrain /tmp/outshine-transport-landings-full-fc98f1923 \
  --output /tmp/outshine-transport-west-bridges-<commit>
```

Danach derselbe Aufruf mit `stair_cycle_verify.py`; Pfeiler über
`stair_support_experiment.py`/`stair_support_verify.py` mit derselben Fixture und
den neuen Produkten. Kostenvergleiche verwenden `stair_landing_cost.py` auf
dem gleichen 24-Quellen-Eingang mit beiden Unterteilungen und dem neuen ganzen
28-Quellen-Eingang; tatsächliche ganze Qualität ist jeweils Pflicht.
Native Integration, Statik, weitere Ebenen/Quellen und Geh-/Fahrhüllen bleiben offen.
