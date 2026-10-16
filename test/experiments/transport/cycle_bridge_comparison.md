# Weitere Wiener Radbrücke (#404)

Die bisher ausgelassene Radbrücke `94031072:0` erhält ein geschlossenes Deck,
20 tatsächlich anschließende Stützen und Profile zu den vorhandenen Bodenradwegen
`32205120:5`/`:6`. Die drei vollständigen IDs/Originalattribute stehen im festen
[Input](fixtures/cycle-bridge-vienna.json). Alle 3259 Quellen bleiben im Ledger;
2563 statt2562 sind verwendet. Die beiden alten Bodenwege wählen abschnittsweise
neue Höhen; die übrigen3256 Entscheidungen bleiben exakt erhalten.

## Eingang und Verfahren

OpenFreeMap-Planet20260927_080001_pt, ENU-Ursprung48.21/16.37. Die gerade Brücke
ist207.411866835m lang; beide vorhandenen Anschlussachsen zusammen rund160.326m.
1.50m Breite ist das vorhandene Querschnittsrezept, keine gemessene Brückenbreite.
Das Fixture enthält zwölf Originalquellen, eine tatsächlich deckende affine
Bodenzelle sowie beide vollständigen Unterführungsflächen aus dem gespeicherten
feinen Verkehrsmesh. Ihre Ränder weichen bis zu Bruchteilen eines Millimeters
von den ursprünglichen Quellbändern ab. Der Vergleich berücksichtigt beide.
`capture_cycle_bridge.py` liest nur gepinnte Offline-Eingänge und überschreibt
keine vorhandene Datei. SDK-/Höhenvermessungswissen wird nicht behauptet.

Chen et al., SIGGRAPH2008, S.2/Fig.2 trennt Graph und Geometrie;
Galin et al., Eurographics2010, S.8/Fig.17–18 trennt Profil, Bauwerk und Terrain.
Beide lokalen PDFs wurden auf diesen Seiten optisch geprüft. Hier werden diese
Trennungen genutzt, nicht die vollständigen Netzerzeugungsalgorithmen übernommen.
[Primärquellen](../../../doc/references/transport/README.md).

Ein Brückentagwechsel ist kein Bodenhöhenanker: Die Unterquerung bei Station197m
liegt nur rund10m vor dem Brückenende. Gewählte Rampen setzen sich deshalb auf
vorhandenen Anschlusswegen fort. Sieben andere Bodenanschlüsse bleiben flächig
auf ihrem bisherigen Höhenfeld; zwei Rad-/Fuß-Unterführungen bleiben getrennt.
Das bewahrt die Topologie dieses lokalen Quellenverbunds. Vollständige
Fahrrouten, Kurvenradien und Barrierefreiheit werden damit nicht freigegeben.

Die Höhe ist `ground(XY) + r(dot(XY-origin, axis))`. Auf der tatsächlichen
einzelnen affinen Bodenzelle wird die ganze Gradientenbedingung exakt zu einem
Intervall für `r'`: mit `a=dot(ground_gradient, axis)` und querliegendem `b` gilt
`-a-sqrt(G²-|b|²) <= r' <= -a+sqrt(G²-|b|²)`. Ganze Überlappungsflächen liefern
Stationsintervalle für Bodenbindung und Durchfahrt. Keine Mittellinienstichprobe.

23 feste Stationen ergeben eine Kette von Differenzbedingungen. Zwei gerichtete
Durchläufe propagieren die unteren Höhenbounds; Gegenläufe verlieren wegen
`lower_rate <= upper_rate` Höhe und müssen nicht wiederholt werden. Jeder
maximale Bound stammt somit von einem einfachen Weg von links oder rechts.
Das liefert die komponentenweise kleinste zulässige Höhe auf dieser festen Kette.
Ein verletzter oberer Bound weist deren Unvereinbarkeit nach. Positive
Trapezgewichte minimieren dieselbe integrierte Anhebung im unabhängigen HiGHS-LP.
Diese Aussage betrifft nur die festgelegte Kette/Form/Bodenzelle, kein globales
Straßennetz- oder kontinuierliches Formoptimum. Beide Verfahren liefern dieselben
Höhen. Die vorhandene unabhängige Streifenschneidung verwirft den tatsächlichen
Input als `projected_geometry_invalid`; die gemeinsame noded Schnittanordnung
liefert vollständige, konforme Flächen ohne numerisches Schweißen/Kollapsentfernen.

## Physische Abnahme

Deckdicke vertikal0.35m; die untere Kappe liegt bei
`ground + max(r-0.35,0)`. Die Enden verjüngen sich bis zum Boden ohne versteckte
Unterlage. Außerhalb des positiven Höhenbereichs bleiben die alten Bodenwege.
Die Brückenachse selbst ist vollständig durch den neuen Körper gedeckt.
Die alten Bodenflächen bleiben als Fundamentgeometrie erhalten; die drei
Quellenprofile bestimmen die aktive Verkehrshöhe. Kein zweiter aktiver Höhenbesitz.

Die erste exakte Double-Lösung bestand nach Float32-Export die12%-Grenze nicht
(12.1294%). Der Gegenfall bleibt als echter Test erhalten. Unveränderte
Abnahme12%/2.50m, Baureserve0.002 im Gradientenbetrag und0.01m Durchfahrt:
Double maximal11.80000000003%, tatsächliche Float32-Kappen11.80018923824%.
Unterführungen Double2.51m; konservative Float32-Untergrenze2.50994910029m.
Diese berücksichtigt die ganzen quantisierten unteren Kappen, alle168 relevanten
ursprünglichen Bodenvertices und die räumliche Verschiebung beider Flächen.
Kein Schluss allein aus gewandelten oder nachträglich geclippten Stichpunkten.

Roh-/Exportkörper:52Vertices/100Flächen, Kappen28/26, Grundfläche360.314299m²,
Volumen123.668447m³. Ganze Felder stimmen bis1.14e-13m; Volumenintegral bis
1.43e-11m³. 20Stützen188Vertices/296Flächen, maximale projizierte Stationslücke
12m; Boden-/Deckkontakte bis2.85e-14m. Das ist ein konstruktives Stützenrezept,
keine Statik. Höchstens64Stationen und fünf örtliche Varianten je Stütze;
fehlende Stützen dürfen das24m-Rezept nicht überschreiten.

Die unabhängige gespeicherte Prüfung kontrolliert vollständige Quell-/Kappen-
Coverage, ganze affine Höhenfelder, geschlossene positive Körper, Kappen-/Wandbesitz,
Float32-Projektion/Neigung/Orientierung, die tatsächlichen Unterführungsflächen,
Stützenkontakte/-überschneidungen, alle alten Arraypräfixe sowie neue Normale,
Farben, Quellenprofile und Ledger. Die aktuelle feine Bodenprüfung betrifft1367
Boxkandidaten/250geschnittene Flächen. Alle461 bestehenden Rand-/Rampenkörper und
alte Brücken/Portale/Pfeiler bleiben geschützt. Ungültige Profilvarianten liefern
keine neue Höhe; der bestehende Szeneninput wird niemals verändert. Der echte8%-
Gegenfall, private Zugänge und korrekte Vertices über einem falschen Flächenknick
gehören zu den19 Kontrollen einschließlich des betroffenen alten Rampenpfads.

## Kosten und tatsächliches Bild

Eingefrorener Erzeuger/Reader/Bilder/Kosten:`10c8719cadf84ca8b052e73f691d3c853133c95e`.
Python3.13.5, NumPy2.2.4, SciPy1.18.1/HiGHS1.12.0, Shapely2.2.0/GEOS3.14.1.
Ganzer Offline-Erzeuger3.756573s, Peak267544KiB. Szene433666Vertices/783240Flächen;
Zuwachs240/396. Besitzende Arrays66801264 statt66766992Bytes. Diese Payloadgrößen
sind keine Totalzählung interner Kopien/Allokationen. Kein Internet-/Native-/FPS-Nachweis.

Elf normale Wiederholungen nach Import/einem warmen LP, Prozesse nacheinander ohne
laufende Blender-Renderer, OPENBLAS/OMP je1. Median Kette0.334501ms, LP3.487108ms.
Die Kette bearbeitet44Relaxationen; LP88Sparse-Nonzeros/1152Sparse-Bytes.
Beide568Input-/184Resultat-Bytes. Separate Memray1.20.0-Aufnahme mit Python-/nativen
Allokatoren:934/19838Ereignisse,56488/1522327angeforderte Bytes,
5688/341319Bytes getrackter Heap-Peak. Worker-RSS84955136/85086208Bytes normal;
instrumentierte Zeit18.5697/66.0016ms. Umfang: nur Profilsolve samt Validierung;
keine Aussage über Gesamtallokationen oder einen entsprechenden Generatorfaktor.

Beide tatsächlich geöffneten Gesamtbilder1280×720,950m horizontaler Ausschnitt,
identische Kamera/Licht/Farben, EEVEE5.2.2/llvmpipeLLVM19.1.7,16Samples.
Vorher30.0049s/2293772KiB; nachher29.9628s/2305848KiB, beide gleichzeitig mit
nice10 und zwei Renderthreads. Die Brücke schließt sichtbar eine lokale Lücke;
der restliche graue Forschungslook bleibt schlicht. 1988/921600Pixel geändert,
1816über1/255, maximal83/255; keine Aufwertung der gesamten Stadt behauptet.
Keine GitHub-Galerie oder Bildveröffentlichung durch den Fachagenten.

Kanonische lokale Belege: `${TMPDIR:-/tmp}/outshine-transport-cycle-404-result-10c8719ca`
mit `comparison.json`, `verification.json`, `bridge.npz`, `piers.npz`,
`source-usage.json`, `profile-input.npz`, `after-body.npz`, beiden PNGs/Receipts und
`pixels.log`; Kosten in `outshine-transport-cycle-404-cost-10c8719ca`.
Die Schlüsselhashes stehen im JSON; der feste Input hat SHA256
`50ba8c4c0b4963b64d539c69ceb23c2b9f28b371d24af1e4437235cc355b916c`.

## Grenzen und nächste Übernahme

C0 statt C1; Gradienten- und Luftraumbedingungen ersetzen keine Fahrdynamik.
Höhe/Breite/Dicke/Stützen/Freiraum sind offen gewählte Rezepte. Ein einzelner
affiner Bodenpatch und eine gerade Achse sind hier ausdrückliche Voraussetzungen.
Weitere86-bzw.nun85Brückenachsen, Weichen, Geländer, Statik und volle Wegefunktion
bleiben offen. Noch keine neue installierbare API, Reservation-/Buildings-/Vegetation-
Freigabe oder native Integration. Der native Wiener Konflikt #400 ist ein anderer,
von master belegter unvereinbarer Fall; dieser Versuch repariert ihn nicht.
Der verifizierte lokale Quellenverbund ist der nächste Eingang für weitere
allgemeine Form-/Kontaktverfahren und die eigenständige native Übernahme.
