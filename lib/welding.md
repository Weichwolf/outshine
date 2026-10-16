# Numerische Vertex-Kanonisierung (#517)

`outshine_ref.geometry.weld_vertices(vertices, tolerance_m, *, max_vertices=65536,
max_pairs=65536)` liefert eigene unveränderliche Float64-Vertices, unveränderliche
Ganzzahllabels je Eingangsvertex und einen tatsächlichen Verschiebungs-/Arbeitsbericht.
Die Eingabe bleibt erhalten. Zwei oder drei Koordinaten, ein gemeinsamer lokaler
metrischer Rahmen; keine Quell-/Datum-/Ebenenentscheidung oder Topologiegarantie.

Der Operator bildet Komponenten der Paare im euklidischen Toleranzradius und wählt
den ersten gelieferten Vertex jeder Komponente. Jeder Eingangspunkt muss danach
höchstens `tolerance_m` entfernt liegen. Lange transitive Ketten werden ausdrücklich
abgewiesen, selbst wenn alle direkten Paare innerhalb des Radius liegen. Diese
kontrollierte Ablehnung ist kein minimaler Clustering-/Ballüberdeckungsalgorithmus.
Die Eingangsreihenfolge bestimmt die kanonischen Koordinaten; Quellen-IDs/Ordnung
bindet der Aufrufer. Kein stilles Sortieren, Mitteln oder wachsender Wiederholungsradius.

Endliche reelle Nx2/Nx3-Eingaben, Betrag höchstens 1e9 m; Toleranz von 1e-12 bis 1 m.
Kapazitäten sind echte Integer bis 1048576, Vertexkapazität mindestens eins,
Paarkapazität darf null sein. Leere/ungültige Eingaben oder überschrittene Grenzen
werden vor der jeweiligen folgenden Arbeitsphase abgewiesen. SciPys cKDTree zählt
alle Radiuspaare vor deren Arrayallokation. Anzahl, Ausgabegröße, tatsächlicher
Maximalabstand und explizite Indexkopie-/Differenz-/Abstandsarrays stehen im Bericht.
SciPy-/Sparse-/Norm-Interna und deren Gesamtkopien sind damit nicht vollständig erfasst.
Zusätzlicher Spitzenbedarf bleibt mit dem wirklichen Verbraucher zu messen.

SciPy ist bereits Paketabhängigkeit; seine Module werden erst beim Aufruf geladen.
Es gibt keine Imports aus Experimenten, keine neue native-/SDK-/Cache-Abhängigkeit.
Der neue Wiener Verbraucher wählt ausdrücklich eine interne 100-nm-Schwelle und
prüft anschließend weiterhin die ursprünglichen ganzen Quellenflächen, Profilkontakte,
geschlossenen Körper/Vertexlinks, Klassenneigungen und tatsächlichen Float32-Exporte.
Die vorherigen 10-nm-Defaults bleiben unverändert. Ein generischer Punktoperator
kann weder gültige Dreiecke noch einen funktionsfähigen Verkehrsverbund versprechen.

Quell-/Wheelkontrollen und echte vollständige Körper-/Gelände-/Pfeiler-/Bild-/Kostenbelege
gehören zum [Fachvergleich](../test/experiments/transport/stair_north_bridges_comparison.md).
