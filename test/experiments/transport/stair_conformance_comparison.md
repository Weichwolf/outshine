# Exakte Nullflächen nach Kantenkonformierung (#522)

Beim geplanten begrenzten westlichen Bodenmodul erzeugt die Konformierung
fast kollinearer Schnittkanten zwei exakt degenerierte Ausgabefacetten.
Der bisherige Nullflächentest steht vor diesem Schritt. Danach ist die
projizierte Coverage ungültig, obwohl ihre Flächenabweichung nur rund
4,842e-9 m² beträgt. Der bisherige Flächenwert allein belegt keine gültige
Triangulierung. Der alte Kandidat wird tatsächlich abgewiesen.

`stair_junction_layout.arrangement` entfernt deshalb nach der Konformierung
erneut ausschließlich Facetten mit exakt null orientiertem XY-Kreuzprodukt.
Alle positiven Ausgabefacetten bleiben erhalten. Coverage, ganze Quellenfläche,
Besitz, geschlossener Körper, Vertexlinks und tatsächliches Float32 werden
weiter geprüft. Keine größere Flächen-, Schweiß- oder Neigungsgrenze.
Der Bericht nennt `conformed_zero_faces_removed` nur bei tatsächlicher Entfernung;
unbetroffene alte Ergebnisse und Notizen bleiben unverändert.

## Tatsächlicher Gegenfall und Grenzen

Die [feste Eingabe](fixtures/stair-west-ground-conformance.json) hält vier
vorbereitete ganze Originalachsen, zugeschnittene Korridore, Masken und die
lokale vorhandene Nachbarfläche fest: 138663000:8, 39881120:24, 80922970:318
und 80922970:325, zusammen 531,063 m. Die ursprünglichen PBF-Attribute bleiben
im vorhandenen festen Payload; Tests dekodieren ihn erneut und verwenden die
echten Features. Achsen und DEM-Priors müssen exakt übereinstimmen. PBF-SHA256
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`,
Snapshot/Frame und vier hashgeprüfte DEM-Fenster entsprechen dem
[32-Quellen-Vorstand](stair_north_bridges_comparison.md).

Der isolierte Layoutfall besteht mit 145 Vertices/203 statt 205 Facetten.
Die bestehenden installierbaren `fit_weighted_heights`-/`close_surface`-Kerne
erzeugen daraus zwei geschlossene Teilkörper, 290 Vertices/576 Facetten.
Tatsächliche Double-/Float32-Körper, Vertexlinks und Quellenneigungen bestehen.
Die feste flache Nachbarhöhe ist eine gewählte Schnittstellenbedingung;
sie wird nicht zur gemessenen Quellhöhe umetikettiert.

Das ist eine vorbereitete lokale Layoutregression und noch keine vollständige
Modul-, Kontakt-, Terrain-, Pfeiler-, Cache- oder native Abnahme. #522 bleibt
aktiv. Gerade der ganze lokale Terrainversuch muss erhaltene Flächen und
zusätzliche Solverreserven getrennt behandeln; ein positiver Feldsolverstatus
ersetzt weder vollständige feste Bodenkontakte noch tatsächliche Exportprüfung.

## Quellen, Reproduktion und Kosten

Zuerst lokale SIGGRAPH-Grundlagen:
[Chen et al. 2008, S. 2/Fig. 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf)
trennt Netzgraph und Geometrie;
[Hoppe et al. 1993, S. 3–4](../../../doc/references/transport/siggraph/1993-mesh-optimization.pdf)
behandelt Konnektivität und geometrische Einbettung. Seiten/Abbildungen wurden
optisch geprüft. Die eigene exakte Ausgabefilterung ist keine Implementierung
ihres Optimierungsverfahrens. Kein SDK-Code und keine neue Runtime-Abhängigkeit.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 python -W error -m unittest stair_conformance_checks -q
```

Fünf Kontrollen prüfen echte Konformierung, ursprüngliche Achsen/DEM-Priors,
Erhalt jeder positiven Facette/ganzer Quellenflächen, abgewiesenen Verlust einer
positiven Facette und tatsächliche geschlossene Double-/Float32-Körper.
Der neue lineare Ausgabedurchgang bleibt innerhalb der bisherigen höchstens
8192 Facetten. Im Gegenfall: 205 Facettentests, zwei entfernte Nullflächen,
203 exportierte Facetten. Explizite neue Arrays: XY-Dreiecke 9840 B,
zwei Kantenvektoren 6560 B, zwei Produkte 3280 B, Boolmaske 205 B und
gefilterte Indizes 4872 B. Diese 24757 B sind Array-Arbeitsvolumen,
kein gemessener Gesamtpeak und keine vollständige Allokationsbilanz.
Ein zusätzlicher Durchgang ist kein Tempogewinn; vollständige Modulkosten
und Bilder werden erst mit dem ausführbaren ganzen Modul bewertet.
