# Physische Wasserränder zwischen lokalen Frames (#941)

Für den vorhandenen begrenzten Höhenkörper gewinnt **ein gemeinsamer physischer
Fit, danach konforme Ausschnitte**. Cachetreffer laden nur den eigenen Ausschnitt.
Dies ist eine Python-Entscheidung unter #760, keine neue native Cachearchitektur,
globale Flussdomäne oder Geräte-/Rasterabnahme.

## Eingabe und Ergebnis

Der Produzent besitzt einen versionierten, bereits geprüften Körper: Float64-ENU-
Mesh, WGS84-Anker, ursprüngliche Quellen/Zuordnungen, Höhenmodell und physische
Neigungsbedingungen. Ein Schnitt liegt ausdrücklich bei `x = plane_x` in diesem
kanonischen Frame. Schnittpunkte entstehen einmal je ursprünglicher Kante mit
geordneten Endpunkt-IDs und einem gemeinsamen affinen Parameter. Kein Weld-Radius.
Beide Hälften übernehmen dieselben geraden Teilkanten und die Elternfacetten.
Der vorhandene Solver und PROJ-Framewechsel bleiben unverändert.

Jedes Ergebnis enthält eigene lokale Vertices/Dreiecke, Frameanker, Elternfacetten,
Quell-CSR/Originalindizes und den gemeinsamen Rand: geordnete IDs, Teilkanten,
kanonische ECEF-Punkte und identische Version. Die privaten NPZ-Dateien demonstrieren
diesen Eingabe-/Ergebnisvertrag; sie definieren kein Produktformat. Ihr Inhaltsdigest
kontrolliert die Versuchseingaben. Produktive Versionierung muss zusätzlich Rezept,
Datum, Quelle und Körperbesitz binden; das bleibt beim nativen Besitzer.

Die andere Variante übernimmt dieselbe konforme Konnektivität, setzt den vorgegebenen
Rand vollständig in lokalem XYZ und fittet die Innenhöhen separat. Andere lokale XY
stammen aus dem ursprünglichen weichen 3D-Zielkörper; die Innenprioren erhalten
−0,25/+0,25 m. Der gemeinsame Rand kommt hier aus dem ersten Fit und ist eine
deklarierte Modellvorgabe. Der Versuch löst keine unabhängige Nachbarkonsensfindung
und bewertet keine beliebigen anderen Randtriangulierer.

## Geometrische Prüfung

Für gleiche Teilkanten ist die ECEF-Differenz entlang der Strecke eine konvexe
Kombination der Endpunktdifferenzen. Deren größter Betrag begrenzt die ganze Kante
in exakter Arithmetik. Zusätzlich werden 17 Punkte je Teilkante ausgewertet;
deren Float64-Interpolationen enthalten weitere Rundung. Nur gleiche Endpunkte bei
unterschiedlicher Tessellierung reichen nicht: der geografische Mittelpunkt eines
0,01°-Längenspans bei 47,23°/500 m liegt rund 16,5 mm neben der ECEF-Sehne.

Die ursprünglichen Normalenhüllen werden ohne neue Richtungsprojektion vererbt.
Alle drei rekonstruierten Kind-Ecken müssen in der ursprünglichen Kugel und
Up-Spanne liegen. Deren konvexer Schnitt enthält dann die ganze Facette.
`grade_envelopes` prüft die tatsächliche Kind-Normale mit den ursprünglichen
Richtungen/Intervallen; bestehende 1e−9- und QP-1e−7-Grenzen bleiben bestehen.
45 baryzentrische Punkte je Facette liefern zusätzlich unabhängige PROJ-Grade.

## Ergebnis auf den Originalen

Hash-geprüfte Eingaben: `fixtures/feldkirch-ill-native-inputs.json` und die dort
gebundenen WATER2-/Metadatenbytes, unverändert. Linux/GEOS-Topologie: 1492 Vertices,
1492 Facetten, ursprünglich 1607 positive Quellmitgliedschaften. DEM-abgeleitete
Kurspegel sind weiche Modellprioren, keine beobachteten Wasserstände/Fließrichtungen.

| Prüfung | Gemeinsamer Körper/Schnitt | Separate Fits am gleichen Rand |
|---|---:|---:|
| Neue Facetten links/rechts | 756/754 | 756/754 |
| Randpunkte/Teilkanten | 10/9 | 10/9 |
| ECEF-Randabstand, Endpunkte und 153 Streckenproben | 0 m | 0 m |
| Größter neuer Diederwinkel innerhalb alter Facette | 4,42e−8° | 0,316661° |
| Vererbte Whole-face-Hülle, maximale Überschreitung | 0 | 3,29e−6 |
| Gesampelte Verletzungen ursprünglicher Grade | 0/203445 | 0/203445 |
| Tatsächliche XY-Grenze: Hausdorff zur Quellunion | 6,27e−10 m | 1,141e−4 m |
| Außerhalb/fehlende Quellfläche nach Frame-Rückweg | 1,50e−6/1,64e−6 m² | 0,24439/0,24998 m² |

Beide behalten alle 13 Quellen, eine Insel und 1653 **neu berechnete** positive
Mitgliedschaften; alte CSR wird nicht ungeprüft auf Schnittkinder kopiert.
Die Insel bleibt bei 352,57387094 m² gegenüber 352,57387093 m² Quellenfläche.
Vor dem Frame-Rückweg beträgt die größte Eltern-Flächenabweichung 1,71e−13 m².
Separate Fits bestehen ihre eigenen regionalen QP-/Hüllenprüfungen. Die fehlende
vererbte Hüllenzertifizierung beweist keine tatsächliche physische Gradeverletzung;
die Stichproben finden keine. Geschlossener Rand beweist außerdem keine gleiche Normale.

Weitere ganze Fits: −50 m Modellhöhe, Datumsgrenze bei 10° Breite und 89,5° Breite
mit lokalen Längenankern 10°/30°. Alle acht gemeinsamen Ausschnitte bestehen die
vererbten Hüllen, maximaler gesampelter Randabstand 7,28e−12 m. Der alte Ansatz
„XY bei Höhe 0, danach Up fitten“ liefert separat 11,399 cm an den 242 originalen
Kursendpunkten, 17,453 cm an der Datumsgrenze und 3,031 m im Hochbreitenkontrollfall.
Das sind gemessene Gegenbeispiele, kein Nachweis eines Fehlers im aktuellen Cooker.

## Kosten und Grenzen

Ein Schnitt besucht 1492 Facetten/8952 Halbraumkanten; zehn Intersektionen werden
einmal erzeugt. Die Clipping-Hauptschleife besucht jede Facette konstant oft;
`np.unique`/Sortierung und die lokale ID-Abbildung verursachen zusätzliche Arbeit.
Eine pauschale O(F + B)-Gesamtschranke ist damit nicht belegt. Gemeinsame Randdaten
benötigen O(B); beim Lesen ist kein Nachbar-Fit nötig. Eingabecaps: 8192 Vertices/16384 Facetten, Randcap 1024;
unbrauchbare Projektion, degenerierter Schnitt oder Budgetende werden lokal abgelehnt.
Unvereinbare harte Kontakte liefern keine Geometrie; gesunde andere Produkte bleiben
ladbar. Die zwölf Kontrollen prüfen auch diese Fälle und veraltete Randversionen.

Der Schnitt hält 60440 Arraybytes, Python-Tracing-Peak 605060 B/134 verbleibende
Allokationsblöcke im aufgezeichneten Lauf. Zwei fertige Originalprodukte halten
zusammen 80692 Arraybytes, davon je 648 B Randdaten im privaten NPZ-Layout;
die finalen `Mesh`-Eigentumskopien betragen mindestens 54408 B. Vollständige temporäre
Kopier-/Allokationszahlen und native Allocator-Peaks bleiben ungemessen.
Linux/i5-1135G7 unter gleichzeitiger interaktiver Last: getracter gemeinsamer Fit
1,33 s/2,44 MB Peak, Schnitt 0,707 s; separate Zusatzfits 0,492/0,486 s mit je
1,25 MB Peak. Diese einzelnen instrumentierten Python-Zeiten sind kein nativer
Geschwindigkeitsvergleich. Tracing misst Funktionsarbeit, Snapshot-Zählen folgt danach;
Audit/Plots/Schreiben liegen außerhalb dieser Einzelzeiten.

Ein bewusstes Float32-Narrowing lokaler Positionen erzeugt trotz Double-Framewechsel
wieder 0,033 mm Randabstand am Original und 0,659 mm im Hochbreitenfall. Die native
Darstellung muss den gemeinsamen Rand bis zur gemeinsamen Kameraraum-Konvertierung
erhalten; GPU-Bindings, LOD-/Rasteranschluss, Cooker und Gerätebilder bleiben offen.
Größere Körper brauchen begrenzte Domänen mit einem verantworteten Randvertrag;
dieser Versuch fordert kein Vorladen/Fitten eines ganzen weltweiten Flusssystems.

## Reproduktion und Quellen

```sh
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/water/test_physical_boundary.py
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/water/compare_physical_boundary.py --original --output /tmp/outshine-water-boundary-proof
.venv/bin/python test/experiments/water/plot_physical_boundary.py --output /tmp/outshine-water-boundary-proof
.venv/bin/python test/experiments/water/physical_boundary_audit.py /tmp/outshine-water-boundary-proof/original_ill-shared_body_cut-0.npz
```

Vorhandene Python-Umgebung: NumPy, SciPy/Clarabel, Shapely/GEOS, pyproj/PROJ und
Matplotlib; keine neue Abhängigkeit. Diagnosebilder bleiben lokal und werden geöffnet.
SIGGRAPH zuerst: [Losasso/Hoppe 2004](https://hhoppe.com/geomclipmap.pdf), §6.2/Abb.4
des vorhandenen PDFs optisch geprüft: Übergänge und T-Junctions sind getrennte
Anschlussfragen. Das Paper liefert keinen geografischen Randbeweis. Frame-/Normal-
Herleitung und Primärquellen besitzt `lib/water_profiles.md`; affiner Schnitt und
konvexer Randfehlernachweis sind oben explizit hergeleitet.

## Gegenprüfung bei der nativen Integration

Auf macOS/arm64 bestehen dieselben zwölf Kontrollen. Die direkte Python-Neutriangulierung
liefert hier 1494 Vertices/1495 Facetten; ihr kanonischer Fit meldet InsufficientProgress.
Der Prozess liefert dafür einen gültigen Diagnosebericht, aber keinen bestandenen
Originalfall. Solver-/Hüllengrenzen wurden nicht geändert.

Mit den bereits geprüften nativen horizontalen Originaldaten (1492/1492) besteht
auch hier der vollständige Vergleich: gemeinsamer Fit und beide Ausschnitte gültig,
756/754 Kindfacetten, zehn neue Randpunkte/neun gemeinsame Teilkanten; ganze Randstrecken
schließen. Alle 13 Quellen und die Insel bleiben erhalten. Dies bestätigt den begrenzten
Schnittvertrag; es ist keine Freigabe beliebiger GEOS-Neutriangulierungen oder Weltflüsse.

`child_rows` verwendet die Primärzeilenblöcke dieses Versuchs (0:F und F:2F).
Der native allgemeine Fitter besitzt explizite `PrimaryGrades`; seine Integration muss
diese Zuordnung beim Schneiden erhalten und darf keine beliebige Zeilenordnung annehmen.
Integrationsbelege und aktuelle Quell-/Commitzuordnung stehen in PR #947.
