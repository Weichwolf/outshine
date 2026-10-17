# Horizontale Straßenkurven mit erhaltenen Originalanschlüssen (#535)

Die sechs südwestlichen Quellen aus [#530](stair_southwest_comparison.md)
erhalten zusätzlich eine begrenzte Fahrgeometrie: fünf innere Knicke mit
**30 m Achsradius**, glattem Krümmungsanschluss und höchstens **25 mm**
Chordbound. Dieselben 43 Kandidaten/74 inneren Endprofile, ein altes
Innenquerprofil und zwölf Außenprofile; keine neue Quellaufnahme oder native
Abnahme (`admitted_source_ids=[]`). Ganze horizontale Kompaktwagenkörper bleiben
auf beiden tatsächlichen Fahrspuren. Vollständige Kreuzungsrouten bleiben offen.

## Quelle, Knoten und gewähltes Verfahren

Originale OpenFreeMap-/DEM-Eingaben, IDs, Tags, 5,5-m-/Zweistreifenannahmen,
Richtungen und beide Endprofile bleiben wie im Vorgängervergleich erhalten.
Das [Klothoidenrezept](fixtures/stair-southwest-curves-vienna.json) ergänzt eine
ausdrücklich getrennte Geometrieachse; Originalpunkte werden nicht überschrieben.
Der vorhandene eigene symmetrische Klothoidenkern und die Achsenkomposition
liegen jetzt unter `outshine_ref.transport.clothoid_turns`/`horizontal`.
[Öffentlicher Vertrag](../../../lib/horizontal.md): offene Achsen, begrenzte
Original-/Ausgabepunkte, Radius, Endgeraden, Verschiebung und Abtastarbeit;
keine Experimentimporte. Alte Experimentimporte sind Weiterleitungen.

Die zunächst naheliegende ersetzende Achsenrundung wäre semantisch falsch:
Am 92,793°-Knick von `103612620:19` endet auch die noch ungeplante Straße
`406276972:0`. Der Originalknoten läge **14,050 m** von der neuen Achse entfernt,
außerhalb ihrer 2,75-m-Halbbreite. Numerisch gültige Kurven allein beweisen
keinen erhaltenen Quellanschluss. Gewählt wird deshalb die Vereinigung der
ursprünglichen belegten Straßenfläche mit der zusätzlichen Kurvenfläche.
Alle Originalflächen bleiben vollständig enthalten; alte Knoten und äußere
Ecken dürfen stehen bleiben. Die tatsächliche Fläche wächst nur um
**147,824 m²**, von 4261,087 auf 4408,911 m². Ungeplante Quellen bleiben mit
Originalen/Gründen im Katalog, keine stillschweigende Anschlussfreigabe.

Der [Kreisbogenvergleich](fixtures/stair-southwest-circle-vienna.json) verwendet
dieselben Originale, Radius-, Chord-, Endgeraden- und Verschiebungsgrenzen,
Spurbreiten und Körper-/Terrainprüfungen. Beide vollständigen Varianten bestehen.
Kreisbögen behalten an geraden Übergängen Krümmungssprünge; die gewählte
Klothoiden–Kreis–Klothoiden-Familie hat Krümmung null an beiden Enden.
Rampenanteil je 0,25; analytische Maxima von Krümmung und Krümmungsänderung.
Die Modelle erfüllen damit unterschiedliche Stetigkeitsanforderungen;
kein Geschwindigkeitsgewinner bei gleicher Qualität behauptet.

SIGGRAPH zuerst: lokale Chen-2008-Referenz zur Trennung von Graph und Geometrie.
Ergänzend Galin et al. 2010, Abschnitt 6/S. 8, Figuren 17–18 samt Text
**optisch geprüft**: diskrete Achse → Klothoiden → Profil/Terrain. PDFs unter
`doc/references/transport/`; Verweise im Vorgängervergleich. Die konkrete
begrenzte symmetrische Konstruktion/Implementierung ist eigenes Verfahren,
kein übernommener Fremdcode oder SDK. Keine neue CityEngine-Analyse in diesem
Schritt; frühere Operationsreferenz #91 beweist keinen OSM-Generator.

## Ganze Fahrspurhüllen und Gegenfälle

Verwendet wird das versionierte kompakte Fahrzeugrezept: 1,8 m Breite,
3,3 m vor der Hinterachse, 1,1 m hinter ihr, Mindestradius 4 m. Je Spur liegt
die Hinterachse 1,375 m neben der analytischen Achse. Originales oneway bleibt
maßgeblich; sonst prozedurales Rechtsverkehrsrezept mit je einer Gegenrichtung.
Kleinster konservativer Hinterachsradius **28,625 m**.

Nicht nur Posen geprüft: Für jeden Körperpunkt gilt über ein Kurvenintervall
die zweite Ableitungsgrenze `L²*(k + rho*(k_rate + k²))`. `rho` umfasst Fahrzeug
und seitlichen Spurversatz. Die konvexe Hülle der Endkörper plus ganzer
Chordabweichung muss innerhalb der **tatsächlichen Quellkappe und ihrer
jeweiligen Fahrspur** liegen. 20 Double-/Float32-Prüfungen, zusammen 408 Posen/
204 ganze Intervalle; größte akzeptierte Abweichung **23,584 mm**. Keine
bloße Mittellinien- oder Stichprobenfreigabe. Numerische Integration durch
unabhängige Kontrollen abgesichert, kein formales IEEE-Intervallzertifikat.

Ein zu breites 3-m-Fahrzeug fällt trotz passender Mittellinie durch. Das ganze
[4-m-Radius-Gegenrezept](fixtures/stair-southwest-tight-curves-vienna.json)
scheitert am Hinterachsradius der inneren Spur;
[1000-m-Radius](fixtures/stair-southwest-space-curves-vienna.json) an zu kurzen
Nachbargeraden. Radius wird nicht verkleinert. Beide ganzen Ablehnungsprodukte
enthalten nur Bericht/Quellkatalog/Readerbeleg und erhalten die 37 alten Quellen.
Kein verworfenes Mesh. Veränderte gespeicherte Geometrieachse wird zurückgewiesen.

Diese Hülle ist horizontal und starr. Keine ganzen Kreuzungs-/Endpunktrouten,
Lenkdynamik, Geschwindigkeit, 3D-Chassisfreiheit, Verkehrsvorschrift oder Statik
bewiesen. Vorhandene Längs-/Quer- und Terrainprüfungen bleiben separat.

## Geprüfter Stand und Kosten

Bibliothek/Geometrie, beide ganzen Varianten, Reader, Rohbelege, Bilder und
Allokationslauf: `71e2b1722`. Danach `45287700e` migriert einen alten
2000-Fit-Kontrollaufrufer auf vier begrenzte Abtastbatches, ohne Fälle oder
Grenzen zu reduzieren; kein geändertes Bibliotheks-/Geometrieverfahren.

- **97 Quell-/97 installierte Kontrollen** bestanden. Zusätzlich unabhängige
  ODE-Integration für Richtungsänderungen bis 2,5 rad, ganze dichte Chordkontrollen,
  Besitz/Mutation, Platz-/Arbeits-/Rückwendungsgegenfälle. Voriger Kern gegen
  neuen auf **93 Fits/11997 Posen bytegleich**. Alte 1000 symmetrische und
  alle 2000 variablen Rampenkontrollen bestanden, Batches 512/512/512/464.
  Vorhandene ganze Hüllenkontrollen: 1029 Krümmungsratenfälle, 3×32 Kurven,
  unabhängige Integration, verborgenes Hindernis und langer Turn bestanden.
- Frisches Wheel SHA256
  `9bb4903594c14f52dd7f1ee0b7ba99b2e84a36dfceeab7f1b17a33e3ae60d6e3`;
  alle **200** installierten Python-Module bytegleich zum Quellenstand,
  zwei additive Module, vorherige 198 unverändert. Keine Plan-/Reservations-/
  Provider-/native-/Shader-API geändert. Alle bisherigen Aufrufer bleiben erhalten.
- Ganze Erzeuger ohne alte Planer, gespeicherte Reader zusätzlich ohne CDT,
  LP/Kegelsolver, Körperbau oder Arrayexport. Alle neun normalen/profilierten
  NPZ-Arraymengen bytegleich; dritter Cache-Elternimport erhält alle alten
  Körperkomponenten XYZ/Facetten bytegleich. 14714 originale DEM-Abfragen und
  vier ganze Crops gegen originale vollständige WebP-Decodes bitgleich.
- Klothoiden: 282 Oberflächenpunkte/411 Facetten, geschlossener Körper
  564 Vertices/1128 Facetten. Kreis: 243/353 und 486/972. Ganze Double-/Float32-
  Topologie, Profile, Wände, Längs-/Querrahmen bestehen; größter nomineller
  Straßenrahmenwert **0,947105**, höchstens **2,285868 %** Quergefälle.
- Nur 841 alte Terrainfacetten ersetzt; lokal 1041 Punkte/1949 Facetten,
  893 feste Höhen/131 wörtlich alte Randhöhen. Gegenüber Kreis 1002/1871.
  Weld 1528→1061, 467 Paare, **0 m Verschiebung**, 20 unbenutzte Punkte entfernt;
  Konformierung 64 Kanten/+316 Facetten. Roh-XY 78672 B, eindeutiges XY
  16656 B, Zuordnungen 39336 B. Ganzer Welt-/Lokalbezug vom Reader geprüft.
- Welt 13374 Punkte/26390 Feldfacetten; Diagnosekörper 26748/53492.
  Außerhalb 24441 ganze alte XYZ-Dreiecke bytegleich. Alte Klassen-/5-m-/
  Neigungsgrenzen, Brücken, Treppen und alle 124 unteren Pfeilerkontakte
  erhalten; maximal weiterhin **15,170 µm** Float32-Kontaktfehler.
- Normaler Klothoiden-Gesamtaufbau **18,675 s/180756480 B Peak-RSS**,
  Kreis **18,239 s/180473856 B**. Parallel zueinander und Nice-10-Kontrollen;
  kein belastbarer Laufzeitvorsprung. Globale Gegenprüfung allein 13,995 s
  beim normalen Klothoidenlauf. Reader 14,234/17,018 s.
- Memray 1.20: **96455670 Allokationsereignisse/11062933125 B angefordert/
  56126428 B verfolgter Heap-Peak/203862016 B Prozess-Peak-RSS**.
  Instrumentiert 43,210 s, davon 34,610 s Weltprüfung; alle Python/native
  Allokationen nach Imports, ohne native Stack-Abwicklung wie im Vorgänger.
  Statistik separat terminal erfolgreich. Unterprozessallokationen, PNG/Reader,
  interne Gesamtkopien und Statistik-Unterprozess-RSS nicht erfasst.
- Sieben Produktarrays 3360674 B, Display zusätzlich 10215296 B;
  globaler XY-/Indexscratch 324872 B, Außenvergleich 3519504 B. Abschließende
  Wandprüfung 173824 Facettenprojektionen, zusätzlich gleicher Umfang für
  Rundungsauswahl. Angeforderte Bytes sind weder Live-Daten noch kopierte Bytes.

Linux/WSL2, i5-1135G7/vier sichtbare logische CPUs, rund 15,44 GiB RAM;
Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Shapely 2.2.0. Unverändert 120 s
CPU/3 GiB Adressraum, Solver 8 s/100 Iterationen. Ein äußerer Profil-Reader-
Prozess endete nach vollständigen validen Exporten mit 143; Wiederholung mit
explizit erfasstem Interpreterabschluss **0** bestand. Keine bloße Export- oder
Prozessstatusannahme. Kein vollständiges natives Lint/Engine-Gate durch diese
Python-Forschung ersetzt; keine Frame-/A18-/RTX-/Lade-/Statikabnahme.

## Bild und Reproduktion

Vier aktuelle vollständig dekodierte/geöffnete **1280×720**-Eevee-Bilder:
vorherige 43er Szene gegen dieselben 43 Quellen mit Kurven, identische Kameras.
Ganze Übersicht 2248,388 m; moderate Sicht 1250 m. llvmpipe/LLVM 19.1.7,
12,040–15,742 s/Bild. `test/scripts/pixels.py`: 521/1460 geänderte Pixel.
Zusätzlicher innerer Kurvenraum erkennbar; kleine Gesamtbildänderung, ursprüngliche
äußere Knoten/Ecken bleiben sichtbar. Netz weiterhin lückenhaft, Umgebung klinisch.
Keine Nahansichtsserie, öffentliche Ersatzgalerie oder Server.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest test_horizontal stair_road_curve_checks -q
```

Die ganzen [Modulaufrufe aus #530](stair_southwest_comparison.md#reproduktion-und-verbleibender-umfang)
mit `stair-southwest-curves-vienna.json` bzw. `stair-southwest-circle-vienna.json`
und frischen Temp-Ausgaben verwenden; anschließend unabhängigen Reader.
`stair_module_cost.py --profile <frischer Trace> --no-native-stacks` misst
aktuelle Prozessallokationen. Ganze alte verifizierte Elternexporte erforderlich;
lokale Tests benutzen versionierte Originalinputs, keine privaten Ausgabepfade.

Aktuelle Belege, Wheel/Trace/Bilder:
`/tmp/outshine-transport-curves-0dc514752`; volle Varianten
`/tmp/outshine-transport-curves-{curves,circle,profile}-71e2b1722`.
Keine Testabhängigkeit von privaten Pfaden. Originale, nötige Eltern, aktuelle
Belege und Gegenfälle bleiben erhalten. Automatische weltweite Modulgraphen,
weitere Klassen/Ebenen, ganze Routen, LOD-/Cache-/Reservationsprodukte und
native Integration bleiben offen; #434/#50 und Gesamtziel bleiben aktiv.
