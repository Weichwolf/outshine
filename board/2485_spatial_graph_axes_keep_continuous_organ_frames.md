Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, spine, graph, bifurcation

# Spatial graph axes keep continuous organ frames

## Ergebnis und Besitzer
Räumliche Astachsen, längenabhängige Organe und Biegestandorte in einem gemeinsamen Graphen.
2351 besitzt räumliche Kurven/Transport, 2483 Graphprodukte, 2484 physische Standortfolgen.
Der Graph benutzt bisher Kreisbögen; räumliche Kurven müssen Geometrie und Kinder gleich führen.

## Verfahren
SIGGRAPH-/TOG-Rahmenverfahren aus 2351 zuerst; Ghidra-Bifurcation-Felder und Standortpfad prüfen.
[Bloomenthal, SIGGRAPH 1985](../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf),
[Wang et al., TOG 2008](../doc/references/vegetation/tog/2008-rotation-minimizing-frames-preprint.pdf):
räumliche Achsen/Doppelreflexion; PDF-Seite 14 mit Algorithmus optisch geprüft.
Ghidra-Läufe 21/22: CreateProperties, FillLocationVector und CBranchNode::GetBifurcationSpots,
alle terminal erfolgreich. Threshold/Spacing/Balance/Locality/Align/Act anhand ELF-Strings gebunden.
GetBifurcationSpots verwendet Nachbar-Spinevektoren, gewichtete Richtungen und normierte Winkel;
Standorte/Rollwinkel werden weitergefiltert. Vollständige Feld-/Betriebsartenbindung bleibt offen.
Ein kamera-/LOD-unabhängiges Kurven-/Rahmenprodukt trägt beliebige angeforderte Organanker.
Physische Bogenlänge, Rahmen und ihre Ableitungen zusammen verwenden; Differentialnormalen prüfen.
Keine vollständige Original-Property-/Kraftbindung oder natürliche Gabeloberfläche daraus behaupten.

## Umsetzung und Beleg
`spine_measure.py`: 16/32-Punkt-Gaussvergleich, monotones Hermite-Bogenlängenprodukt und Batch-Inverse.
`spine_field.py`: Doppelreflexion, Hermite-Rahmenfeld, Tangentenprojektion und exakte Feldableitungen.
`spatial_axis.py`: kanonische lokale Y-Achse, Ellipsen/Radius/Roll, direkt grobe/feine Flächen samt Rinde.
`graph_frames.py`/Graph: identische Einzel-/Batch-Anker, vollständige Elternrahmen gebündelt auswerten;
zusätzlicher Rahmencache hängt am begrenzten Standortcache. Details erst nach Pose-/Dreieckbudgets.
`bend_pattern.py`: eigene lokale Richtungsänderungen, physisches Fenster/Abstand, Prominenz und Limit;
Elternbereiche vor Auswahl anwenden, Zweige zur Außenseite der Biegung richten.
[SciPy find_peaks](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.find_peaks.html)
liefert lokale Maxima/Abstände; Prominenz lokal begrenzen, Rundungswellen gleichmäßiger Bögen vermeiden.
Vier Kurven mit Gerade, Wendepunkt, räumlichem Verlauf und Roll gegen unabhängige Quad-/DOP853-Referenzen.
Längenfehler ≤4,44e−16 m, geprüfte Bogenanker ≤8,48e−9 m; Rahmenfehler ≤8,82e−6.
Analytische Differentialnormalen gegen Differenzen: ≤1,27e−8; 80 gemeinsame grobe/feine Vertices gleich.
Geschlossene zweifach inzidente Kanten, nichtdegenerierte Flächen und unitäre rechtshändige Rahmen geprüft.
Gerade/gleichmäßiger Kreis: keine Biegestellen; drei planare Biegungen, Außendrehung ≤1,23e−16 Fehler.
16 Intervalle/Kubiksegment erreichen 1e−5 am Datensatz, 8 scheitern; kein allgemeiner Kurvenfehlerbeweis.
Fünf lokale CPU-Läufe: 16er Vorbereitung+513 Anfragen Median 5,184 ms, warm 1,037 ms/513 Anfragen;
22.544 vorbereitete Koeffizienten-/Stützpunktbytes. Reine Graph-Blattposen samt frischem Aufbau:
36,768 ms Einzelrahmen / 23,515 ms Batch; alle 41 Posen und direkte Teilanfragen exakt identisch.
Probe: Stamm, drei Biegezweige, 41 Blätter; komplette grobe/feine Prototypen 362/3.128 Dreiecke.
Produktaufbau/Kompression/Schreiben 0,065 s (eine Probe), frischer Prüfprozess 105,58 MiB Peak.
Bestehende Graph-/Standortprodukte aus neuer Implementierung elementweise unverändert;
vorhandene Renderprodukte wiederverwenden. Keine native/GPU-/Gerätefreigabe durch Python-Belege.
Gesamt-/Grob-/Nahansicht 2560×1440 betrachtet; lokale Kragen/Rinde in 2486, volle Originalbindung offen.

## Abnahme
Unabhängige Integrations-/ODE-/Differentialreferenzen, Wendepunkte und räumliche Kurven prüfen.
Grobe/feine Meshs und Einzel-/Vollanfragen behalten identische Anker und gemeinsame Flächenpunkte.
Komplette Produkte mit Rindenkoordinaten rendern, 2560×1440-Bilder tatsächlich betrachten.
Erstaufbau/Lookup/Geometrie/Bytes getrennt messen, Varianten bei gleicher Qualität vergleichen.
