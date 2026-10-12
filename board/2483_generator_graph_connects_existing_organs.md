Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, graph, hierarchy

# Generator graph connects existing organs

## Ergebnis und Besitzer
Äste, Blätter und Fronds aus einem gemeinsamen unveränderlichen Generatorgraphen erzeugen.
2348 besitzt SpeedTree-Analyse, 2346 bestehende Achs-/Organpläne, 2482 Frondflächen.
Gemischte Funktionspflanzen vor weltweiten Artenrezepten; keine botanische oder native Freigabe.

## Bedarf und Verfahren
Die festen Astordnungen können eigene Blatt- und Frondgeneratoren bislang nicht frei verbinden.
Ghidra-Lauf 19: vier CGenerator-Funktionen erfolgreich dekompiliert; Elternvorlage nimmt
den ersten passenden eingehenden Link, TopologicalLeaf berücksichtigt sichtbare Kindgeneratoren.
Mehrere Eltern sind eine eigene Erweiterung, keine nachgewiesene SpeedTree-Semantik.
Original-Modeler 10.2 mit Palm_Desktop tatsächlich geöffnet; GUI-Bildreferenz unter
`build/shots/reference/vegetation/speedtree-palm-desktop-modeler.{png,json}`.
Trial entfernt zufällige Exportpolygone; keine vollständige Geometrie-Oracle daraus ableiten.
Private GLU/OpenGL/Xvfb-Proot-Laufzeit bleibt in Downloads, Originalmodelle bleiben unverändert.
Graphen/Instanzen und Geometrie trennen; stabile Hierarchiepfade bestimmen Identitäten und Varianz.
Zahlen/konservative Bounds vor Platzierungen oder Meshing ableiten; angeforderte Teilprodukte begrenzen.
Gemeinsame Prototypflächen mit vollständigen Platzierungen und Form-/Materialrezepten speichern.
[Weber/Penn, SIGGRAPH 1995](../doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf)
begründet parametrische Hierarchien; 2482 besitzt die Frond-/Elternprofilverfahren.
`generator_{specs,graph,examples,scenes,checks,experiment}.py`: vier eigene Grundtypen,
feste Anzahlen plus physische Standortfolgen (2484), Kreisachsen/Profile/Pfade; volle Generierungsarten offen.
`graph_{sources,products,material,preview}.py`: vollständige Prototypen und Instanzfelder,
verlustfreie Hierarchiepfade, physische Rindenkoordinaten auch bei Instanzskalierung.

## Geprüfter Iststand
Funktionspflanze: 63 logische Instanzen, vier Formen, 19.872 feine Prototypdreiecke.
Identische Form aus zwei Generatoren: ein Prototyp, 50 Instanzen mit eindeutigen Pfaden.
Grobe/feine Produkte teilen Identitäten und Platzierungen, Dreiecke werden direkt grob erzeugt.
Budgets verwerfen Anfragen vor Platzierung/Meshing; 262.144 Instanzen als eine Teilanfrage
verweigert, einzelner Pfad daraus benötigt vier Posen. Nullanzahlen erzeugen keine Arbeit.
Frames bleiben bei 90° stetig, rechtshändig/orthonormal; Rezeptreihenfolge/Namen ändern keine Form.
Sieben lokale CPU-Proben derselben sieben Fronds: Median 21,470 ms Vollaufbau,
4,990 ms Bedarf (15 statt 63 Posen), 0,048 ms Wiederverwendung; Graphdefinition/Meshing/IO ausgeschlossen.
Nah-/Grobprodukte inklusive Kompression/Schreiben zusammen 0,149 s (eine lokale Probe).
Peak des frischen Prüf-/Produktprozesses 95,23 MiB; 723.479 komprimierte Mesh-/Posenbytes
für beide Detailstufen ohne doppelt gezählte gemeinsame Platzierungen; kein Geräte-/GPU-Nachweis.
Gesamt-, Grob- und Nahbilder 2560×1440 betrachtet; sparse Mischpflanze ohne Artenfreigabe.

## Abnahme und offene Lücke
Voll-/Teilanfragen, Graphreihenfolge, Mehrfachanschlüsse, ungültige Graphen und identische Produkte prüfen.
2560×1440-Gesamt-/Nahbilder tatsächlich betrachten; Kosten, Speicher und Cachetreffer getrennt prüfen.
Vollständige SpeedTree-Graph-/Generierungsarten, Kräfte/Wind/Wachstum und Arten-/native Freigabe bleiben offen.
