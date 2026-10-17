# Begrenzte horizontale Transportkurven (#535)

`outshine_ref.transport.clothoid_turns.fit_symmetric_turns(node, trim,
start_heading, end_heading, tolerance=1e-7, transition_fraction=0.5)` ist der
eigene bisherige Experimentkern für symmetrische Gerade–Klothoide–Kreis–
Klothoide–Gerade-Verbindungen. `sample_symmetric_turns(fit, stations)` liefert
XY, Richtung und vorzeichenbehaftete Krümmung. Keine Experimentimporte im Paket;
die bisherigen Experimentnamen leiten auf denselben installierten Kern weiter.

Die Kurven beginnen/enden mit Krümmung null. Ihr Krümmungsverlauf ist eine
lineare Rampe, optional konstante Kreiskrümmung und eine lineare Rückrampe.
`transition_fraction` liegt in `(0, 0.5]`; bei 0,5 gibt es keinen Kreisteil.
Für Länge L, Richtungsänderung d und Rampenanteil t gilt
`k_max = abs(d)/(L*(1-t))` und `k'_max = k_max/(t*L)`.
Endposition und Richtung bestimmen die positive Länge durch symmetrische
Integration. 16-Punkt-Gaußquadratur, unabhängige 64-Punkt-Schlusskontrolle;
deren Differenz ist ein gemessener numerischer Kontrollwert, kein formales
Intervallzertifikat. Tests vergleichen zusätzlich mit unabhängiger ODE-Integration
des vorgeschriebenen Krümmungsverlaufs.

Ein oder bis zu 4096 unabhängige Fits pro Aufruf. `trim`/Richtungen sind
gleich lange eindimensionale Arrays, `node` ein XY-Punkt oder je Fit ein Punkt.
Finite metrische lokale Koordinaten/Trims bis 1e6, positive Trims/Toleranz.
Ungültige geometrische Fits erhalten `valid=False`; Sampler verlangen gültige
Fits. Höchstens 65536 Fit-/Stationspaare, Stationen in `[0,1]`.
Gaußordnungen 16/64 sind feste interne Arbeit; Quadraturtabellen read-only.
Fit-/Samplerarrays sind eigene Ausgaben; Eingabemutation verändert den Fit nicht.
Der Aufrufer darf seine Fitdaten verändern und trägt deren Konsistenz selbst.

`outshine_ref.transport.horizontal.plan_horizontal_axis(original, recipe)`
komponiert lokale Kurven zwischen den Geraden einer **offenen** ursprünglichen
XY-Achse. Beispiel:

```python
import numpy as np
from outshine_ref.transport.horizontal import plan_horizontal_axis

axis, notes = plan_horizontal_axis(
    np.array([[-100, 0], [0, 0], [0, 100]], dtype=float),
    {"model": "clothoid", "radius_m": 30.0, "error_m": 0.025,
     "end_guard_m": 8.0, "maximum_shift_m": 30.0},
)
```

Das Rezept enthält genau diese fünf Felder; alternative Modellwahl `circle`.
Radius 4–1000 m, Chordfehler 0,0001–0,1 m, positive erhaltene Endgeraden und
maximale Verschiebung bis 1000 m. Radius und Verschiebung sind prozedurale
Konstruktionsparameter. Ein Kreisbogen ist G1 und behält an den Übergängen
Krümmungssprünge; die Klothoidenwahl ist G2, mit Rampenanteil 0,25.
Der Kreisbogen ist ein Vergleich, kein G2-Ersatz.

Maximal 128 Originalpunkte, wenigstens 1 µm lange Segmente, lokale Koordinaten
bis 1e6. Rückwendungen ab 150°, geschlossene Achsen, unzureichende Nachbarlängen,
Selbstschnitt, Verschiebungs- oder Abtastbudgetüberschreitung werden abgewiesen.
Der Radius wird bei Platzmangel ausdrücklich nicht verkleinert. Beide
Nachbarkurven teilen die vorhandene Gerade; Anfang/Ende und mindestens die
gewählte Endgerade bleiben erhalten. Pro Kurve höchstens 240 Segmente,
abgeleitete Gesamtausgabe höchstens 256 Punkte. Kurven werden nacheinander
gebildet, der Gesamtumfang vor Zusammenfügen geprüft; kein globaler Optimierer.

Chordbound `k_max*(L/n)**2/8` aus der ganzen zweiten Ableitung der nach Bogenlänge
parametrisierten Achse; numerische Quadratur-/Koordinatenfehler bleiben getrennt.
Alle Kurvenintervalle dicht gegen den Bound kontrolliert; keine bloße
Endpunktprüfung. Ausgabeachse besitzt ihren eigenen read-only-Speicher;
Notizen enthalten frische Rezeptwerte, Originalknotenindex, Trim, Länge,
Krümmungsgrenze, Abtastumfang und numerische Schlusskontrolle.

**Der Kern liefert eine geometrische Achse, kein fertiges Verkehrsnetz.**
Original-IDs, gemeinsame Knoten, Ebenen, Verkehrssinn, Fahrstreifen, Fahrzeug-
hüllen, endliche Flächen, Höhen/Querfall, Reservierungen und LOD besitzt der
Aufrufer. Ein Kurvenzug schneidet regelmäßig die ursprüngliche Ecke ab;
Originalknoten können weitere Quellen anschließen. Der Wiener Verbraucher
erhält deshalb ausdrücklich die ganze ursprüngliche Straßenfläche und ergänzt
die Kurvenfläche. Originale und abgeleitete Geometrie bleiben getrennt.
Er prüft tatsächliche Double-/Float32-Körper, ganze Straßenprofile/Terrainkontakte
und ganze horizontale Kompaktwagenhüllen innerhalb ihrer jeweiligen Fahrspur.
Keine allgemeine Kreuzungsroute, 3D-Chassis-, Verkehrsregel-, dynamische
Lenkungs-/Geschwindigkeits- oder Statikabnahme.

Quellen: zuerst lokale SIGGRAPH-Referenz Chen 2008 (Graph-/Geometrie-Trennung),
ergänzend Galin et al., *Procedural Generation of Roads*, Abschnitt 6,
S. 8/Figuren 17–18, Text und PDF optisch geprüft. Lokale PDFs unter
`doc/references/transport/`; die konkrete begrenzte symmetrische Konstruktion
und ihre Implementierung sind eigene Verfahren, kein übernommener Fremdcode.
Keine SDK-Runtime. Methoden-/Real-/Wheel-/Bild-/Kostenvergleich besitzt #535.
