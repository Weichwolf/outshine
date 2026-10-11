Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2111
Depends:
Area: generators, render
Tags: vegetation, python, leaves, grass

# Procedural organs and shoot groups preserve attachments

## Ergebnis und Besitzer
Python-Bausteine ersetzen geschlossene Nahkronen durch räumliche Blätter, Blattstiele
und gebogene Grashalme. `test/experiments/vegetation` besitzt Verfahren und lokale Bilder;
2111 besitzt Bestände, weltweite Vielfalt und die native Übernahme. SpeedTree ist Maßstab,
keine erreichte Qualität. Blatt-/Triebgruppen ergänzen Broccoli/Romanescos Hierarchie.

## Quellen und gewonnene Entscheidungen
Nutzerpaket `/tmp/SpeedTree_Modeler_v10.2.0_Linux.tar.gz`, entpackte Referenz
unter `~/Downloads/SpeedTree_Modeler_v10.2.0`; keine fremden Assets im Repository.
Komprimierte XML-Modelle zeigen Generatorgraphen, Profile, Anker und geteilte Mesh-Assets.
Öffnete Vorschauen: Oak Sapling, Oak Hero Forest, Oak English Cluster Branch und Grass.
Die Eichenprobe enthält 483 Leaf-Mesh-Knoten, 68 Äste und 50 Fronds; gespeicherte
3 328 Dreiecke sind Modeler-Statistik, keine eigene Laufzeitmessung oder Anzahl echter Blätter.
Nadelbaum: Leaf-Mesh-Gruppen besitzen weitere Leaf-Mesh-Kinder; Blattgruppen sind keine Einzelorgane.
Cutouts enthalten eigene Anker: Geometrievereinfachung darf Kindanschlüsse nicht verschieben.
`inspect_speedtree.py` liest ausgewählte Pflanzen und dokumentiert Befunde ohne Fremdcode/-Assets zu bündeln.

[SpeedTree 10: Modellierung](https://docs.unity3d.com/speedtree-modeler/manual/modeling-approach.html):
Generatorgraph, Standort am Elternast, Eltern-/Verlaufsprofile und kontrollierte Varianz trennen.
[Phyllotaxis](https://docs.unity3d.com/speedtree-modeler/manual/phyllotaxy-generation-mode.html):
Internodien in Längeneinheiten, gegenständige/wechselständige/quirlige Anordnung statt freier Kronenpunkte.
[LOD](https://docs.unity3d.com/speedtree-modeler/manual/level-of-detail.html):
Innenverdeckung, Silhouette, reduzierte Segmente und vergrößerte verbleibende Blattgruppen.
Das ist eine Vergleichstechnik, kein Beweis für Deckungs-/Schatten-/Lichtkonservierung.
[Batched Leaf](https://docs.unity3d.com/speedtree-modeler/manual/batched-leaf-generator.html):
hoher Durchsatz durch Gruppensteuerung, aber laut v10-Doku keine dynamischen LODs/Growth.
Unsere Gruppierung muss bedarfsabhängige Produkte und stabile Organ-IDs trotzdem erhalten.
[Kollision](https://docs.unity3d.com/speedtree-modeler/manual/leaf-collision.html):
Cutout-Dreiecke enthalten transparenten Leerraum; reine Dreieckskollision würde zu viel entfernen.
Organflächen, Gruppenbelegung und Schatten deshalb gemeinsam beurteilen.

`standard_shaders/SpeedTreeWind.h` im Paket ist lesbare Implementierung:
`WindRuntimeSdk` kombiniert Ripple, zwei Astebenen und globale Bewegung; verankerte
Richtungsänderung mit Längenbegrenzung. Ruhegeometrie und laufender Wind bleiben getrennt.
`linux/SpeedTree_Modeler` enthält die Generatorfunktionen direkt, mit Funktionssymbolen.
Gelesen: relative Segmentzahl, Blatt-LOD-Gewicht und Aufrufe der Mastersegment-Berechnung;
Elternzugriff/Bezierprofile sind belegbar, vollständige interne Solver daraus nicht abgeleitet.
Ghidra 12.1.2 (Snap) dekompiliert sieben gezielte Funktionen, Projekt außerhalb des Repositories.
Phyllotaxis `0x1e296e0`: Schritt durch wirksame Elternlänge normalisieren, vom letzten
zum ersten Standort laufen; Profile steuern Abstand/Roll, eine Anzahl begrenzt die Schleife.
Intervallplatzierung `0x1e2b3a0` verwendet zusätzlich den Kehrwert eines Frequenzparameters.
Mastersegmente `0x1cb7260`: absolute/relative Anteile, Profile und Mindestzahl vier.
Diese Aussagen beschreiben sichtbare Operationen; unbekannte Felder/Signaturen bleiben unbenannt.
Blatt-LOD-Vorbereitung reserviert Skins; vollständige Reduktions-/Deckungsverfahren bleiben offen.
Keine Übernahme proprietären Codes. Eigene Verfahren werden aus Geometrie/Verträgen hergeleitet.

[Self-organizing Trees, SIGGRAPH 2009](https://algorithmicbotany.org/papers/selforg.sig2009.html):
Metamer/Blattknoten als Architekturbaustein. Begrenzte Triebgrammatik ist keine Lichtkonkurrenz.
[Weber/Penn, SIGGRAPH 1995](../doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf):
Astordnungen mit Elternprofilen, Neigung und Blattverteilung; PDF-Seiten 2/5 optisch betrachtet.
Eine begrenzte eigene Grammatik ergänzt die vorhandenen Einzeltriebe zu belaubten Pflanzen.
[Leaf Translucency](../doc/references/vegetation/egsr/2007-real-time-leaf-translucency.pdf):
dünne reflektierende/transmittierende Flächen; geöffnete PDF-Seite 2. Vorschau-Materialmix
ist qualitative Näherung, kein implementiertes gemessenes BSSRDF-Modell.
[Q. robur](https://efloras.org/florataxon.aspx?flora_id=2&taxon_id=210001863):
12 × 6 cm und 4 mm Blattstiel sind eine plausible Probe, kein weltweiter Standard.

## Umsetzung
`curve_frame.py`: bogenlängenparametrisierte Kreisachse mit orthonormalem Rahmen.
`blade_surface.py`: gelappte Randfunktion, Biegung, Twist; analytische Flächennormalen.
Breite Blätter erhalten eine quadratische Querwölbung, Grashalme eine lineare Falz;
Positionen und Differentiale verwenden dieselbe Form. Vorherige reine Falz wirkte im Bild papierartig.
Gemeinsame Parameterpositionen bleiben bei Unterteilung gleich; Spitze ohne Nulldreiecke.
Twist benötigt auch Querteilung: nur Längsteilung konvergiert bei der Flächenprüfung nicht korrekt.
Blattstiel beginnt genau am Triebknoten. Falz/Blattstielknick besitzen seitenspezifische Ecknormalen.
Fläche durch konvergierte Quadratur; Triebgruppen speichern Anzahl/Fläche/Bounds vor Organ-Meshing.
`shoot_plan.py`: fünf Blattstellungen, physische Internodien, unveränderliche geteilte Platzierungen.
`axis_surface.py`: geschlossener gekrümmter Trieb; `organ_experiment.py` prüft Differentiale,
Flächen, Unterteilung, Anschlüsse/Bounds/Budgets und gleiche Geometrie bei gemeinsamer/expandierter Ablage.
`organ_preview.py` konsumiert deklarierte Szenarien mit geteilten Meshes und Platzierungsrahmen.
Feste Bilder: drei 48-cm-Triebe mit 44 Blättern und 1 764 Halme; EEVEE-Proben optisch betrachtet.
Acht stabile Blattvarianten und feinere Ränder vermindern Gleichförmigkeit/Kantigkeit.
Prozedurale Mittelrippe, Reflexion/Transmission und echte Schatten; keine Fototexturen.
`plant_plan.py`: begrenzte Astordnungen, Elternintervalle, Längenprofile und orientierte Kreisachsen.
Stabile Pfade bestimmen Ast-/Organ-IDs; Mesh-Unterteilung verändert weder Achsen noch Anker.
Anzahl, variantengewichtete Blattfläche und konservative Teilbaum-Bounds vor Platzierungen ableiten.
Teilgruppen entstehen erst bei ausreichendem Detailbudget; Kameras sind kein Bestandteil der Identität.
`plant_experiment.py`: Jungbaum mit 64 Achsen/571 Blättern, Gesamt-/Nahansicht desselben Plans.
Blattneigung gegen steile Flächen zum Himmel hin abflachen; das ist ein Formrezept, keine Lichtsimulation.
Rahmen über Kreuzprodukte bilden: direkte Projektion verliert bei fast parallelem Hint Orthogonalität.
Gabeln bestehen aus überlappenden geschlossenen Rohren; organische Verschweißung bleibt offen.
Krone und Grasverteilung wirken weiterhin regelmäßig. Arten-/Bestandsvielfalt bleibt 2111s Aufgabe.
Geteilte Organ-Meshes plus kompakte Platzierungen gegen vollständige Expansion messen.
Kostenprobe: dieselbe Geometrie/UVs/Ecknormalen, 32 Längssegmente und eine Querteilung je Hälfte.
Median/Spanne aus drei Zusammenstellungen; gemeinsamer Mesh-/Platzierungsaufbau und Prüfungen aus Messzeit.
Grobe Anfragen benötigen keine Organpositionen; begrenzte Expansion erhält unbearbeitete Teilpläne.
Ruheplan/IDs sind kamera- und zeitunabhängig; Wind wirkt später an denselben Ankern.

## Abnahme und offene Lücke
Unabhängige Differential-/Flächenprüfung, reguläre Geometrie, Einheiten und exakte Anschlüsse.
Identische IDs/Positionen bei Wiederholung und gemeinsamer Nah-/Fernherkunft; Budgets explizit.
Belaubte Triebe und Gras als tatsächlich geöffnete EEVEE-Bilder, Kosten und Speicher getrennt messen.
Drei Pflanzen-Seeds: vollständige/teilweise Expansion, umgekehrte Anfragefolge, Anker und Teilbaum-Bounds.
18 ungültige Fälle zurückweisen; gleiche angeforderte Zweiggruppe gegen vollständige Expansion messen.
Volle Artenvielfalt, organische Gabeln, Deckungs-/Schatten-LOD und Windbilder bleiben offen.
CPU-/Blender-Proben ersetzen weder native Runtime-Bilder noch GPU-/A18-Pro-Budgets.
