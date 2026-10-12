Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, branches, welding, geometry

# Organic branch forks share one surface

## Ergebnis und Besitzer
Gemeinsame organische Gabeloberflächen ersetzen sichtbare Nähte überlappender Einzelrohre.
2348 besitzt SpeedTree-Funktionsanalyse; 2350 Radiusprofile, 2351 räumliche Astachsen,
2346 spätere Pflanzen-/Organanschlüsse. Der Funktionsnachbau bleibt vor Artenrezepten.

## Referenz und Verfahren
Ghidra: ComputeWeldRing, WeldStart, CacheWeldRing, GetWeldParent und FixBifurcation.
[SpeedTree Welding](https://docs.unity3d.com/speedtree-modeler/manual/branch-generator-properties.html#welding):
Elternanschluss, Raycast-Alternative, Offset und Verbreiterung des Anschlussbereichs.
[Bloomenthal, SIGGRAPH 1985](../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf)
liefert räumliche Gabel-/Sattelreferenzen; Abbildungen zur Verzweigung optisch prüfen.
Eigene lokal begrenzte Verschmelzungsform aus Astsegmenten; direkte radiale Flächenabtastung
mit volumetrischem Lewiner-Marching-Cubes auf denselben Eingaben vergleichen.
Bibliotheken nutzen; Feld, Gradienten und Fehler unabhängig prüfen. Kein Fremdcode im Repository.
[Lewiner et al., JGT 2003](../doc/references/vegetation/jgt/2003-marching-cubes-topological-guarantees.pdf)
mit [scikit-image 0.26.0](https://scikit-image.org/docs/stable/api/skimage.measure.html#skimage.measure.marching_cubes):
Topologiegarantie betrifft das Gitterinterpolant, keine beliebige kontinuierliche Eingabe.
[Simplex-Projektion, Wang/Carreira-Perpiñán 2013](../doc/references/vegetation/math/2013-probability-simplex-projection.pdf)
für symmetrische kompakte Feldmischung; PDFs einschließlich Algorithmen/Abbildungen optisch geprüft.

## Eigene Form und Datenfluss
Segmentfelder si=ri²−dist² zum Segment ab gemeinsamem Ursprung, positive Radien.
σ(s)=max_{w≥0,Σw=1}(w·s−τ||w||²/2)+τ/2; Gewichte durch Simplex-Projektion bestimmen.
max(s)≤σ≤max(s)+τ(1−1/N)/2; allein dominante Segmente außerhalb der Mischzone unverändert.
Bei σ=0 sind alle Segmentabstände mindestens ri; ∂σ/∂t≤−2rmin²/t entlang Ursprungsstrahlen.
Eigener Beleg für eindeutige radiale Nullstelle und nichtverschwindende Oberflächengradienten.
Feldwerte haben m², keine vorzeichenbehaftete Distanz; Normalen aus dem Gradienten ableiten.
`fork_field.py` besitzt Plan/Feld; `fork_surface.py` Gitter-/radiale Flächen, `fork_adaptive.py` Vergleichspfad.
`cube_surface.py` ist gemeinsame Cube-Parametrisierung; Kronenaufrufer migrieren, Ausgaben erhalten.

## Umsetzung und Abnahme
Stabile Quelldaten bestimmen Gabel und Anschlüsse; angeforderte Auflösung vor Erzeugung.
Eine geschlossene außenorientierte Oberfläche, gültige Dreiecke, gemeinsame Normalen;
getrennte interne Rohrflächen dürfen den neuen Anschluss nicht darstellen.
Gleiche Form-/Fehleranforderung, Erzeugung und Produkt-/Arbeitsspeicher getrennt messen.
Symmetrische, ungleiche und räumliche Astgabeln unter kontrollierter Kamera/Licht rendern;
mit dem bisherigen Überlappungsbild und passender Referenz tatsächlich optisch vergleichen.
Beobachtete Stichprobenfehler nicht als allgemeine Hausdorff-Schranke ausgeben.

## Aktueller Beleg und Verfahrenswahl
40 unabhängige SLSQP-Vergleiche, zwei analytische Kugelfälle mit 202 Richtungen,
je Gabel 37 skalare Feldpositionen/41 Brent-Richtungen, Differential-/Transformations-/Permutationsprüfungen.
Symmetrisch, ungleich und räumlich: 27 Geometriefälle, jeweils drei CPU-Wiederholungen.
355 unabhängige Richtungen je Fall, gleicher radialer Stichprobenfehler ≤5 mm, keine Normalenumkehr.
Alle Flächen geschlossen, eine Komponente, Eulerzahl zwei; Produktbytes/Volumengitter/RSS getrennt.
Marching Cubes mit 128 Zellen erfüllt den Bildprobenvertrag in allen drei Fällen und ist hier CPU-günstiger.
Gleichmäßige radiale Abtastung verfehlt ihn; adaptive Abtastung reduziert Dreiecke, kostet mehr Python-CPU.
Ein adaptiver Lauf kann seine lokale Probeanforderung trotz gültiger Geometrie wegen Tiefenlimit verfehlen.
Zusätzliche Vertex-Projektion beim 64er Gitter reicht im Vergleich ebenfalls nicht für die Fehleranforderung.
Vergleichspfade bleiben Experimente, keine native Gesamtwahl; Original-Weld-Solver/-Kosten nicht ausgeführt.
Aktuelle Probe mit identischen Segmenten vor/nach Verschmelzung und zwei Varianten tatsächlich öffnen.

## Offene Lücke
Vollständige SpeedTree-Weld-Property-/Elternflächenbindung, beliebige gekrümmte Gabelregionen,
Rinde, Wind, ganze Pflanzen und native Runtime-/Geräteabnahme bleiben offen.
