# Begrenztes Bodenmodul an vorhandener ganzer Schnittstelle (#522)

Die [feste Rezeptauswahl](fixtures/stair-west-ground-module-vienna.json) ergänzt
vier ganze Originalwege/rund 531 m am offenen westlichen Anschluss des
[32-Quellen-Verbunds](stair_north_bridges_comparison.md). Der alte Straßenkörper,
seine Treppen und Brücken sowie alle vorhandenen Pfeiler bleiben unverändert.
Die neue Höhenplanung umfasst nur vier Quellen. Das alte 32-Quellen-Budget wird
nicht erhöht. Diese Familie unterstützt ausdrücklich flache, tatsächliche
Bodenanschlüsse von Geh-/Radwegen; andere Quellen brauchen andere Rezepte.

## Verfahren und Besitz

Der Erzeuger liest verifizierte alte Körper/Felder und deren gebundene Berichte,
die tatsächliche flache äußere Profilfläche und neue Originalteile aus demselben
festen PBF. Er ruft keine alte Quellen-/Höhenplanung auf. Ein gemeinsamer
ursprünglicher MVT-Knoten und ganze Profilhüllen verbinden den neuen Ast.
Nähe ersetzt diesen Nachweis nicht. Originale, IDs und DEM-Priors bleiben
erhalten; die gespeicherte Anschlusshöhe ist keine gemessene OSM-Deckhöhe.

Neue Bandflächen werden gegen vorhandenen Boden ausgeschnitten. Mitbenutzte
alte Flächen erhalten pro neuer Quelle eine ausdrückliche Flächenbilanz.
Die installierbaren `fit_weighted_heights`-/`close_surface`-Kerne erzeugen die
Ergänzung als zwei geschlossene Teilkörper. Ganze Profile und tatsächliche
Seitenwände prüfen die Verbindung. Gemeinsame vertikale Rechtecke müssen auf
beiden Seiten vollständig und entgegengesetzt orientiert getragen werden.
Die kanonische Double-Ebene bleibt auch beim tatsächlichen Float32-Mesh fest:
aus einem kurzen gerundeten Teilsegment neu geschätzte Ebenen würden ihren
Winkelfehler über eine lange vorhandene Wand vergrößern. Die XY-Hülle leitet sich
aus den tatsächlichen lokalen Koordinatenrundungen plus 100 nm Konstruktion ab;
die getrennte Höhenwache bleibt 100 nm. Keine neue pauschale Millimetergrenze.
Die Körper bleiben separate Assets mit ausdrücklich beschriebenen Kontaktflächen;
kein boolesch verschmolzener Gesamtmesh oder globales Null-Überlappungsvolumen.

Das Terrain wählt ganze alte Facetten bis 24 m um den Zusatz und schließt die
Eltern aller betroffenen Pfeilerfüße ein. Alte Straßenbetten und diese
Pfeilereltern bleiben feste Referenzen. Neue Referenzabfragen benutzen echte
konforme Meshes; eine Triangle-Soup mit getrennten IDs ist keine kontinuierliche
Samplerreferenz. Ganze alte Randhöhen werden direkt aus den alten Vertices
übernommen. Außerhalb der gewählten Region müssen die alten XYZ-Facetten
bytegleich bleiben. Die gemeinsame Weltassembly dient Prüfung und Darstellung;
sie ist kein neuer nativer Cache-/Streamingvertrag.

Die neue Familie wählt ausdrücklich **0 m zusätzlichen** geometrieabhängigen
Brücken-Solverzuschlag. Der bisherige Default 0,1 m bleibt unverändert.
Dieser zusätzliche positive Zuschlag kann neu geschnittenen Facetten eines
schon physisch geprüften, fest gehaltenen Feldes widersprechen. Der echte
Gegenfall mit altem Zuschlag bleibt ablehnend. Tatsächliche vollständige
Double-/Float32-Kollisionsfreiheit, Boden-/Pfeilerkontakte, Neigungen und
5-m-Konstruktionsbereich bleiben verbindlich. Bei gewöhnlicher lokaler Ablehnung
bleibt der geprüfte alte Teilverbund verwendbar; alle neuen IDs bekommen einen
konkreten Ablehnungsgrund. Ein erfolgreich beendeter Prozess beweist damit
ausdrücklich noch kein neues Modul.

## Quellen und Reproduktion

Zuerst lokale SIGGRAPH-Referenzen:
[Chen et al. 2008, S. 2/Fig. 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
Graph und Geometrie getrennt;
[Hoppe et al. 1993, S. 3–4](../../../doc/references/transport/siggraph/1993-mesh-optimization.pdf),
Konnektivität und geometrische Einbettung. Zusätzlich
[Galin et al. 2010, S. 9/Fig. 20](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
lokaler Geländequerschnitt. Seiten und Abbildungen optisch geprüft.
Die konkrete Modul-/Kontaktprüfung und die konservative Teilfamilie sind eigene
Verfahren; kein vollständiger Straßengenerator aus diesen Quellen abgeleitet.
SDK/Fremdcode und Runtime-Abhängigkeiten werden nicht übernommen.

`stair_module_experiment.py` erzeugt, `stair_module_verify.py` liest gespeicherte
Produkte ohne Höhenfit oder Meshneubau. Beide verlangen einen sauberen Commit.
`--base` ist der vollständig geprüfte 32er Netz-/Terrainstand, `--supports`
dessen geprüfter Pfeilerstand. Deren gebundene Berichte und Originale müssen
vorhanden sein. Alle privaten Ausgaben liegen in ausdrücklich neuen System-Temp-
Verzeichnissen; keine Abhängigkeit der festen Tests von privaten Ausgaben.

```sh
export PYTHONPATH=lib/src:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest stair_module_checks -q
python test/experiments/transport/stair_module_experiment.py \
  --base "$TRANSPORT_BASE" --supports "$TRANSPORT_SUPPORTS" \
  --output "$TRANSPORT_MODULE"
python test/experiments/transport/stair_module_verify.py \
  --base "$TRANSPORT_BASE" --supports "$TRANSPORT_SUPPORTS" \
  --output "$TRANSPORT_MODULE"
```

Der kleine feste Kontakt-/Terraininput enthält tatsächlich ausgeschnittene alte
Referenzflächen samt Elternindizes und Originalhashes. Er ist kein ganzer
Weltcache und ersetzt nicht den vollständigen Erzeuger-/Reader-/Bildnachweis.
Weitere Modulklassen, vollständige Fahr-/Gehhüllen, Statik, LOD-/native Integration
und die weltweite Quellabdeckung bleiben außerhalb dieser örtlichen Abnahme.
