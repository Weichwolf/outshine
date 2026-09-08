Type: defect
State: active
Area: render
Depends: 2111

# Piece materials will preserve unlit shading

Beim Material-Ankunftstest 2111 wird eine Unlit-Oberfläche im Piece-Pfad beleuchtet.
`build/material-append-corrected.log`, Exit 2: 18/19 PASS, ausschließlich
PieceInstancesShareTheirGeometry rot (102 Checks, ein Fehler). PNG material-after
zeigt das neue grüne Dreieck zu dunkel. ReadSceneLinear ist der unveränderte Oracle;
der erste Lauf hatte einen falsch benannten Test-Readback-Aufruf, inzwischen korrigiert.

Ursache: PlacePiece setzt VertexRunsCarried.Normal immer auf true. BindSurface
speichert Unlit nicht. Die Pipelinewahl behandelt jeden solchen Piece-Draw als Lit.
Native SubjectProxy prüft dagegen bereits Row.Unlit bei der Layoutwahl.

Wahl vor Reparatur: dieselbe Regel für Pieces anwenden. SurfaceSlot hält Unlit;
Retable leitet das verwendete Layout aus residenten Streams und aktuellem Material
ab. Bei Unlit Normal/Tangent aus dem Draw-Layout ausschließen, Geometrie erhalten;
UV/Farbe bleiben. Material-Ankunft markiert die Tabelle für Neubindung, damit auch
vorher platzierte Pieces mit erst später verfügbaren Slots erreichbar werden.
Filament/glTF-Unlit ist das gleiche Materialprinzip; Unreal/RAGE sind der Benchmark
für gemeinsam residente Instanzgeometrie, keine Quelle für eine abweichende Unlit-Regel.

Beweis: vorhandener GPU-Piece-Test, neu angehängte Unlit-Textur bleibt bei deklarierter
Beleuchtung in ihrem linearen Farbwert; vorhandene Lit/Normal-/Mask-Prüfungen bleiben.
Negativkontrolle Unlit-Layoutwahl aussetzen: grüner Texturwert wieder rot. Anschließend
PNG öffnen und Koerbersee gegen vorherigen Render prüfen. Keine Orakelsenkung.
