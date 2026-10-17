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

## Vollständiger örtlicher Prüfstand

Geometrie-/Erzeugerstand `ef8cb0a1c`, Kostenhelfer `d151b1005`, zusätzlicher
gespeicherter Feldkopplungscheck `11a609cc5`. Linux/WSL2, i5-1135G7, vier
sichtbare logische CPUs, rund 15,44 GiB RAM; Python 3.13.5, NumPy 2.2.4,
SciPy 1.18.1 und Shapely 2.2.0. Die spätere Integration enthält Buildings
PR #525 und Masters Terrain-/Skirt-Fortschritte; unsere benutzten Bibliotheken,
Transporthelfer und Eingaben bleiben dabei unverändert.

- Vier ganze Originalteile/531,063 m sind ergänzt. 36 lokale Kandidaten,
  62 ganze innere Endprofile (56 alte, fünf neue und der geschlossene alte Port),
  zehn verbleibende Außenprofile. 1621 übrige Linien und 71 Flächen bleiben mit
  Originalen/IDs/Gründen erhalten; native `admitted_source_ids=[]` bleibt.
- Neuer Körper: 145 obere Vertices/203 Facetten, zwei geschlossene Teilkörper,
  290 Vertices/576 Körperfacetten. Alle tatsächlichen Double-/Float32-Facetten
  sind nicht kollabiert und nicht invertiert; Volumen, Vertexlinks und ganze
  Quellenflächen/Neigungen bestehen. Gemeinsame Wandlänge 8,593926845 m,
  fünf ganze vertikale Segmente. Beide Seiten tragen die vollständigen
  Rechtecke innerhalb der abgeleiteten Rundungshülle; größte XY-Wache
  7,235 µm, getrennte Höhenwache 0,1 µm. Kein exaktes Null-Spaltvolumen behauptet.
- Keine neue projizierte Brückenüberlappung. Kleinster Abstand zur ganzen
  vorhandenen Pfeilerprojektion 3,618813/3,618814 m, Double/Float32, über 0,2 m.
  Alle 124 alten ganzen unteren Kontakte bestehen, maximale Float32-Abweichung
  weiterhin 15,170 µm. Alte Körper/Treppen und 14 ganze Überführungen bleiben.
- Lokales Feld: 705 Punkte/1302 Facetten, 549 feste Höhen, 106 bytegenaue alte
  Randhöhen. 520 ausgewählte alte Facetten und zehn geschützte Eltern unter
  fünf betroffenen Pfeilern. Welt: 12764 Punkte/25170 obere Facetten;
  Diagnosekörper 25528 Vertices/51052 Facetten. 23868 alte Außenfacetten
  XYZ-bytegleich. Gespeicherte lokale und ganze Felder müssen für alle 705
  Vertices/1302 Facetten exakt zusammenpassen; gültige, aber verschiedene
  Einzelfelder werden nicht als zusammengehöriges Produkt akzeptiert.
- Ganze Klassen-/Brücken-/Bodenprüfungen bestehen in Double und tatsächlichem
  Float32. Globale Erdneigung maximal 0,664668769/0,665873762, konstruktiv
  9,998000298/9,998496124. 87 konstruktive Facetten/37,164596 m² innerhalb des
  wirklichen 5-m-Bereichs. Drei davon liegen im neuen lokalen Feld.
- Alle vier ganzen Roh-DEM-Decodes/Crops stimmen exakt; 13642 Abfragen,
  darunter sämtliche 12764 Welt-, 705 lokalen und 145 Modulziele sowie die
  Originalachsen, stimmen bitgenau mit vollständigen Roh-Decodes überein.

133 breit ausgeführte Quellkontrollen bestanden in 96,483 s, Exit 0.
Nach dem zusätzlichen Reader-Kopplungscheck bestanden nochmals alle 13
betroffenen Modulprüfungen; die Umfänge überlappen sich. Alle 13 alten 32er
Körperarrays einschließlich Dtype/Shape bytegleich, alte Layoutnotiz unverändert.
35 aktuelle installierte Kontrollen bestanden in 21,944 s, Exit 0.
Die unveränderte Bibliothek verwendet das schon geprüfte Wheel aus PR #520;
alle 198 Module und der tatsächliche Importpfad werden gegengeprüft.
Ruff Check/Format, Funktionen höchstens 120 Zeilen, Diffcheck und Docs-Gate.

Frische vollständige Erzeuger beider Formulierungen und gespeicherte Reader
bestanden. Neun alte Planungseinstiege sind im wirklichen Erzeuger gesperrt;
der einzige neue Arrangementaufruf erhält vier Quellen. Reader auch mit
gesperrtem CDT, LP/Kegelsolver, neuem/alten Plan-/Körperbau und Arrayexport.
Die Gegenprobe mit nicht flacher tatsächlicher Referenz beendet sich mit
verwendbarem alten Teilverbund und vier konkreten Ablehnungen, ohne neue
Geometrie zu behaupten. Alter Terrain-Default mit 0,1-m-Zuschlag besteht
ebenfalls im gespeicherten Reader ohne Neubau; sein alter Beleg bleibt erhalten.

## Gleiche Eingaben und tatsächliche Kosten

Beide Varianten benutzen identische lokale XY-Facetten, DEM-Ziele, feste Höhen,
Gewichte und ganze Qualitätsbedingungen. Nur Variablenkoordinaten und direkte
Elimination fester Höhen unterscheiden sich. Einzelne normale Läufe:

| Formulierung | Solvervariablen | Zeilen | A-Nichtnullen | Lokaler Fit s | Ganzer Erzeuger s | Peak-RSS B |
|---|---:|---:|---:|---:|---:|---:|
| feste Höhen eliminiert | 413 | 6333 | 9057 | 0,295152 | 14,522418 | 175550464 |
| ursprüngliche Höhen | 962 | 6882 | 15573 | 0,265721 | 14,259950 | 175439872 |

Weniger Unbekannte belegen hier keinen Tempogewinn. Für die reduzierte Form
werden außerdem 118764 B Sparse-Arrays und 23088 B explizite Verschiebungs-/
Lineararrays gehalten. Gemeinsames ursprüngliches Sparse-Arbeitsvolumen
234360 B. Beide Verfahren liefern gültige Produkte; globale Optimalität oder
ein geräteübergreifender Laufzeitgewinner werden nicht hergeleitet.

Ganzer Standardlauf: neuer Provider 0,23 s, Cachelesen 0,04 s, neuer Körper
0,13 s, Modulprüfung 0,36 s, lokale Terrainplanung 0,31 s, Welt-Diagnoseassembly
1,83 s und ganze Weltprüfung 10,7 s. Der Provider dekodiert weiterhin die ganze
218795-B-PBF-Kachel und normalisiert 1657 ursprüngliche Linienteile für Quellen
und vollständige Bilanz. Kein vollständig quellenfreier Cachetreffer und kein
Weltlade-/Frame-/A18-/RTX-Nachweis. Die neue Planung/Höhenanpassung wiederholt
keine alten Quellenrezepte; globale Quellen-/Prüf-/Diagnosearbeit bleibt sichtbar.

Explizites Array-Arbeitsvolumen: rohe lokale XY 49200 B, eindeutige XY 11296 B,
Zuordnungen 24600 B. Numerischer Weld: 1020 → 729 Punkte/291 Paare, tatsächliche
Verschiebung 7,106e-15 m; 24 unbenutzte Punkte danach entfernt. Ganze
XY-/Zuordnungsassembly 308848 B, alter/neuer Außen-XYZ-Vergleich 3436992 B.
Die sieben Produktarchive enthalten zusammen 3144719 B Arrays. Der Wandzähler
96840 zählt den finalen Durchlauf; dieselben Facetten werden vorher nochmals
für die Ebenenwache projiziert. Interne GEOS-/Solverkopien bleiben unbeobachtet.

Memray 1.20.0, native und Python-Allokatoren, jeweils ganzer Erzeuger nach
Imports einschließlich Provider, aller Prüfungen, Inventar und Export;
PNG-Rendering und unabhängiger Reader separat:

| Formulierung | Allokationsereignisse | Kumulativ angefordert B | Getrackter Heap-Peak B | Profilierte Erzeugung s | Erzeuger-Peak-RSS B |
|---|---:|---:|---:|---:|---:|
| feste Höhen eliminiert | 80482651 | 8920356875 | 52926185 | 97,541801 | 289423360 |
| ursprüngliche Höhen | 80505491 | 8921488194 | 52913453 | 97,771033 | 290349056 |

Angeforderte Bytes sind Aktivität einschließlich erneuter Anforderungen,
weder eindeutiger Speicher noch kopierte Bytes. Der getrackte Heap beginnt
nach Imports; RSS enthält weitere Prozess-/Profilerkosten. Statistik läuft im
eigenen, ebenfalls begrenzten Prozess; dessen zusätzlicher Peak ist nicht
gemessen. Profilierte Zeiten werden nicht als normale Zeiten verwendet.
Ein früher Python-Ereigniszähler wurde nach abgeschlossenem Trace am
120-CPU-s-Limit beendet (Exit 137); dieser Worker zählt nicht als bestanden.
Der aktuelle Helfer verwendet die offizielle native `memray stats`-Auswertung
in einem getrennten Prozess. Beide aktuellen Kostenworker enden mit Exit 0.
Alle acht erzeugten NPZ-/Displayarchive sind je Formulierung zum normalen Lauf
arrayweise bytegleich. Kein Profilierungsergebnis ersetzt die tatsächliche
unabhängige Körper-/Feld-/Exportprüfung. Memray korrigiert beim Start die
Allocator-Symbole; hier werden Summen, keine vollständigen Stackzuordnungen,
als Beleg verwendet.

`stair_module_cost.py` akzeptiert dieselben Eingaben, `--variable-mode` und
optional `--profile <frischer Tracepfad>`; zusätzlich wird Memray 1.20 benötigt.
Normale/Instrumentierungsläufe benutzen jeweils neue System-Temp-Ausgaben.

## Wirkliche Bilder und verbleibende Lücken

Vier frische 1280×720-Vorher-/Nachherbilder sind vollständig dekodiert und
geöffnet: ganze Übersicht und derselbe moderate 1450-m-Ausschnitt. Tatsächliche
Positions-/Topologie-/Normal-/Farb-/Kamera-/installierte Helperbindungen stimmen.
Eevee mit llvmpipe/LLVM 19.1.7: ungefähr 11,6–15,4 s pro Bild unter paralleler
Reader-/Kontrolllast. Das ist kein Geräte-/Runtime-FPS-Beleg.
Der zusätzliche dünne westliche Ast ist sichtbar. Blasse klinische Darstellung,
kleine Linienbreiten im Gesamtbild und die geringe Quellabdeckung bleiben
Bilddefizite. Bilder bleiben privat; keine öffentliche Ersatzgalerie.

Originalbelege unter `/tmp/outshine-transport-module-evidence-ef8cb0a1c/`:
`all-checks.log`, `link-checks.log`, `wheel-final-checks.log`,
`default-body-proof.json`, `default-terrain-proof.json`, `*-guard.json`,
`*-reader-final.log`, `raw-proof.json`, `cost-summary.json`,
`profile-output-equality.json`, `render-completeness.json` und
`opened-render-proof.json`. Normale Produkte/Bilder unter
`/tmp/outshine-transport-module-full-ef8cb0a1c/`, Vergleich unter
`/tmp/outshine-transport-module-heights-ef8cb0a1c/`; aktuelle Profiltraces und
Statistiken sind dort getrennt gebunden. Ganze Modulgraphen/fortgesetzte
Erweiterung, weitere Profilfamilien, Cache-/LOD-Verträge und native Integration
bleiben offen. #434/#50 und der vollständige Forschungsauftrag bleiben aktiv.
