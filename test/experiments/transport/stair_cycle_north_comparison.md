# Nördlicher Radweg und darstellungssichere Kontaktflächen (#434)

Dieselbe begrenzte Versuchsfolge verarbeitet jetzt eine zweite feste
[Planungseingabe](fixtures/stair-cycle-north-vienna.json). Sie ergänzt den ganzen
ursprünglichen Radweg `39881120:23` zum [19-Quellenstand](stair_cycle_comparison.md).
Quellen-/DEM-Auswahl und geschützte Profile sind Instanzdaten; eine Straße erzeugt
keinen neuen Katalogeintrag. Kein gemeinsames Rezeptformat oder Universalsolver
wird daraus abgeleitet; den Indexvergleich und die gemeinsame Entscheidung besitzt #483.

Die Achsen teilen zwei ursprüngliche MVT-Vertices. Der vorhandene Fachplan nutzt
2158/1789 als echten Endkontakt; die zusätzliche gemeinsame Bandfläche bleibt eine
ausdrückliche Planentscheidung. Der interne gemeinsame Vertex 2677/2591 wird nicht
pauschal zu einem gemessenen Straßenknoten erklärt. Bisherige Achsen, Priors und
vollständige Quellattribute bleiben erhalten; gemeinsame Höhen und Terrain sind neu.
Das nördliche alte Außenprofil ist ganz gebunden; sechs wirkliche Außenprofile
bleiben offen (vier bisherige und zwei neue). Keine globale/native Freigabe.

## Tatsächliche Gegenfälle und Verfahren

Der frühere Zellen-/Kontaktkantenschnitt erzeugt am neuen Eingang zwei reale
Float32-Probleme. Drei schmale Bodenfacetten bestehen Double, überschreiten nach
Rundung aber die Erdgradientengrenze. Darüber hinaus dreht eine lange zusätzliche
Kontakt-Unterteilung ihre projizierte Orientierung um. Solverstatus oder ein
geschlossener Double-Körper genügen deshalb nicht. Beide Gegenfälle bleiben
abgelehnt, keine Erde wird als konstruktive Fläche außerhalb ihres Bereichs umetikettiert.

Die neue Zerlegung vereinigt sämtliche Zellen am Wegegrundriss zu einem Bodenband
und trianguliert dessen Differenz zum ursprünglichen Wegegrundriss. Ferne Zellen
bleiben regelmäßig. Ganze Quellkontaktfacetten und ihre tatsächlichen Kanten bleiben
unverändert; nur die freien Bodenflächen werden konform zerlegt/verfeinert.
Die gespeicherten tatsächlichen Double-/Float32-Höhenfelder und sämtliche alten
Kontakt-/Neigungs-/Unterführungsgrenzen bleiben verpflichtende Abnahme.

Die eigene Rundungsreserve verwendet tatsächliche Float32-XY und einen ausdrücklich
beschränkten Höhenbereich: Extremwerte von DEM/Pins plus/minus 8 m. Für jede Facette
werden die acht extremen Vorzeichenkombinationen des halben größten Float32-ULP
durch ihren affinen Gradientenoperator Gq abgebildet. Ihr größter Normwert ist E.
Für den Double-Operator Gd gilt über die affine Ebene `Gq*h = T*(Gd*h)` mit
`T = Gq*XY_double`; die konstante Höhe fällt heraus. Die Spektralnorm von T liefert
zusammen mit der vorhandenen Double-Reserve einen Ausschlusstest für zusätzliche
Kegelbedingungen. Beide Enden des erlaubten konstruktiven Zuschlags werden geprüft.
Die selektive Variante fügt nur nicht bereits implizierte Bedingungen hinzu;
die vollständige Variante prüft alle. Gleiche feste Vernetzung, Höhenintervalle,
Gradientengrenzen, Ziele und Zeit-/Speicherlimits; tatsächlicher Export bleibt
entscheidend. Keine Konvergenz oder globale Optimalität aus MaxIterations ableiten.

SIGGRAPH-Recherche zuerst: Deering 1995, [Autor-PDF](https://www.michaelfrankdeering.com/Projects/HardWare/CompressedGeometry/SigGeometryCompression.pdf),
[lokales PDF](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf),
S. 3 §4 optisch geprüft: lokale Koordinaten und bewusst begrenzte Positionspräzision.
Die Autorenfassung enthält keine Ergebnisfotos; daraus wird kein Bildvergleich
übernommen. Die hier verwendete Gradienten-/Würfelabschätzung ist eigene Arbeit,
keine behauptete Formel dieses Papers. Membranenergie und Profil-/Terraintrennung
bleiben wie in den vorigen Vergleichen belegt. Kein Fremdcode oder SDK in der Runtime.

## Reproduktion und offene Abnahme

[Aufrufe](README.md). Der alte 19-Quellen-Eingang bleibt bytegleich und benutzt
weiterhin seine bisherigen Operatorvorgaben. Für neue gewöhnliche Bodenquellen
gelten harte Anzahl-/Profilgrenzen; indoor, Brücken/Tunnel und andere Ebenen werden
aus diesem gewöhnlichen Rezept ausdrücklich abgelehnt. Räumliche/native APIs
bleiben unverändert. Ein erfolgreicher vollständiger Export, unabhängiger Leser,
tatsächliche Bildprüfung und gleiche Kostenkontrollen werden im PR belegt.

Übrige Quellen/Ebenen, Stationsrezept, Fahr-/Gehhüllen, Radien, Statik, allgemeine
Selbstschnittfreiheit und native Übernahme bleiben offen; #434 bleibt aktiv.
