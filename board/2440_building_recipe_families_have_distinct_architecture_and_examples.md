Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators
Tags: buildings, recipes, roofs, facades, variety

# Distinct architecture recipes produce a visible building catalogue

Nutzerreihenfolge: erst vollständiger Python-Modellierkern in [2480](2480_python_modelling_covers_the_documented_cityengine_capabilities.md), dann moderne/zeitgenössische Rezepte für alle lieferbaren OSM-Klassifizierungen und gültigen Kombinationen. Rezepte als gemeinsame ausführbare Python-Funktionen.
Bildreferenzen: [Modern architecture](https://en.wikipedia.org/wiki/Modern_architecture) und [Contemporary architecture](https://en.wikipedia.org/wiki/Contemporary_architecture); daraus Baukörper-/Fassaden-/Material-/Raumprogramme ableiten, Quellform/-höhe erhalten.

## Ergebnis und fehlender Vertrag
Besitzer: Gebäudeagent/Python; 2173 liefert Quelle/Nutzung, 2420 konstruktive Raum-/Öffnungspläne.
Erkennbare Bauformen mit passenden Fassaden, Dächern, Erdgeschossen und Innen-/Außenanschlüssen.
Tausende nachvollziehbar verschiedene Varianten; Familien, Varianten und Instanzen getrennt zählen.
Aktuell wenige Fassadenparameter auf extrudierten Grundrissen, keine belegte Architekturvielfalt.
2420 liefert einen nutzbaren Python-Raum-/Geschoss-/Portalvertrag; native Integration und Nutzungsprogramme bleiben offen.
Aktiver Ausbau: Dachflächen/Traufe/First auf denselben Quellgrundrissen, zuerst 2D-Schnitte, danach 2.5D und äußere Ansichten.
2400 untersucht dafür Esri-Dachkerne mit Ghidra; allgemein gültige Verfahren gegen metrische analytische Dächer vergleichen.
Gesamtoberkante/Unterkante bleiben geliefert; Dachraum vor Geschossplanung innerhalb dieser Höhe reservieren.
Gelieferte Geschoss-/Dachangaben erhalten, fehlende ergänzen und kennzeichnen; zu knappe Intervalle explizit ausweisen.
Skelettflächen für konkave/Hof-Dächer gegen analytische metrische Rechteckdächer vergleichen; Quelle/Höfe unverändert.
Zusätzliche externe Python-Bibliothek `py_straight_skeleton==0.1.0` in `.venv`; kein Fremdcode im Repository.
Ungewichtetes Skelett ist ein Vergleichspfad, keine vollständige Esri-Dachentsprechung; Gewichte/überhängende/even-Dächer bleiben offen.
[`roof_planes.py`](../test/experiments/buildings/roof_planes.py): Flach-, Walm-, einfirstige Sattel- und Pultflächen.
Rechtecke analytisch gegen Skelett vergleichen; konkave/Hofgrundrisse aus Skelett, keine Bounding-Box-Ersetzung.
Je 21 warme Läufe, gleiche geprüfte Flächen/Aufpunkthöhen/Plattenvolumen; lokale Kosten nahezu gleich, kein Globalgewinner.
Analytisch bei tatsächlich rechteckiger Quelle, sonst Skelett; Flächen müssen Quelle lücken-/überlappungsfrei abdecken.
256 Quellvertices/1.024 Skelettknoten/4.096 Events; Zwei-Sekunden-Abbruchcheck während Events, Vorbereitung/Initialisierung außerhalb dieses Timers.
Gemeinsamer Bibliothekstracer geschützt und auch nach Fehler wiederhergestellt; keine harte Framezeitgrenze.
[`roof_components.py`](../test/experiments/buildings/roof_components.py): geschlossene 0,18-m-Dachplatten und Giebel-/Attikwandinfill.
Obere Geschossdecke am Dachkontakt verjüngen, darunterliegende Kopffreiheit erhalten; überlappende Außenflächen erzeugten sichtbare Streifen.
Abgeleitete Kontaktpolygone verwenden beidseitig das vorhandene 1e−8-m-Raster; Kontaktdicken unter 1e−7 m auf null, größere negative Dicke verwerfen.
Auch Wandinfill im selben Raster; exakt doppelte horizontale Gegenflächen bei null Kontaktdicke paarweise entfernen, sonst ablehnen. Bauteilschluss bleibt streng.
Unabhängige Projektion tatsächlicher Außendreiecke auf jede Quellfassade erkennt Flächenüberlappung; alte Vollplatte als Gegenfall wieder einfügen.
[`roof_recipe.py`](../test/experiments/buildings/roof_recipe.py): ursprüngliche Eigenschaften/Quelle behalten, Körperoberkante als eigene Dachreservierung.
Typ/Höhe/Winkel/Richtung/Orientierung/Dachfarbe aus direkten Angaben; fehlende Dachform aus Typ/Größe/Höhe oder explizitem Experiment.
Original-OSM-Dachfelder sind optionale Kontrollen, keine Behauptung über ihren OpenFreeMap-Lieferumfang; Unsupported/Konflikte mit Grund ablehnen.
32 konstruktive Dach-/Körperkombinationen auf acht Grundrissen, zwölf ausgelassene Gegenkombinationen; reine Dachkomponenten, keine fertigen Architekturfamilien.
[`verify_roofs.py`](../test/experiments/buildings/verify_roofs.py): tatsächliche Dachdeckung, geschlossene Bauteile, Gesamthöhe/Höfe und vorhandene Körperwege prüfen.
[`roof_probe.py`](../test/experiments/buildings/roof_probe.py): echte Mesh-Schnitte, 2.5D und äußere Nah-/Gegen-/Distanzansichten.
Mesh-Schnitt enthält auch Kanten/Faces genau auf der Ebene; sonst verschwanden First und Deckenabschnitte in den Diagnosebildern.
Gleiche Kamerakonstruktion/Licht wie Esri-Candler; Candler ergänzt oberhalb seines Höhenparameters, eigener Dachraum liegt innerhalb der Quelle.
Distanzbilder zunächst vollständige Detailreferenzen; kein freigegebener Fern-LOD, keine vollständige Rezeptfamilie oder Nachtlichtabnahme.
24 Kombinationen auf sechs Grundrissen: 72 Nah-/Gegen-/Distanzbilder und 24 Plan-/Schnitt-/2.5D-Vergleiche optisch geprüft.
Synthetische Außenansichten 1280×720, reale Esri-Vergleichskameras 854×480; dieselben gespeicherten Kamera-/Lichtbedingungen.
Bilddefizit: generische Fensterachsen ohne Rahmen/Erdgeschossgliederung/Dachranddetails; geneigte Dachflächen in Eevee zu schwach lesbar.
Blender-Import von Koordinaten, Materialien und geneigten Dachnormalen geprüft; diagnostische koplanare Triangulationskanten im Schnitt bleiben sichtbar.
Aktiver Fassadenausbau: Fenster-/Eingangsrahmen und Fensterbank am vorhandenen Öffnungs-/Raumplan; keine unabhängigen Fassadenöffnungen.
Nächster Ausbau: teilnutzungsbezogene Ladenfronten am vorhandenen Erdgeschoss-/Raumplan; obere Geschosse und übrige Wohnräume erhalten.
Lokale POI-Nutzung nur dem tatsächlichen Host zuordnen; fehlendes Geschoss als prozedurale Erdgeschossannahme kennzeichnen.
Breite/niedrige Schaufenster mit vorhandenen Wanddurchbrüchen/Gläsern/Rahmen konsistent erzeugen; keine zweite Öffnungswelt.
Candler-Grundgeschoss als Formreferenz, `envelope2002` für weitere nachweisbare Nutzungs-/Grundrissrezepte prüfen (2400).
Metrische Ringextrusion gegen gleich geformte Balkenbauteile vergleichen; Template-Wiederverwendung und Platzierung statt erneuter Polygonarbeit je Öffnung.
Konstruktive Kontakte, freie Türöffnung, Körperhöhen/Quellfarbe und tatsächliche Module erst in 2D/Schnitt, dann 2.5D/Nah/Gegen/Distanz prüfen.
`opening_modules` liefert Sash/Einfassung/geneigte Bank und außenliegende Eingangsrahmen; `opening_details` hält stabile Slots/Room-IDs und wiederverwendet Templates.
Fenster-Ringmodule 88 statt 120 Balkendreiecke, gleiche analytische Volumen/Öffnungen; 48 Öffnungen × 21 warme Läufe vergleichen, kein nativer Framebeleg.
Sechs gespeicherte Körperpläne/676 Bauteilplätze gegen tatsächliche Paneelen und freie Türbreiten prüfen; versetzte Öffnungsbeschreibung als Gegenfall.
Glasdiagnose verwendet dünne Alpha-Oberflächen mit vorhandenen Innenräumen; Refraktion, Raumprogramme/Nachtlicht und freigegebene LODs offen.
Historische EEVEE-Nahkontrolle 1280×720/48 Samples: Dithered körnig, Alpha-Blending glatter; Innenboden blieb körnig.
121/343/329 s entstanden mit Kamera auf Beleuchtungszentrum statt Öffnungsziel; keine gültigen Nah- oder Auswahlbelege.
Separate Glasgeometrie zeigte damals keinen zusätzlichen Bildgewinn; aktuelle Glas-/Schattenkontrollen und Kameravertrag in 2460.
Sechs vollständige Körperansichten bleiben gültig; Fenster-/Eingangs-/Kompositbilder sind im Showcase rot bis zum korrekten Ersatz.

## Familien und Komponenten
- Haus/Bungalow/Reihenhaus: Geschosse/Räume/Eingang, Fensterachsen und Dach-/Traufkonstruktion.
- Wohnblock/Büro/Hotel/Mischbau: Höfe/Parts, Erschließungskern, gegliederte Fassaden/Teilnutzungen.
- Laden/öffentlicher Bau: Schaufenster/Portale, Säle/Korridore und passende getrennte Eingänge.
- Halle/Lager/Hangar/Garage: Tragstruktur, Tore/Ladezugang, große Innenflächen und passende Öffnungen.
- Sakralbau/Gewächshaus, Silo/Tank, Schornstein/Wasserturm und weitere gelieferte Sonderklassen:
  passende Körper/Nutzung/Wartung/Details; keine Wohnfassade auf jedem Typ.
- Flach-/Pult-/Sattel-/Walm-/weitere belegte Dächer: Löcher/Parts, Traufe/First/Parapet/Entwässerung/Zugang.
- Sockel/Geschossbänder, Rahmen/Leibungen/Stürze/Fensterbänke/Brüstungen, Türen/Tore.
  Balkone/Vordächer/Außenfluchten/Dachaufbauten funktional an den gemeinsamen Plan anschließen.
- Materialien/Proportionen kohärent je Familie; Quellfarbe und Herkunft ergänzter Architektur erhalten.

## Umsetzung und Abnahme
Eine Familie vollständig über 2D → 2.5D → Tag/Nacht/Nah/Gegen/Fern, dann nächste Bildlücke wählen.
Form-/Teilungs-/Komponentenverfahren bei gleichen Anforderungen vergleichen; gültige einfache Rezepte behalten.
Mehrere reale Eingaben je Familie, passende Esri-Referenzen bei gleicher Kamera/Licht optisch prüfen.
Varianten nach tatsächlich verschiedenen Form-/Bauteil-/Raumkonfigurationen zählen, nicht nach Seeds/Instanzen.
Konstruktive Funktion, Bildgewinn und Erzeugungs-/Detailkosten gemeinsam prüfen; Tausende Varianten bleiben Ziel.
Fremde Modelle/Code bleiben externe Referenzen (2400); eigener Generator unter `test/experiments/buildings/`.

## Quellen
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
[Instant Architecture, SIGGRAPH 2003](../doc/references/buildings/siggraph/2003-instant-architecture.pdf): Fassaden → Bauteile, affine Grundformen; Seiten 3/4 optisch geprüft.
[Held/Palfrader 2016](../doc/references/buildings/geometry/2016-weighted-skeleton-roofs-and-terrains.pdf): Dachflächen aus Wavefront; Seiten 1/6/8 optisch geprüft, CGA-Shape-Seite 4 ebenso.
[Python-Skeleton-Bibliothek](https://pypi.org/project/py_straight_skeleton/),
[Esri Walm](https://doc.arcgis.com/en/cityengine/latest/cga/cga-roof-hip.htm),
[Esri Sattel](https://doc.arcgis.com/en/cityengine/latest/cga/cga-roof-gable.htm).
[OSM Dachrichtung](https://wiki.openstreetmap.org/wiki/Key:roof:direction): Gefälle, keine Firstachse; Grad/16 Kompassrichtungen.
[Esri-Pakete c9ebe92](https://github.com/Esri/pyprt-examples/tree/c9ebe92f27f6355179c9d7792f430a3c133b9214).
Gemeinsame Arbeits-/Bildregeln in AGENTS, Gebäude-/Quellvertrag in 2173.
