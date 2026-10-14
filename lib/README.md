# Outshine Python reference libraries

Gemeinsame Arbeit: [#47](https://github.com/Weichwolf/outshine/issues/47).

`test/experiments/` untersucht Verfahren und ihre Qualität/Kosten. Bewährte Verfahren
werden in dieses installierbare Paket übernommen. Der Integrationsagent setzt sie
unter `src/` maschinennah um und prüft ihre native Bildwirkung und Kosten.

```sh
.venv/bin/python -m pip install -e ./lib
.venv/bin/python -m unittest discover -s lib/tests -v
```

Die öffentliche API liegt unter `outshine_ref`. Fachmodule `transport`, `buildings`
und `vegetation` importieren gemeinsame Verträge und Geometrieverfahren.
`pipeline.prepare` verbindet ihre Planungsfunktionen in dieser Reihenfolge.
Kerne importieren weder Experimente noch Renderer, Showcases oder fremde Checkouts.

Alle Produkte verwenden denselben benannten rechtshändigen lokalen Rahmen:
X Ost, Y Nord, Z oben, Meter. Geometrien sind XY; Höhenintervalle beziehen sich auf
denselben lokalen Z-Ursprung. Die Quellaufbereitung bindet Projektion und Datum vor
Aufruf der Bibliothek. Esri-Scopes und pflanzeneigene Achsen werden an Fachgrenzen umgerechnet.

Transport liefert nachvollziehbare `FootprintAdjustment`-Produkte und bindende
`Reservation`-Freiräume. Buildings erhält die angepassten Grundrisse; die unveränderten
Quellen bleiben daneben verfügbar. Neue Teile behalten ihre Quelle und erhalten
eindeutige Teilidentitäten. Höhenkontakte und bloße Randberührung sind keine Volumenüberlappung.
`Hint` enthält freiwillige Gestaltungshinweise; er ersetzt keine bindende Reservierung.
Vegetation erhält Reservierungen und Hinweise beider Vorgänger.

Pläne bleiben kameraunabhängig. Die Bibliotheksbasis enthält noch keine vollständigen
Fachsolver, Rezepte, LOD-Ausgabe oder nativen Exportformate. Planungsfunktionen werden
von den Fachbibliotheken schrittweise übernommen; Detailausgabe folgt gesondert.
`Mesh` besitzt unveränderliche Float64-Positionen und Uint32-Dreiecksindices als
erste Referenzbuffer; Material-, UV-, Normalen- und native Präzisionsverträge folgen
mit konkreten Verfahren. Dies ist noch keine Runtime-Asset-ABI.

Das erste übernommene Verfahren `geometry.conform_triangles` stammt aus dem
Transportexperiment: vorhandene Schnittstellenvertices in Dreieckskanten einsetzen,
ohne Koordinaten oder belegte Fläche zu ändern. Die zugehörigen Gegenfälle prüfen
Mehrfachschnitte, echte Lücken, Transformationen und unveränderte Eingaben.

Die Tests arbeiten lokal. Sie lesen oder verifizieren keine GitHub Issues.

`transport.fit_surface_heights(mesh, grade_frames, budget=HeightBudget(), fixed=...)`
passt ausschließlich Z-Höhen an einen weichen Höhenprior an. XY, Dreiecke und der
regionale Z-Bezug bleiben erhalten; gemeinsame Vertices besitzen genau eine Höhe.
Ein 2×2-Rahmen je Dreieck begrenzt die Norm seines normierten Gradienten über die
ganze affine Fläche, beispielsweise Längsneigung und Quergefälle. Ein Kantenlimit
allein genügt nicht. Feste Vertexkontakte verwenden denselben Höhenbezug.
Das Ergebnis `HeightFit` enthält einen unveränderlichen Mesh oder ausdrücklich
keinen Mesh samt Grund. Auch bei Budgetende wird nur ein separat auf ganze Flächen
und Kontakte geprüftes Ergebnis akzeptiert. Solverzeit, Iterationen und Eingabegröße
sind begrenzt; Vorbereitung wird separat gemessen und ein Solver-Schritt kann das
Zeitlimit überschreiten. Dies ist ein Offline-Verfahren, kein Framebudget-Vertrag.

```sh
.venv/bin/python -m pip install -e './lib[transport]'
.venv/bin/python -W error -m unittest discover -s lib/tests -v
```

SciPy/Clarabel werden erst für die Höhenlösung importiert; bereits gültige Priors
brauchen keinen Solver. Die minimax Zielgröße gilt für das feste Eingabemesh und
die vorgegebenen Gradientenrahmen, nicht für eine optimale Erdrekonstruktion.
C1-Anschlüsse, Überhöhung, Terrainformung, Zugänge und Ebenenplanung bleiben offen.
Der Operator ändert keine gemeinsamen Plan-/Reservierungsverträge; ein erhöhtes
Road-Mesh allein ist kein Boden-/Gebäudekontaktprodukt. Fachvertrag:
[#20](https://github.com/Weichwolf/outshine/issues/20), Übernahme #50.

`geometry.SurfaceSampler(mesh)` bereitet eine ausdrücklich gewählte XY-Höhenfläche
für wiederholte `sample(xy)`-Abfragen vor. `SurfaceSamples` besitzt unveränderliche
Float64-XYZ-Punkte/Facettennormalen, Int64-Face-Indices und einen Grund je Abfrage.
Außerhalb, bei mehrdeutiger Projektion oder erschöpftem Kandidatenbudget bleiben
Z/Normale NaN und Face -1; unbekannte Höhe wird nicht null. Gemeinsame Kanten oder
Vertices mit konsistenter Höhe wählen deterministisch die niedrigste Face-ID.
Die aufwärts gerichtete Normale gehört zu dieser Facette und ist nicht geglättet.
Vertikale/degenerierte Projektionen und doppelte Faces werden bei Vorbereitung
abgelehnt. Überlagerte Innenflächen werden lokal als mehrdeutig ausgewiesen.

Default: XY-Abstandstoleranz 1e-8 m, Höhenkonsistenz 1e-7 m, 65536 Vorbereitungsfaces,
65536 Abfragen und 1000000 Kandidaten pro Aufruf. Batches begrenzen GEOS-Ausgabe
vor der Allokation konservativ anhand aller Faces; ein kleiner Rest kann für eine
weitere Abfrage unzureichend sein. Frühere gültige Kontakte bleiben erhalten.
Innerhalb der XY-Toleranz wird die affine Höhe am angefragten XY extrapoliert.
Der Aufrufer hält `LocalFrame`, Produktart Boden/Deck/Dach, Herkunft/Gültigkeit und
Quell-IDs an Mesh/Face-Zuordnung; der Sampler wählt weder die höchste Fläche noch
einen funktionalen Zugang. Gemeinsame Plan-/Reservierungsverträge bleiben erhalten.
Der Terrainversuch unter #20 bindet damit Straßen und Boden an dieselbe C0-Fläche.

`transport.close_surface(mesh, depth_m)` schließt eine bereits geprüfte, konforme
XY-Fahrfläche mit ihren expliziten XYZ-Höhen durch einen Boden und gerichtete
Außen-/Lochwände. Gemeinsame obere Vertices bleiben einmal vorhanden; die Tiefe
gilt vertikal. Das Verfahren prüft Kanteninzidenz, Randzyklen und darstellbare Tiefe,
plant aber weder DEM-Höhen noch Verkehrsverbindungen. Der Aufrufer muss zuvor
überlappungsfreie Geometrie und fehlende T-Verbindungen beweisen.
Gegenfälle prüfen Innenlöcher, Neigung, umgekehrte Eingabeflächen, große
Verschiebungen, nicht darstellbare Tiefe und den Höhenvertrag einer Brücke in der
Transport → Buildings → Vegetation-Kette. Die Bibliotheksübernahme besitzt
[#50](https://github.com/Weichwolf/outshine/issues/50).

Die Vegetationsbibliothek exportiert `vegetation.Needle`: geschlossene, spitze
Organflächen mit elliptischen, rhombischen oder Halbkreis-/Kreissektorquerschnitten,
kontrollierter Biegung/Twist und analytischen Normalen. Gemeinsame Nah-/Grobpunkte
bleiben erhalten. Pflanzenlokaler Rahmen: X quer, Y längs, Z normal, Meter; dieser
Rahmen wird erst bei Weltplatzierung in ENU umgerechnet. `mesh()` liefert eigene
Float64-Positionen, UVs, Parameter und Ecknormalen sowie Int32-Dreiecke als Dictionary.
Harte Kanten und Deckel behalten getrennte Ecknormalen. Unteraufgelöste, falsch
orientierte Facetten werden abgelehnt; feste Unterteilungen beweisen keine LOD-Deckung.
Material, Wind, vollständige Pflanzen und Weltplatzierung bleiben Experimentarbeit.

```python
from outshine_ref.vegetation import Needle

organ = Needle(section="sector", sector_count=5, aspect=1.0)
near = organ.mesh(32, 16)
coarse = organ.mesh(16, 8)
```

Quellen, Gegenfälle und Bild-/Kostenbelege: [#45](https://github.com/Weichwolf/outshine/issues/45),
[#46](https://github.com/Weichwolf/outshine/issues/46), Übernahme [#52](https://github.com/Weichwolf/outshine/issues/52).

`buildings.modelling` enthält den übernommenen Scope-Kern: `Frame`, `Scope`,
`Relative`, `t`, `r`, `s` und `translate`. Er hält die dokumentierten CGA-Achsen
Ost/Auf/Süd intern; `cga_to_enu`/`enu_to_cga` konvertieren Positionen und Normalen
an der gemeinsamen ENU-Grenze, ohne Dreieckswinding oder Höhenbezug zu ändern.
Alle Gebäudeexperimente verwenden diese Implementierung. Der verbundene Modellierkern
enthält außerdem `Shape`, `Geometry`, `Face`, `Material`, `Asset`, `Texture`,
`Branch`, `Part` und `Floating`: Komponenten, Extrusion, Unterteilung, Offset,
Pult-/First-/Walmdächer, UV-/Materialbearbeitung, Projektionen und statische Asset-Platzierung.
`Shape.rotate_scope` richtet nur den Scope neu aus und erhält die sichtbare Geometrie.
`Shape.align_scope_to_geometry_bbox("xz")` findet die flächenminimale projizierte
Box mit GEOS; das erste benannte Ausmaß ist länger, die dritte Achse bleibt fest.
Alle sechs geordneten Achsenpaare sind möglich. Pivot und Scope-Ursprung fallen
zusammen; native Indices, UV0–9, Normalen und Materialressourcen bleiben erhalten.
Punkt-/Linienprojektionen sind zulässig. Unreferenzierte Vertices beeinflussen
die Box nicht; ein unbenutzter Vertex außerhalb einer nullgroßen Achse wird
ausdrücklich abgelehnt. Gleichstände sind deterministisch, aber noch nicht gegen
ein isoliertes SDK-Orakel abgeglichen; Trim-Planes bleiben offen.
Die vorhandenen Gegenfälle/Referenzen bleiben unter `test/experiments/buildings/`.
Konstruktive Gebäudepläne und Rezepte liegen weiterhin dort (#51); viele dokumentierte
CityEngine-Fähigkeiten und Teilverträge sind noch offen (#34).

`prepare_hip(polygon)` bereitet unveränderliche metrische Walmdach-Pläne vor:
Facetten, ursprüngliche Quellkanten, geteilte Ridgeknoten und Wavefront-Distanzen.
`HipPlan.lifted_vertices(value, mode)` verwendet dieselbe Topologie für `byAngle`
oder `byHeight`; sein XY gehört zur Eingabeebene, Z zeigt entlang deren Normale.
Der Aufrufer setzt diesen lokalen Ebenenrahmen ausdrücklich in die Welt ein.
Konkavität und Höfe bleiben erhalten. Rechtecke benötigen keinen Skelettsolver;
allgemeine Formen verwenden das optionale Extra `pip install -e './lib[roofs]'`.
SFCGAL wird zuerst geprüft; vereinfacht es ursprüngliche Quellkanten, übernimmt
der Python-Solver. Für Höfe werden gemessene Skelett-Bisektoren mit den unveränderten
Quellringen polygonisiert; Trauf-Endpunkte werden innerhalb der bestehenden
Rundungstoleranz an die ursprünglichen Punkte gebunden. Der direkte Facettenexport
bleibt für Formen ohne Höfe erhalten. Beide Ergebnisse müssen dieselben Geometrie-/Quellkantenprüfungen
erfüllen. Der vollständige Verfahrensvergleich umfasst auch die nötigen Rückfälle.
`SkeletonLimits` begrenzt Eingaben, Knoten und kooperativ verstrichene Zeit;
`python_events` gilt nur für den Python-Solver. Native Ereigniszahlen sind unbekannt
und werden als `None` gemeldet. Schwere Aufrufe gehören in abbrechbare Worker,
keine Deadline-Garantie innerhalb jedes Backend-Aufrufs.
`Shape.roof_hip(value, mode="byAngle")` verbindet diese Pläne mit nativen
Quell-Dreiecken: Schnitte erhalten deren UV0–9-Interpolationsbereiche,
Material-/Gruppen-/Tagbesitz und den lokalen Objektframe. Authored Normalen
werden bei der geänderten Geometrie neu aufgebaut; fehlendes UV0 bleibt fehlend.
Die gemeinsame planare Überlagerung verwendet ein explizites Präzisionsraster
und berichtet dessen Meterweite im `roof_info`. `max_cells` begrenzt vorab
Quell-Dreieck-/Facettenkandidaten, danach Zellen und Ausgabedreiecke.
`byHeight` ist ebenfalls verfügbar. `even=True` unterscheidet Firstgruppen
von verbindenden Walmlinien und hält Traufknoten fest. Bei `byAngle` werden
Gruppenhöhen gemittelt, bei `byHeight` auf das jeweilige Gruppenmaximum gesetzt.
Die vorgegebene maximale Dachhöhe bleibt dabei erhalten. Nichtplanare Facetten besitzen
eine feste Triangulation vor dem UV-Schnitt; unverändert planare Flächen
behalten ihre ursprüngliche Darstellung. Die Gruppierung folgt dem statisch
untersuchten 2°-Knotenfächer und wird gegen einen isolierten SDK-Geometrieaufruf
geprüft. Ein degenerierter realer SDK-Gegenfall bleibt ausdrücklich offen.
`roof_info.surfaces` hält die vorbereiteten Flächen; tatsächliche gemeinsame
Höhenanpassungen am Präzisionsraster werden in Metern berichtet.
Negative `overhang`-Werte schneiden das feste Dachfeld an einer höheren Traufe.
Die eigene Variante bildet eine geschlossene Hülle: Unterseite im ursprünglichen
Quellniveau, verbleibende Dachfläche und senkrechte äußere/innere Randflächen.
UV0–9 folgen weiterhin den nativen Quell-Dreiecken; Randflächen erben den
jeweiligen Rand-UV-Chart ohne zusätzliche Variation entlang der Höhe.
`roof_info.insets` enthält Domäne, Traufhöhe und Raster; vollständig kollabierte
Quellflächen stehen in `collapsed_source_faces` und erzeugen leere Produkte.
Der Schnitt ändert die erhaltenen Firste nicht. Das ist eine eigene gültige
Interpretation und keine SDK-Kontur-/Bottom-/UV-Gleichheit. Positive Überhänge,
genaue SDK-Gratgruppen-Semantik und Trim-Planes bleiben bei
[#67](https://github.com/Weichwolf/outshine/issues/67); positive Überhänge werden
weiterhin ausdrücklich abgelehnt.

`partition_exterior_charts(corners, a, b, exterior)` zerlegt eine bereits gültige
Außendomäne nach den nativen Dreieck-Charts an einer ursprünglichen Traufkante.
Stationen entlang der Kante bestimmen den Besitzer; erster/letzter Chart reichen
in die Eckbereiche. `extrapolate_chart` setzt die zugehörigen affinen
Eckattribute fort, ohne die UVs am Grundrissrand zu klemmen. Die Domäne bleibt im
ursprünglichen Koordinatenrahmen; eine Rotation schmaler GEOS-Konturen kann ihre
Topologie durch Rundung verändern. Ein gemeinsamer Roof-/Source-Schnitt muss
die Chartgrenzen anschließend in der tatsächlichen Mesh-Ausgabe erhalten.
Das ist eine eigene Rand-Chart-Politik, kein SDK-UV-Orakel und noch keine gültige
Gesamtgeometrie für positive Überhänge.

```sh
.venv/bin/python -m pip install -e './lib[assets]'
```

`assets` installiert Trimesh/Pygltflib für den optionalen OBJ/glTF/GLB-Import.
Der Grundkern benötigt NumPy, Shapely und Pillow. Importiert er nur eigene Shapes,
lädt er die Asset-Decoder nicht. Intern bleiben CGA-Achsen ausdrücklich benannt;
`Shape.from_polygon` übernimmt einen ENU-XY-Grundriss und erzeugt ihn in CGA-XZ
mit der angegebenen lokalen Höhe. Gemeinsame Ausgabe an Transport/Vegetation/world
wird mit `cga_to_enu` für Positionen/Normalen konvertiert. Quellen/IDs sind der
Auftrag des übergeordneten Gebäudeplans; ein Shape allein ist kein Gebäude-Assetvertrag.

`vegetation.Profile` übernimmt vorbereitete skalare Profile: unveränderliche
Float64-Tabellen, Lookup, einseitige Tabellenslopes, Clamping und gekachelte Drift.
Laden/Auswertung importiert keinen Kurvensolver. Kurvenvorbereitung liegt getrennt
in `vegetation.bezier_curve.BezierCurve` und benötigt SciPy ≥1.15; es ist eine
deklarierte Paketabhängigkeit. Inversion nutzt maximal 100 Solveriterationen.
Die ausgegebene SciPy-Polynomdarstellung ist eine unabhängige Kopie.
Bool-, Text-, komplexe und nicht endliche Koordinaten sind ungültig.

Abbruch anhand kleiner Funktionsreste reicht bei flachen x-Kurven nicht;
`x(t)=t³` wird gegen die analytische Kubikwurzel geprüft. Tabellenauflösung
begrenzt Lookup-Genauigkeit; eine allgemeine y-Fehlergarantie ist nicht belegt.
[SciPy 1.15 find_root](https://docs.scipy.org/doc/scipy-1.15.0/reference/generated/scipy.optimize.elementwise.find_root.html)
dokumentiert Intervall-/Resttoleranzen; Profile und Referenzproben gehören zu [#26](https://github.com/Weichwolf/outshine/issues/26).

`vegetation.Blade` enthält offene Blatt-/Grasflächen in Metern, mit gelapptem
Rand, Kreisführung, Twist, Querwölbung/Falz und Blattstiel. Pflanzenlokal gilt
X quer, Y längs, Z normal. Float64-Positionen/Parameter/Ecknormalen und Int32-
Dreiecke gehören dem Aufrufer; Knicke behalten einseitige Normalen.
`surface_jet` liefert dieselben Normalen und Flächenmetrik plus Ableitungen
nach u=(s+1)/2 und t. Gemeinsame Parameterpunkte bleiben bei Unterteilung gleich.
Falsch orientierte oder nicht endliche Facetten werden abgelehnt; der Aufrufer
muss innerhalb seines Budgets verfeinern oder die Detailanfrage zurückweisen.
Unterteilung allein beweist keine Silhouetten-/Deckungsfehlergrenze.
`area` integriert die Fläche; geteilte Quadraturpuffer sind unveränderlich.
Quellen/Flächen-/Materialbelege und offene botanische Qualität stehen in [#22](https://github.com/Weichwolf/outshine/issues/22);
Bibliotheksübernahme in [#52](https://github.com/Weichwolf/outshine/issues/52).

### Organ sites along parent axes

`outshine_ref.vegetation.DistancePattern` prepares complete phyllotactic or whorled
site tables from parent lengths in metres, positive internode profiles in metres,
normalized bounds and divergence in radians. Tables contain `(parameter, azimuth)`
rows in stable group/member order. Seeded spread is independent per parent; pass
stable seeds when parent order changes. Recipe reload preserves these sequences.
Preflight uses conservative capacities: an insufficient aggregate budget returns
`None` before site allocation; a parent exceeding 4096 sites is rejected. Empty
and populated tables own immutable bytes. The implementation skips empty parents
after vectorized preflight, with bounded parent/site work. This does not establish
a complete SpeedTree property mapping, botanical recipe or native placement ABI.

### Spatial branch curves and surfaces

`outshine_ref.vegetation.CubicSpine`, `ArcMeasure`, `SpineField` and `SpatialAxis`
provide camera-independent cubic plans, monotone arc coordinates, transported
frames and closed elliptical sweeps. Plant-local X is transverse, Y forward,
Z normal, in metres. World conversion remains the caller's explicit ENU boundary.
Cubic controls certify positive forward speed and a conservative curvature bound;
radius must satisfy the local tube certificate. This proves neither global
collision freedom nor a general silhouette/coverage bound.

Prepared knots have immutable bytes. Public SciPy polynomial/curve properties
return independent inspection copies; hot sampling uses private prepared objects.
`ArcMeasure.evaluate(t, derivative)` evaluates arc length or its derivatives at
a normalized raw curve parameter. `parameter(fraction)` maps normalized arc
length to the raw curve parameter. `SpatialAxis.mesh` owns Float64 positions,
per-corner normals and physical bark fields plus Int32 faces; caps retain hard
normals. Roll-profile total variation per band must stay at most π/2, and every
facet must agree with each corner normal. Refine within the caller's budget or
explicitly refuse unresolved detail; do not accept aliased full rotations.
Spatial classes load lazily, keeping prepared Profile import NumPy-only.
Sources, independent references, original frame probes and open SpeedTree
property/force/LOD/wind bindings belong to [#27](https://github.com/Weichwolf/outshine/issues/27),
[#39](https://github.com/Weichwolf/outshine/issues/39) and [#52](https://github.com/Weichwolf/outshine/issues/52).

### Procedural fin attachment

`FinAttachment(parent, fraction, azimuth, start_angle)` maps a source mesh onto a
fixed cross-section of a prepared `SpatialAxis`. Source X wraps around the ring;
after rotation about X, Y offsets along the ellipse's outward unit normal and Z
extends along the parent tangent. The nominal X radius is the geometric mean of
the cross-section semiaxes; X is not actual elliptical arc length. Angles use
radians and fraction uses normalized parent arc length. Templates remain owned
by the caller; `deform` preserves source faces/UVs and emits owned positions,
per-corner inverse-transpose differential normals and physical bark fields.

Rotated Y must be nonnegative, |X| must stay strictly below π times the nominal
radius, and each triangular face must span at most π/2 in X angle. Positive
Jacobian determinant and face/corner orientation are checked. The continuous
outward convex-ring warp is injective on this domain; overlapping source meshes
and collisions with other objects remain the caller's responsibility. The
cofactor normal method and independent NumPy `solve` method use the same full
inputs; neither method is a native GPU cost claim.

`SpatialAxis.mesh_at(fractions, angles)` accepts bounded increasing fractions
covering [0,1] and increasing ring angles in [0,2π). Callers can include attachment
anchors in both sampling grids. The Fin experiment shares exact parent/fin
positions at those anchors. Indices still belong to separate meshes: this is not
a Boolean union or topological weld, and off-ring vertices do not follow the
longitudinal parent surface. Source vertices are bounded to 65,536 and triangles
to 262,144 (or a smaller `max_faces`); oversized deform requests refuse before
parent preparation. Full SpeedTree property, wind and LOD bindings remain open
in [#81](https://github.com/Weichwolf/outshine/issues/81).
