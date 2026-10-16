# Wirkliche Treppe im Wiener Vierwegefall (#434 / #400)

Der lokale Versuch erhält die vier tatsächlichen Quellbänder einschließlich
Rückbrücke und erzeugt zwölf Tritte/Setzstufen mit gemeinsamen flachen Podesten.
Er ist noch keine Wiederanbindung an die vollständige Stadt: Die beiden äußeren
Profile ändern ihre Höhe und bleiben ausdrücklich vorgeschlagen. #434 bleibt aktiv.
Native sichere Quellauslassung und Weltgegenprüfung besitzt #400 bei master.

## Eingang und räumlicher Vertrag

Unverändertes [Originalfixture](../../outshine/data/transport/wien-junction-sources.json),
SHA256 `edf7f4ec5cb50b533c0c041e7a6008e16587be09b89d9ed93c835eab44f91719`:
Treppe377113090/native7683, Radweg97041640/7493, lange Brücke229812320/8463,
Rückbrücke1013938082/8431. Alle ursprünglichen MVT-Geometrien, Properties,
Kachel-/Payload-/Quellzuordnungen, nativen Stationen und Höhen bleiben erhalten.
OpenFreeMap/OpenMapTiles20260927_080001_pt, Kachel14/8937/5678,
Payload97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8.

Die lokale Achsumrechnung benutzt genau die nativen111132m/Lat-Grad und
111320*cos(Referenzbreite)m/Lon-Grad; die mitgelieferte metrische Rückbrückengeometrie
stimmt bis1e-10m überein. Das ist dieser lokale Frame, keine allgemeine geodätische
Genauigkeitsbehauptung. Quellhöhen sind Geländevorgaben, keine fertigen Porthöhen
und kein vollständiges ursprüngliches DEM. Der erste Treppenknoten behält den
165.2197113037-m-Datumwert; sonst wirken Höhen als weiche L1-Priors.

Ganzes vorgeschlagenes Radweg-Außenprofil:165.046100–165.054196m statt der
165.929016-m-Mittellinienvorgabe; Änderung−0.882916 bis−0.874820m.
Langer Brückenanfang:164.237083–164.237748m statt164.521042m;
Änderung−0.283959 bis−0.283294m. Beide dreipunktigen Querschnitte mit Original-ID
stehen in `outer_ports`, Status `proposed_pending_actual_neighbour_contact`.
Mit beiden Quellprior-Höhen zusätzlich hart fixiert ist dieses endliche Modell
unvereinbar. Das beweist keine allgemeine Unmöglichkeit des Originalnetzes.
Tatsächlich gewählte Nachbar-/Bodenflächen wurden bei #400 angefragt; deren Prüfung
und eine nötige Erweiterung des örtlichen Planungsbereichs bleiben Teil von #434.

## Endliche ganze Flächen und tatsächliche Stufen

Chen et al., SIGGRAPH2008, S.2/Fig.2 trennt Graph und Geometrie; Galin et al.,
Eurographics2010, §6/Fig.17–18 trennt Profile, Bauwerke und Terrainänderungen.
Die lokalen PDFs/Abbildungen sind optisch geprüft, GalinS.8 hier erneut geöffnet;
[Primärquellen](../../../doc/references/README.md). Die folgende finite Herleitung
ist eigene Arbeit. Keine SDK-Aufrufe, Dekompilationsübernahme oder SDK-Runtime.

Eine gemeinsame Knotenanordnung enthält tatsächliche Bandränder, Querschnitte,
Quellachsen und Podestgrenzen:396XY-Knoten/576Flächen/1067Segmente, kein numerisches
Schweißen dieser Grundanordnung. Eigene Höhenidentität pro Quellfläche; nur echte
gemeinsame Podeste vereinen sie. Die Radweg-/Brückenüberlappung bleibt getrennt.
Je Facette ist die Höhe affin. Alle drei Radflächen brauchen Gradientennorm≤0.06,
die glatte Treppen-Gradiente≤0.50. Ihre Werte bleiben zwischen den Podesten und
sind in den tatsächlichen Quellabschnittsrichtungen monoton. Treppenquerschnitte
bleiben über die Breite auf gleicher Höhe; Knicke ergeben zusammenhängende Wender.

830Variablen,18327Ungleichungen,54047Sparse-Nonzeros.16gerichtete Halbebenen mit
Radiusfaktor cos(pi/16) sichern die tatsächliche euklidische Gradientengrenze,
zusätzliche Baureserve0.0002 bei Radflächen beziehungsweise0.002 an der Treppenhülle.
Alle Überlappungsecken sichern2.82m zwischen Verkehrsoberflächen;0.30m obere
Körperdicke lässt mindestens2.52m lichte Höhe. Höhenbudget relativ zum Datum−2/+10m;
max4096Höhenknoten/8192Facetten, LP-Zeitlimit5s; Erzeuger/Reader3GiB/60CPU-s,
Kostenworker3GiB/90CPU-s.
Der alte zusätzliche4-m-Kern ist auch bei freien äußeren Priors unvereinbar;
an tatsächlichen Querschnitten bemessene Podeste plus0.75m örtlicher Zusatz passen.
Keine Aussage, dass dieses Modell die nativen Solveroperanden exakt reproduziert.

Die erste Double-Lösung überschritt nach tatsächlichem Float32-Export6%:
6.315156% auf einem nur wenige Millimeter schmalen Rückbrückenfacet. Der gemessene
Dreipunktfall ist als Gegenkontrolle gespeichert. Ganze Float32-XY-Koordinaten
werden deshalb bereits im LP berücksichtigt. Für jede Richtungszeile `c` gilt
`c*h <= G*cos(pi/16) - epsilon*sum(abs(c))`; epsilon ist die maximale halbe
Float32-Höhen-ULP im ganzen gewählten Höhenbudget. Damit bleibt auch jede mögliche
unabhängige Höhenrundung innerhalb der Normgrenze. Gemeinsame flache Podeste haben
eine identische Exporthöhe; ihre Rundung erzeugt keine Neigung.

Die Gradiente ist noch keine Treppe. Ihre horizontalen Höhenbandschnitte werden
zu zwölf wirklich ebenen Tritten, vertikalen Setzstufen und zwei flachen Endpodesten.
Maximal erlaubte Steigung der Hülle50%, Setzstufe18cm; Konstruktion nutzt17.9cm als
Rundungsreserve. Tatsächlich17.182397cm, quantisiert maximal17.182922cm. Jede
zusammenhängende Trittfläche besitzt Platz für eine0.28-m-Scheibe; kleinste Fläche
zulässiger Scheibenmittelpunkte nach Erosion0.122923m² (JSON-Schlüssel
`minimum_support_disk_area_m2`, kein Flächenmaß der Scheibe selbst).
Das ist keine Treppen-Norm-, Handlauf-, vollständige Geh-/Fahrhüllenfreigabe.
Treppen sind kein behaupteter befahrbarer Radweg; ursprüngliche Modustags bleiben.

Die kontinuierliche Unterseite folgt der Gradiente−0.30m. Äußere Seitenwände
werden an allen vertikalen Trittgrenzen geteilt: einfache Quads hatten echte
T-Verbindungen, die geschlossene Indexprüfung ablehnt. Aktueller Körper1060Vertices/
2124Flächen,709obere Facetten/124Setzstufenfacetten, Volumen271.995119m³.
Level-Schnittschweißen höchstens8.89e-16m; keine kollabierte/invertierte
Float32-Fläche. Tatsächliche Float32-Radnormen maximal0.05981618/0.05981342/0.05985377.
Ganze Durchfahrt Double2.52m, Float32 2.519989014m; ganze gemeinsame Podestabweichung0.

## Prüfung, Kosten und Bild

Freeze für Erzeuger/Reader/Bild/Kosten:`930054f4371ac5075dee49ce09432c54cbfed652`.
Python3.13.5, NumPy2.2.4, SciPy1.18.1/HiGHS1.12.0, Shapely2.2.0/GEOS3.14.1.
Acht Kontrollen: echter Vierwegekörper/Float32-Durchfahrt, alter Kern, zusätzliche
harte Porthypothese, echter Double-/Float32-Gegenfall, analytisch geschlossene zwei
Stufen, verlorene Quell-Coverage, zweites LP-Verfahren und native metrische Achsen.
Der gespeicherte Reader regeneriert keine Kandidatengeometrie und ruft keinen
Solver auf. Er prüft Körper-/Kappen-/Wandbesitz, alle vier ganzen Quellenflächen,
gespeicherte Hülle gegen Solverfeld, Podeste/Monotonie, tatsächliche Float32-Grenzen,
Trittflächen/Setzstufen, Durchfahrt und vorgeschlagene Außenprofile. Tatsächliches
Darstellungsmesh, Indizes, Normale und Farben werden separat gebunden.

Frischer vollständiger lokaler Worker einschließlich drei Varianten/Audit/NPZ:
1.319225s,123404KiB Peak-RSS. Körper-NPZ SHA256
`d856b9d73ee6fcfa559f5bb5110796b8a491868114b5a2fd9b5c72e6b047d9a0`.
Gleiche Bedingungen/akzeptierte Körperqualität: HiGHS-Dual-Simplex und Interior-Point
erreichen dieselbe L1-Zielfunktion. Elf normale Wiederholungen nach warmen Importen
und Solves, Prozesse nacheinander ohne eigene Renderer, BLAS/OMP je1: Median
415.088/812.562ms für Bedingungsaufbereitung plus LP; Dual-Simplex gewählt.
Beide651888Sparse-Bytes/3296Resultat-Bytes; explizite Float32→Double-XY-Kopien9888B.
Normale Worker-RSS127471616/127627264B. Separate Memray1.20.0-Aufnahmen:
1441825/1439662Allokationsereignisse,212589967/299480342angeforderte Bytes,
43494843/43429447B getrackter Heap-Peak; instrumentiert1844.872/2264.194ms.
Aufbereitung/LP-Zeiten stehen getrennt im Rohbeleg; keine Totalzählung interner
Kopien, kein günstiger generischer Welt-/Native-/Framepfad behauptet. Vor einer
allgemeineren Bibliotheksübernahme sind weniger Variablen/temporäre Objekte nötig.

Tatsächliche500-m-Gesamtansicht und42-m-Konstruktionsdiagnose1280×720 geöffnet,
EEVEE5.2.2/llvmpipeLLVM19.1.7/16Samples, zwei Threads/nice10:3.734/4.407s,
1724808/1724896KiB Peak-RSS. Die Diagnose zeigt echte Tritte am Quellenknick;
die Gesamtansicht bleibt wegen der langen Brückenachse schlicht. Keine neue
Präsentationsgalerie oder erfundene umliegende Stadt/DEM. Lokale kanonische Belege:
`/tmp/outshine-transport-stair-434-result-930054f43`, getrennte Kosten unter
`outshine-transport-stair-434-cost-930054f43`. Verfahren und reproduzierbare Aufrufe
im [README](README.md); Außenkontakte, Terrainänderungen, Stützen/Statik und
vollständige native Rückgewinnung der ausgelassenen Quelle bleiben offen.
