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
Keine Übernahme proprietären Codes. Eigene Verfahren werden aus Geometrie/Verträgen hergeleitet.

[Self-organizing Trees, SIGGRAPH 2009](https://algorithmicbotany.org/papers/selforg.sig2009.html):
Metamer/Blattknoten als Architekturbaustein. Begrenzte Triebgrammatik ist keine Lichtkonkurrenz.
[Leaf Translucency](../doc/references/vegetation/egsr/2007-real-time-leaf-translucency.pdf):
dünne reflektierende/transmittierende Flächen; geöffnete PDF-Seite 2. Vorschau-Materialmix
ist qualitative Näherung, kein implementiertes gemessenes BSSRDF-Modell.
[Q. robur](https://efloras.org/florataxon.aspx?flora_id=2&taxon_id=210001863):
12 × 6 cm und 4 mm Blattstiel sind eine plausible Probe, kein weltweiter Standard.

## Umsetzung
`curve_frame.py`: bogenlängenparametrisierte Kreisachse mit orthonormalem Rahmen.
`blade_surface.py`: gelappte Randfunktion, Biegung, Falz, Twist; analytische Flächennormalen.
Gemeinsame Parameterpositionen bleiben bei Unterteilung gleich; Spitze ohne Nulldreiecke.
Twist benötigt auch Querteilung: nur Längsteilung konvergiert bei der Flächenprüfung nicht korrekt.
Blattstiel beginnt genau am Triebknoten. Falz/Blattstielknick besitzen seitenspezifische Ecknormalen.
Fläche durch konvergierte Quadratur; Triebgruppen speichern Anzahl/Fläche/Bounds vor Organ-Meshing.
`shoot_plan.py`: fünf Blattstellungen, physische Internodien, unveränderliche geteilte Platzierungen.
`axis_surface.py`: geschlossener gekrümmter Trieb; `organ_experiment.py` prüft Differentiale,
Flächen, Unterteilung, Anschlüsse/Bounds/Budgets und gleiche Geometrie bei gemeinsamer/expandierter Ablage.
Feste Render-Eingaben: drei belaubte Triebe und 1 764 Halme; Render-Abnahme steht noch aus.
Geteilte Organ-Meshes plus kompakte Platzierungen gegen vollständige Expansion messen.
Grobe Anfragen benötigen keine Organpositionen; begrenzte Expansion erhält unbearbeitete Teilpläne.
Ruheplan/IDs sind kamera- und zeitunabhängig; Wind wirkt später an denselben Ankern.

## Abnahme und offene Lücke
Unabhängige Differential-/Flächenprüfung, reguläre Geometrie, Einheiten und exakte Anschlüsse.
Identische IDs/Positionen bei Wiederholung und gemeinsamer Nah-/Fernherkunft; Budgets explizit.
Belaubte Triebe und Gras als tatsächlich geöffnete EEVEE-Bilder, Kosten und Speicher getrennt messen.
Volle Artenvielfalt, organische Gabeln, Deckungs-/Schatten-LOD und Windbilder bleiben offen.
CPU-/Blender-Proben ersetzen weder native Runtime-Bilder noch GPU-/A18-Pro-Budgets.
