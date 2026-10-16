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

## Vollständiger aktueller Export

Erzeugung und unabhängige Reader auf `14b0a8f75` bestanden terminal mit Exit 0.
Die erste, einmal verfeinerte Bodenpartition blieb mit den wirklichen
Brückenbedingungen `PrimalInfeasible`; sie wird nicht als Ergebnis verwendet.
Eine ausdrücklich gewählte zweite freie Verfeinerung ergänzt 192 Zellen und
341 Kantenmittelpunkte. Höchstens zwei Durchgänge, unveränderte Punkt-/Facecaps,
acht Solversekunden/100 Iterationen, Erdneigung 2/3, konstruktiv 10 und wirklicher
5-m-Bereich. Kein unbegrenzter Wiederholungsversuch oder gelockerter Freiraum.
Die Elternindizes werden bis zur ursprünglichen Partition zusammengesetzt;
die echte Prüfung kontrolliert Kind-in-Elternfläche und unveränderte Bodenfacetten.
Ohne neue Option bleibt der vorige 24-Quellen-Terrainplan arrayweise bytegleich.

11975 Punkte/23592 Höhenfeldfacetten tragen das unveränderte Rechteck von
2044993,089198 m². Der geschlossene Geländekörper hat 23950 Punkte/47896 Facetten.
Die vier vollständigen Original-Terrarium-Decodes stimmen mit den ganzen festen
Crops überein; alle 11975 Zielhöhen stimmen exakt mit erneuter Rohabtastung überein.
Alle 2233 ursprünglichen Bodenfacetten bleiben als ganze XYZ-Dreiecke bytegleich
in Double und Float32 erhalten. Ganze Kontakt-/Hüllen-/Vertexlink-/Klassenprüfungen
bestanden, ohne globale Null-Überlappungsvolumen- oder Selbstschnittbehauptung.
Die tatsächlichen Erdneigungen sind höchstens 0,664684635/0,666502132,
konstruktiv 9,997999050/9,998496124. 81 konstruktive Facetten belegen 11,223479 m²
projizierte Fläche im wirklichen 5-m-Bereich. `AlmostSolved` allein wäre kein Beleg;
erst die nachgelagerte Prüfung des wirklichen Exports lässt diesen Fall zu.

Der bestehende installierbare SupportVolumeBuilder erzeugt 79 feste oder 84
verschobene ganze Pfeiler bei 88 Stationen. Die gewählte Variante benötigt 113
Versuche/12268 Kontaktpaarprüfungen/3260 Körperfacetten; vier Stationen bleiben
wegen anderer gewählter Verkehrsquellen ausgenommen. Mindesthöhe Float32
0,108795 m; größter tatsächlicher Float32-Kontaktfehler 14,446 µm innerhalb der
jeweils abgeleiteten Grenze. Weiterhin 12-m-Stationen, höchstens 4 m Verschiebung
und direkte 0,2-m-Fremdverkehrsabstände in beiden Präzisionen. Keine Statik,
Fundamente oder Bodenkennwerte abgenommen.

Der Quellenkatalog erhält 1657 ursprüngliche Linienteile: 28 Kandidaten,
1629 übrige Linien sowie 71 Flächen mit Originalen/IDs und Auslassungsgründen.
`admitted_source_ids=[]`: vollständige Verkehrs-/native Zulassung bleibt offen.
Sieben Außenprofile bleiben; der alte westliche Endpunkt ist jetzt geschlossen,
der Anfang von `39881120:25` neu offen. Die übrigen sechs Außenprofile bleiben.
Gespeicherte aktuelle Terrain-/Pfeilerreader laufen außerdem mit gesperrtem CDT,
LP-/Kegelsolver, Körper-/Pfeilerbuilder und Arrayexporter. Der bisherige dichte
24-Quellen-Reader besteht separat, ohne seinen ursprünglichen Beleg zu verändern.

## Gleicher Qualitätsvertrag und gemessene Arbeit

Je ein frischer normaler Prozess und separat je ein Memray-1.20.0-Prozess mit
Pythonallokatoren/native traces, `OPENBLAS_NUM_THREADS=1`, `OMP_NUM_THREADS=1`.
i5-1135G7, zwei physische/vier logische Kerne, rund 16 GiB; keine exklusive
Maschinenreservierung. Messcommit `3b882089a`; der finale neue Körper ist exakt
derselbe wie im Kostenlauf. Die spätere Fixtureänderung betrifft ausschließlich
die Geländeverfeinerung, nicht diesen gemessenen Körperabschnitt.

| Ganzer Körper | Dicht, 24 Quellen | Achsknoten, 24 Quellen | Achsknoten, 28 Quellen |
|---|---:|---:|---:|
| Normale Aufbauzeit, s | 6,5623 | 2,4428 | 3,0062 |
| Prozesspeak, B | 376885248 | 225988608 | 255234048 |
| LP Variablen / Zeilen | 6664 / 146208 | 3312 / 68755 | 3868 / 81515 |
| LP Nichtnullwerte | 432970 | 203829 | 241425 |
| Behaltene Sparsearrays, B | 5222300 | 2459200 | 2912576 |
| Explizite XY-Quantisierungskopie, B | 79032 | 38808 | 45336 |
| Eigene Körperarrays, B | 933580 | 484064 | 556548 |
| Profilierte Zeit, s | 25,8389 | 12,1769 | 13,9513 |
| Allokationsereignisse | 19044179 | 9353021 | 11014396 |
| Kumulativ angefordert, B | 2526093906 | 1218687546 | 1422851509 |
| Getrackter Heappeak, B | 349448518 | 165181087 | 194307486 |

Zeit/Tracker umfassen Podestauswahl, Arrangement, LP und geschlossenen Körper
nach Imports/Eingangsdecode und bisheriger einzelner Bodenpodestergänzung.
Ganze physische Prüfungen und Datei-IO liegen außerhalb des Trackers; der
Prozesspeak enthält diese Prüfungen. Terrain, Pfeiler und Renderer liegen außerhalb
dieser Messphase. Alle 41 beziehungsweise 49 inneren Profile und zwölf
beziehungsweise 16 Paarprofile bestehen; kein Vergleich mit unvollständiger Ware.
Der alte dichte Körper bleibt arrayweise bytegleich. Interne Gesamtkopien sind
unbekannt; explizite Kopien und behaltene Arrays sind getrennte Größen.

Der ganze neue Netz-/Terrainaufbau vor Display benötigt separat 26,3238 s,
Prozesspeak bis dorthin 256282624 B. Das letzte Höhenfeld braucht 3,6538
Solversekunden/5,0171 s einschließlich Vorbereitung. Pfeilergeneration inklusive
beider Varianten/Display separat 4,8728 s/132681728 B Prozesspeak. Phasenpeaks
nicht addieren; instrumentierte Zeiten nicht als normale Zeiten ausgeben.
Keine Frame-, Geräte-, Weltlade- oder universelle Beschleunigungsabnahme.

## Tatsächlich geöffnete Bilder und Belege

Vier frische 1280×720-Bilder auf `14b0a8f75`: vorher/nachher jeweils ganze
2248,388-m-Übersicht und moderater 700-m-Netzausschnitt mit ganzer alter Brücke
und neuem westlichem Ast. Identische Kamera, Licht und Palette pro Paar,
tatsächliche Mesh-/Normal-/Farb-/Helper-/Szenenbindungen geprüft, alle PNGs ganz
decodiert und geöffnet. Blender Eevee über llvmpipe/LLVM 19.1.7, kein GPU-/Runtimebeleg.
Die neue westliche erhöhte Verbindung/Schleife samt Pfeilern ist sichtbar;
die Umgebung bleibt klinisch und die dünnen hellen Wege weiterhin kontrastarm.
Keine Nahansichten oder öffentliche Ersatzgalerie. Pixels: 0,5541 % der
Übersichtspixel und 3,3311 % im Netzausschnitt ändern sich; keine Behauptung,
dass jede Pixeländerung ein Qualitätsgewinn ist. Renderzeiten je Bild 11,55–18,03 s.

Lokale Originalbelege: `/tmp/outshine-transport-frontier-paths-1fd7341d8/`
(65 Quellkontrollen, weiterer 20er-Lauf mit überlappenden Brücken-/Terrainfällen,
Kosten-/Memrayberichte, alte Defaultgleichheit, Roh-/Reader-/Bildbelege und der
abgelehnte Ein-Durchgang-Fall). Aktuelle ganze Produkte:
`/tmp/outshine-transport-west-bridges-full-14b0a8f75/`, Pfeiler und vier Bilder:
`/tmp/outshine-transport-west-bridges-support-14b0a8f75/`.
Keine SDK-/Fremdcode-/native Runtime-Abhängigkeit hinzugefügt.

## Reproduktion

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest stair_west_bridge_checks stair_landing_checks \
  test_junction_pads stair_corner_checks stair_corridor_checks stair_cycle_checks \
  stair_cycle_north_checks stair_cycle_south_checks stair_terrain_cells_checks -q
python -W error -m unittest stair_west_terrain_checks stair_blend_checks -q
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
