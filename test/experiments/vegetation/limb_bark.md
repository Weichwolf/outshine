# Metrische Rinde auf durchgehender Asthaut

Forschungsumfang: #759, Kind von #682; vorhandene Grundlagen #35 und #739.
Der zuletzt geprüfte Baum besitzt eine durchgehende C1-Holzhaut, aber nur eine
konstante braune Oberfläche. Dieser Versuch ergänzt Materialkoordinaten direkt
beim vorhandenen Sweep. Er ersetzt weder die Geometrie noch das Pflanzenrezept.
Die Rindenmaterialien bleiben ausdrücklich unkalibrierte Bildproben.

## Verfahren und Grenzen

`limb_mesh(..., bark=True)` ergänzt zwei Float64-Punktfelder:
`bark_coordinates=(r cos φ, r sin φ, s)` und `bark_radius=r`.
`s` ist die integrierte Geschwindigkeit der vorhandenen kubischen Mittellinie.
Eine feste 16-Punkt-Gaußregel integriert jedes Sweepintervall separat; diese
Intervalle überqueren keine kubischen Quellsegmente. Regelpunkte und Gewichte
werden einmal erzeugt und unveränderlich wiederverwendet.

Die Querkoordinaten bilden einen Kreis im lokalen transportierten Rahmen ab.
Die umlaufende Schließkante benutzt dieselben vorhandenen Ringvertices; ein
periodisches Winkel-U und seine Texturauflösung entfallen. Endkappenzentren
erhalten `(0,0,s)` und den jeweiligen Endradius. `canonical_wood` verkettet die
Felder in derselben Reihenfolge wie Vertices und bestehende Pfadbesitzer.
Kein Punkt wird nachträglich gegen fremde Astachsen geprüft. Ohne `bark=True`
werden weder Charts noch Quadratur erzeugt. Kapazitätsablehnung erfolgt vorher.

Das ist ein lokales solides Materialfeld, keine globale isometrische
Parametrisierung. Auf Facetten wird linear interpoliert. Getrennte Seitenäste
behalten getrennte Charts mit eigener Nullphase; Kragen, Elternphase, Endholz
und Fusionsflächen sind damit nicht gelöst. Die Vorschau setzt Objektmaßstab
eins voraus. Erneut erzeugte einheitlich skalierte Quelldaten ergeben metrisch
skalierte Felder; beliebige nachträgliche Instanztransformationen sind nicht
abgenommen. Ein LOD-Wechsel darf nicht als texturidentisch vorausgesetzt werden.

## Prozedurales Material

`bark_material.py` bleibt unverändert. `limb_bark_material.py` verwendet dessen
Principled-/Rauheits-/Radius-/Bumpaufbau und ergänzt ein gemeinsames Höhenfeld
für Pigment und Normalen. `mature_recipe()` besitzt die expliziten Vorschauwerte;
die zusätzliche Komposition ist durch diesen Versuchscode festgelegt:

- Feinkorn: bestehendes 3D-Noise mit metrischer Skala `(70,70,4)`.
- Grobe Platten: Blender-3D-Voronoi `DISTANCE_TO_EDGE`, Skala `(18,18,2)`.
  Noise bei Skala 0,8/Detail 2 verschiebt den Abfragepunkt um
  `0,9*(Color-0,5)` in diesem dimensionslosen Plattenraum.
- Furchenhöhe: geklemmtes Smoothstep von Zellrandabstand 0 bis 0,16;
  Plattenhöhe mal `(0,75+0,25*WarpNoise)` plus `0,08*Feinkorn`.
- Derselbe Wert steuert Farbverlauf und Bumphöhe. Rauheit bleibt 0,8,
  metallisch null; Reliefdistanz 6 mm, Stärke maximal 0,5, bei Radien
  5–25 mm weich zunehmend. Dünne Zweige behalten die alte glattere Färbung.

Die verwendeten Texturfunktionen sind Blender-Standardknoten; kein Vendorasset,
keine Fototextur und keine übernommene SpeedTree-Formel. Das sind noch keine
gebackenen GPU-Assets oder abgenommenen nativen Materialbindungen.

Grundlage ist die lokale SIGGRAPH-Veröffentlichung
[Perlin 1985, An Image Synthesizer](../../../doc/references/vegetation/siggraph/1985-image-synthesizer.pdf):
solide Koordinaten und komponierte stochastische Funktionen; PDF-Seite 8 mit
Musterbild wurde erneut optisch geöffnet. Die konkrete Zellkomposition stammt
aus diesem Experiment, nicht aus diesem Paper. Die bereits mit Ghidra geprüften
SpeedTree-UV-Befunde stehen in #35 (u.a. `ComputeBlendedUVs`, `0x1c739f0`);
hier wurde kein neuer Binärlauf durchgeführt. Der eigene solide Chart benötigt
deren periodische U-Glättung nicht. Daraus folgt keine SpeedTree-Gleichwertigkeit.

## Korrektheit und Kosten

Der kalte Wiederaufbau aus `data/crown682` reproduziert die feste Ausgangsquelle.
Die Teilkronenquelle behält Plan `0e9819d59d105c236be898111214c0c6d7c20cf4fbbb80646ffb122de36fc7d6`
und Katalog `680b9d908b2fdb370420e6a6a4d33bf676058c88b651c15dce4b8cceb37d8021`.
Alle alten Holzarrays sind exakt gleich, einschließlich Ecknormalen und Besitzern.
Keine Blattpose oder Nahlamina ist für die Rindenfelder nötig.

| Vollständiges Holzprodukt | Beobachteter Wert |
|---|---:|
| Pfade / Vertices / Dreiecke | 993 / 72.234 / 140.496 |
| Alte Geometriearrays | 14.097.264 Bytes |
| Zusätzliche persistierte Felder | 2.311.488 Bytes |
| Zusätzliche Geschwindigkeitsauswertungen | 124.608 |
| Chartvertex-Auswertungen / fremde Achsenabfragen | 72.234 / 0 |
| Aufbau ohne / mit Rinde, Median aus je drei alternierenden Läufen | 2,215 / 2,392 s |
| Separater Python-Tracemalloc-Aufbaupeak ohne / mit | 29.093.172 / 33.988.087 Bytes |

Messung Python 3.13.5/NumPy 2.5.4/SciPy 1.18.1 auf gemeinsam genutztem Host.
Gleiche vorab residente Eingaben; Peaks erfassen nur während des jeweiligen
Aufbaus verfolgte Allokationen, weder RSS noch kumulative Allokationsbytes.
Keine vollständigen Speicherzugriffszähler oder GPU-Zeiten vorhanden.
Die neuen Felder werden pro Pfad angelegt und einmal ins vollständige Produkt
kopiert: während der Verkettung leben Teilfelder und Gesamtfelder gleichzeitig.
Das erklärt zusätzlichen Peak; es ist kein Nullkopiepfad. Die alte Geometrie
behält ihren bisherigen Aufbau/Kopieraufwand. Das Ergebnis ist ein bezahlter
Materialzusatz, keine Beschleunigung des bisherigen glatten Baums.

Sechs neue Kontrollen prüfen unabhängige adaptive Quadratur, analytische Gerade,
kurze und 68-stationige Pfade, Maßstab/Transformation, Kappen, Rundumnaht,
arraygleichen Altpfad, Verkettung/Besitzer und Kapazität vor Arbeit. Zusätzlich
stimmen alle Stationen aller 993 tatsächlichen Pfade mit adaptiver Quadratur
bis beobachtet `4,875e-12 m` überein (Prüftoleranz `1e-9 m`). Das ist ein
numerischer Vergleich, keine allgemeine Fehlergrenze oder physische Genauigkeit.

## Bildbefund und Reproduktion

Alle Vergleiche zeigen dieselbe vollständige Holzgeometrie, zwei feste Kameras
(Stammnähe/Ganzholz), 2560×1440, 32 EEVEE-Samples und identisches Studiolicht.
Blätter fehlen beidseitig, damit die Materiallücke sichtbar bleibt; Schatten
sind ausgeschaltet. Vorheriges konstantes Braun, reine Fasern, unverformte
Zellplatten und die verformte Probe wurden tatsächlich geöffnet.
Fasern allein wirken wie Holzmaserung; unverformte Zellen besitzen zu geradlinige
Kanten. Die Verformung bricht einige davon auf, erhält jedoch lange große
Zellen; der Stamm wirkt weiterhin geschnitzt. Als überzeugende Eichenrinde
ist auch diese Probe verworfen. Erhalten bleiben der geprüfte Chart und der
ausdrückliche Diagnoseshader, keine neue Art-/Materialvorgabe. Weitere reine
Voronoi-Parametervarianten sind kein begründeter nächster Schritt; erforderlich
ist ein anderer Aufbau der längs laufenden, unregelmäßig verzweigten Furchen.
Massive Gabeln, Reliefsilhouette, maßstabsabhängige Filterung und eine
unabhängige lebende Eichenreferenz bleiben offen. Der ganze belaubte Baum,
bewegte/schattierte Szene und Welt-/Gerätebudget werden hier nicht abgenommen.

Alle privaten Produkte und Bilder bleiben ausschließlich unter `/tmp`.
Aufruf aus dem Checkout, Umgebung `.venv`; `PYTHONPATH` enthält
`lib/src:lib/tests:test/experiments/vegetation:test/experiments/render`.
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 PYTHONDONTWRITEBYTECODE=1` setzen.
Private Matplotlib-/Mesa-/XDG-Caches ebenfalls nach `/tmp` legen.

```sh
.venv/bin/python test/experiments/vegetation/subcrown_probe.py --output /tmp/outshine-vegetation-bark/pilot
.venv/bin/python test/experiments/vegetation/limb_bark_probe.py --pilot /tmp/outshine-vegetation-bark/pilot --original test/experiments/vegetation/data/crown682 --output /tmp/outshine-vegetation-bark/product
EGL_PLATFORM=surfaceless DISPLAY= LIBGL_ALWAYS_SOFTWARE=1 .venv/bin/python test/experiments/vegetation/render_limb_bark.py --inputs /tmp/outshine-vegetation-bark/product --mode bark --view trunk --output /tmp/outshine-vegetation-bark/bark-trunk
.venv/bin/python -W error -m unittest limb_bark_checks limb_checks source_limb_checks subcrown_checks oak_scaffold_checks oak_fork_checks bark_checks bark_layer_checks graph_wood_checks
```

Für den Bildvergleich `--mode smooth` und `--view whole` ergänzen; jedes Ziel
muss neu sein. Blender 5.2.2 LTS meldet hier llvmpipe/LLVM 19.1.7: ausdrücklich
kein RTX-Nachweis. Nach Neustart fehlende Privatdaten werden aus den versionierten
Originaleingaben erzeugt; alte Berichte ersetzen keine neue Produktprüfung.
