Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, placement, graph

# Distance patterns generate stable parent locations

## Ergebnis und Besitzer
Längenabhängige Blattfolgen und Astquirle direkt in den Generatorgraphen integrieren.
2348 besitzt SpeedTree-Analyse, 2483 Graphprodukte, 2350 Profile, 2482 Frondflächen.
Feste Zählungen allein bilden unterschiedlich lange Eltern nicht plausibel ab.

## Verfahren und Referenz
[SIGGRAPH-Kurs 2003](../doc/references/vegetation/siggraph/2003-l-systems-and-beyond.pdf):
Elternprofile und Divergenzwinkel bestimmen Organfolgen; PDF-Seite 48 samt Quirlmodell optisch geprüft.
[SpeedTree Interval](https://docs8.speedtree.com/modeler/doku.php?id=genmode_interval):
Gruppen vom letzten Standort, Häufigkeit und Länge bestimmen weitere Standorte.
Ghidra-Lauf 20: Phyllotaxy-/Interval-CreateProperties erfolgreich dekompiliert (zwei Funktionen).
ELF-Strings binden Phyllotaxy-Felder an Style/Children per node/Align/Internode length/Spread/Spiral/Limit;
Interval-Felder an Frequency/Count/Sweep/Spread/Spiral/Limit/Align/Balance.
FillLocationVector bestätigt positive Schritte, Rückwärtsfolge, profilspezifische Schrittweiten und
Gruppenlimit; identische Bereichsgrenzen erzeugen keine Standorte. Native Zufalls-/Stylebindung offen.
Physische positive Schrittprofile; vom letzten Standort rückwärts bis zur unteren Grenze.
Intervallgruppen haben getrennte Mitglieder, Gruppenrotation und begrenzte Streuung.
Konservative Kapazitäten vor teurer Arbeit; exakte angeforderte Standorte bleiben im Produkt.
Gleiche Hierarchiepfade/Rezeptparameter ergeben unabhängig von LOD dieselben Standorte.
Keine vollständige SpeedTree-Property-/Zufallsbindung oder Bifurcation-Gleichwertigkeit behaupten.

## Umsetzung und geprüfter Iststand
`placement_patterns.py`: eigene physische Schrittprofile, Quirlgröße, Rotation, Streuung und Gruppenlimit.
Phyllotaxy/Interval teilen den Rekurrenzkern; eigene Parameter in Metern/Radiant, keine native Feldkopie.
Konstante Schritte direkt berechnen; Profile als Batch-Rekurrenz, Standortarrays kompakt nach Eltern packen.
`generator_specs.py`/`generator_graph.py`: Patternrezepte, echte variable Instanzen und konservative
`capacities` getrennt. Ungültige Einzelpfade übernehmen weder Posen noch Standorttabellen.
Vollständige Anfragen markieren notwendige Eltern abgeschlossen; warme Anfragen brauchen kein Restbudget.
Standorttabellencache auf 100.000 Zeilen begrenzt, weitere Einzeltabellen nur temporär auswerten.
`graph_products.py`: Kapazität vor Placement/Meshing; komplette exakte Instanzen in beiden Detailprodukten.
180 unabhängige skalare Fälle: maximal 4,44e−16 Parameter-/Winkelfehler; Streuung bleibt elternstabil.
65.536 Eltern, nur einer lang: 4.096 Standorte; größter Standort-Zwischenpuffer 4.096 Werte.
Variable Graphprobe: 64 Posen Kapazität, tatsächlich 48 (44 Blätter), Wiederholung null neue Posen.
Fünf CPU-Läufe/256 Eltern: 24.195 Standorte konstant, Median 30,725 ms skalar / 10,440 ms Batch;
28.416 Standorte mit Profil, 38,192 / 15,309 ms; gleiche vollständige Folgen, ohne Mesh/IO.
Frischer Prüf-/Produktprozess 85,47 MiB Peak; vier Graph-/Detailprodukte zusammen 0,274 s (ein Lauf).
Funktionsbilder: drei Eltern von 0,55/0,95/1,35 m; 62 Blätter bzw. 39 Astquirl-Instanzen und 273 Blätter.
Feine/grobe Prototypen direkt: Blattfolgen 4.304/518, Astquirle 5.480/674 Dreiecke.
Signed Bildversätze vor Arithmetik; die unsigned Hierarchie-IDs bleiben unverändert.
Der erste Bildvergleich deckte den Überlauf auf; alle vier korrigierten Endbilder zeigen die drei Eltern.
Grobe Bilder behalten die Standortfolge und Quirlstruktur; Feingeometrie/Schattengrenzen bleiben offen.

## Abnahme
Unabhängige skalare Standortfolge gegen Batch-Verfahren, Grenz-/Profil-/Gruppenfälle.
Voll-/Teilanforderungen, warme Wiederverwendung, Bounds und Budgets im Graphen prüfen.
Eltern unterschiedlicher Länge in 2560×1440 rendern und Bilder tatsächlich betrachten.
Kosten, Speicher und vollständige gespeicherte Produkte getrennt messen; commit/push/showcase.
