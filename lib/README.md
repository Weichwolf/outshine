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

`places` teilt die geodätische Szenariokamera sowie begrenzte, explizit ausgewählte
MVT-/Terrarium-Eingaben zwischen den Python-Fachagenten. Installation, Einheiten,
Fehlverhalten und Grenzen stehen in [places.md](places.md) (#216).

`materials.filtered_edge_weight(distance_m, reach_m, half_footprint_m)` liefert eine
Float32-Materialmaske über einen signierten metrischen Rand. Ein analytischer Boxfilter
integriert die kubische Übergangsmaske im eindimensionalen Normalen-Footprint; beide Seiten
teilen komplementäre Gewichte. Keine Geometrie-/Klassenänderung und keine exakte Integration
gekrümmter Ränder oder beliebig orientierter quadratischer Pixel. #173 besitzt die native
Übernahme samt räumlichen Hinweisen und Bild-/Kostenprüfung.

`materials.MaterialRegion` bindet eine polygonale Fläche an lokalen Rahmen, Materialzeile,
Rang und Quell-IDs. `coalesce_regions(tuple)` vereinigt gleiche Material-/Ranggruppen nur,
wenn der Rang in der Eingabe eindeutig einem Material gehört. Mehrdeutige Ränge behalten
ihre Originaleinträge/Reihenfolge. Quellen bleiben beim Aufrufer; alle beitragenden IDs bleiben
im Ergebnis. GEOS-Union ist ein Offline-Verfahren, kein Runtime-/GPU-Kostenbeleg.

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
C1-Netzanschlüsse, Überhöhung, Terrainformung, Zugänge und Ebenenplanung bleiben offen.
Der Operator ändert keine gemeinsamen Plan-/Reservierungsverträge; ein erhöhtes
Road-Mesh allein ist kein Boden-/Gebäudekontaktprodukt. Fachvertrag:
[#20](https://github.com/Weichwolf/outshine/issues/20), Übernahme #50.

`transport.plan_port_heights(PortHeightInput(...), budget=PortHeightBudget())`
plant gemeinsame geneigte Kreuzungsebenen, ganze rationale C1-Straßenübergänge
und einen konformen C0-Straßen-/Bodenmesh. Eingabe: unveränderlicher `Mesh` mit
weichen originalen Höhenpriors, Face-Modul-IDs (`-1` für Boden), Rollen (`0`
Korridor/`1` Kreuzung), ganze gemeinsame Portkanten und deren Modulpaare sowie
ein normierender 2×2-Gradientenrahmen je Face. Originale Face-/Modul-IDs und
Vertexanker bleiben erhalten, auch bei anderer Face-Reihenfolge oder Winding.
`PortHeightFit` enthält einen `PortHeightPlan` oder einen konkreten Grund je Modul.

Der Plan besitzt unveränderliche Feldparameter, endlichen `mesh`, Quellface- und
Vertex-/Portabbildungen, Ganzflächen-Höhenschranken und geerbte Gradientenrahmen.
`plan.evaluate(xy, module_ids)` liefert absolute analytische Höhen/XY-Gradienten;
es ist ausdrücklich keine Kontakt- oder automatische Flächenabfrage.
Tatsächliche endliche Kontakte konsumieren
`geometry.SurfaceSampler(plan.mesh, max_faces=budget.max_output_faces)`;
Frame, Kind, originale Quellen/Attribute und vorherige Auslassungen bindet der
Aufrufer. `PortPlaneField` bietet separat die mathematische Patchfamilie samt
`evaluate`/`bounds`; außerhalb der gewählten Domäne ist das nur eine Fortsetzung.
Bei exakt gespeicherten Ankern kann `xy_tolerance_m=0` ausdrücklich gewählt
werden. Ein positiver Suchradius kann mehrere benachbarte Features treffen;
der bestehende Sampler meldet dann konservativ einen mehrdeutigen Kontakt statt
einer erfundenen Höhe. Das verändert weder seine Defaults noch gemeinsame APIs.

Abnahme: ganze normierte Neigungen ≤1+1e-7, Höhenchords ≤0,01m und endliche
Port-Gradfehler ≤0,005. Nur originale Beobachtungen gehen in den Fit ein.
Ein bereits zulässiger globaler Ebenenprior bleibt bis auf Rundungsfehler erhalten,
auch an freien Korridorenden. Derselbe Topologie-, Neigungs- und Kontaktcheck gilt;
dafür sind weder zusätzliche Unterteilung, Sparse-Assemblierung noch ein
numerischer Solver nötig. Alle Vertex-/Modulpaare werden gegen die gemeinsame
Ebene geprüft; der numerische Eckhöhenunterschied ergänzt die Chordschranke.
Die Verfeinerung reserviert vor einem weiteren Fit intern 20% der Höhen-/Port-
Approximation, beendet sich aber an den tatsächlichen Abnahmegrenzen. Alle
ursprünglichen Flächen und konkurrierenden Rahmen müssen erhalten bleiben.
Budgets begrenzen Eingaben, Variablen, Kegel/Sparse-Arbeit, Queries, Fits und
Geometrie. Zeit ist kooperativ; harte CPU-/Adressraumgrenzen setzt der Offline-
Worker. Clarabel bleibt optional; weder SDK noch Experimentimports sind nötig.
Keine globale Zweitableitungs-/Fahrdynamik-, C1-Boden-, Ebenennetz-/LOD-/native
Abnahme. Fachnachweis #169/PR #175, Paketübernahme #177, native Integration #48.

`transport.plan_vertical_transition(z0, z1, length, grade=0.08,
second_derivative=0.002, method="parabolic")` liefert einen unveränderlichen
`VerticalTransition` samt Bericht oder `None` mit lokalem Längengrund. Stationen,
Höhen und lokale Polynomkoeffizienten verwenden Meter; beide Endneigungen sind null.
Stückweise parabolisch/linear/parabolisch: minimale mögliche Länge unter den
eindimensionalen Neigungs-/Zweitableitungsgrenzen; bei vorgegebener längerer Strecke
wird die kleinste ausreichende maximale Zweitableitung gewählt. Alternativ kubisches
Smoothstep-Profil unter denselben Grenzen. Das ist keine Netzoptimalität oder
Fahrdynamik-Abnahme; die Zweitableitung hat die Einheit 1/m, keine Geschwindigkeitsbindung.
`minimum_transition_length(delta_z, grade, second_derivative, method=...)` gibt die
jeweilige Längengrenze; `evaluate(stations, derivative=0|1|2)` extrapoliert nicht.

`sample_vertical_transition(profile, height_error_m=0.01, max_vertices=4096)` liefert
Stations-/Höhenpaare oder begründete Abwesenheit vor der Vertexallokation.
Die linearen Höhenchords besitzen pro Polynomintervall die Schranke
`max(abs(z''))*step²/8`; Segmentgrenzen werden erhalten, Default-Querycap 65536.
C1 gilt für den analytischen Plan. Endliche Dreiecksflächen bleiben C0 und erhalten
begrenzte Facetten-/Endneigungssprünge; C2 an flachen Anschlüssen wird nicht behauptet.
Unterstützt: Höhen ±1e7 m, Länge 1e-6..1e7 m, Neigung 1e-9..1,
Zweitableitung 1e-12..1/m. Der konstante Aufwand der Planung umfasst höchstens drei
Polynomintervalle; SciPy-PPoly wird erst beim ersten Profilaufbau geladen.
Querneigung und der vollständige XY-Querschnitt verbrauchen eigenes Neigungsbudget.
Der Caller bindet Quell-ID, Frame, Deck/Boden, Anschluss und Reservierungen.
Gekrümmte Achsen, freie Anschlussneigungen, DEM-Innenfit und Ebenenplanung folgen
getrennt; `Plan`/`Reservation` und vorhandene Sampler bleiben erhalten (#20).

`transport.plan_endpoint_transition(z0, z1, length, start_grade, end_grade,
grade=0.08, second_derivative=0.002, method="envelope")` ergänzt ausdrücklich
vorgegebene Endneigungen bei fester Länge. Ergebnis: unveränderlicher
`EndpointTransition` mit Bericht oder begründete Abwesenheit. Dieselben Einheiten,
Eingabegrenzen und Query-/Chordbudgets wie oben; höchstens acht Polynomintervalle.
Die erreichbaren Grad-Hüllen sind
`lower=max(-G,g0-A*s,g1-A*(L-s))`,
`upper=min(G,g0+A*s,g1+A*(L-s))`.
Eine konvexe Mischung ihrer Integrale trifft die Endhöhe und erhält die Endgrade
sowie die ganzen Neigungs-/Zweitableitungsgrenzen. Das charakterisiert die skalare
Feasibilität in exakter Arithmetik; numerisch nicht zertifizierbare Fälle bleiben
ausgewiesen. Passende affine Kontakte werden direkt als Gerade erhalten, ohne
zusätzliche Biegung. Alternativ prüft `method="hermite"` direkt ein kubisches Hermite-Polynom;
es kann bei denselben Grenzen scheitern, obwohl die Hüllenkonstruktion gelingt.
Bei freien Endgraden ist Feasibilität über der Länge nicht monoton. Deshalb gibt
es hierfür keine bisektierte globale Mindestlänge. Der vorhandene flache Typ und
`minimum_transition_length` behalten ihren Vertrag.

`sample_vertical_transition` unterstützt beide Profiltypen. Die Endneigungen
betreffen nur die skalare Station; der Aufrufer muss ganze Kontaktquerschnitte,
Querneigung, Frame/Flächenart und Quellen prüfen. Der flach extrapolierende
`mesh_projected_profile` akzeptiert weiterhin ausschließlich `VerticalTransition`.
Vorhandene facettierte Netze erhalten durch dieses Einzelprofil keine C1-Ports;
variable Bank, gekrümmte Achsen und Netzplanung bleiben bei #20/#163.

`transport.mesh_projected_profile(domain, profile, origin_xy_m, direction_xy,
start_station_m=0, height_error_m=0.01, budget=ProjectedSurfaceBudget())` creates
a complete conforming height mesh for a polygon/multipolygon, including holes.
The analytic field is `z=h(dot(XY-origin,unit_direction)-start)` from an immutable
`VerticalTransition`, flat outside its endpoint interval. The report bounds the
whole face height error by each chord's `max|h''|*step²/8` plus an explicit numeric
guard; the finite mesh is C0. Complete domain coverage, one chord per face and
whole-face gradients are checked. Types/invalid values raise TypeError/ValueError;
resource/numeric geometry failure returns no mesh with a concrete reason.

`geometry.projected_height_surface(domain, origin_xy_m, direction_xy, height_table,
budget=ProjectedSurfaceBudget())` is the lower-level sampled-table operator.
An arbitrary table is only piecewise linear; it does not certify a C1 curve.
Source IDs, frame, surface kind and contact choices remain caller-owned.
`conform_triangles` optionally accepts `max_candidates`/`max_faces`; existing
default calls keep their outputs. Candidate query batches are capped before
allocation; numeric weld pairs are counted before the pair array is requested.

Default budget:2s cooperative offline phase,16384 boundary coordinates,4096
stations,8192 vertices/faces,65536 weld pairs and1048576 interface candidates.
GEOS steps can overrun a phase; this does not replace isolated worker time/memory
limits or establish a frame/device budget. A fully flat requested domain avoids
sampling unused profile intervals. No whole-world ground/deck selection, horizontal
route, driving, terrain deformation, collision or LOD claim. The same-source
Wiener #119 experiment imports this library; proof and migration belong to #122.

`geometry.vertical_clearance(lower, underside, max_pairs=65536, seconds=1.0,
max_faces=8192)` prüft den kleinsten vorzeichenbehafteten Z-Abstand über alle
positiven XY-Dreiecksschnittflächen. Beide Meshes sind ausdrücklich gewählte
affine Höhenflächen im gemeinsamen metrischen Datum; Frame/Kind/Quelle und
benötigte physische Freiheit bindet der Aufrufer. Es wird keine höchste Fläche
gewählt und keine vollständig abgedeckte Welt oder Fahrbarkeit nachgewiesen.

Ein vorhandenes Minimum liefert `valid`, `minimum_clearance_m`, `witness_xy`,
`face_pair`, Kandidaten-/Überlappungszähler und Laufzeit. Negative Abstände sind
numerisch gültig. Bei fehlender positiver Überlappung, degenerierter Projektion
oder Ressourcenende liefert der Operator `valid=False` mit konkretem Grund,
ohne unvollständiges Minimum als Zertifikat auszugeben. Punkt-/Kantenkontakt
ist keine positive Schnittfläche. Eingabefaces, tatsächliche Kandidatenpaare
und kooperative Zeit sind begrenzt; eine Abfrage alloziert höchstens
`max_faces` Indices. Ein Hard-Worker-Limit gehört weiterhin zum Aufrufer.

Ganzflächenminima können an neuen Schnittvertices liegen und werden nicht durch
Quellvertexstichproben ersetzt. Der Kernel bereitet Ebenengradienten einmal je
Face vor; alle Brücken-/Portalversuche importieren diesen installierbaren Kern.
[#146](https://github.com/Weichwolf/outshine/issues/146) besitzt Übernahme und
Vergleich; API-/Wheel-/Bild-/Kostenbelege stehen im zugehörigen PR.

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

Die Vegetationsbibliothek exportiert `vegetation.NeedledPlant`: bladeless
`PlantPlan`-Holzpläne mit optionalen `NeedlePattern`-Anordnungen je Achsenordnung
und physischen `Needle`-Prototypen. Nadeln erben keine Längenskalierung ihrer Eltern.
`Needle.mesh(..., axial_sampling="profile")` erhält beim Reduzieren axialer Ringe
das Radialmaximum und verteilt Segmente getrennt über Körper/Spitze. Der Default
`"uniform"` und alle physischen Rezepte bleiben unverändert. Die Ringzahl allein
belegt weder Bildschirm-, Coverage- noch Shadingfehler; Vergleich und Grenzen:
[#143](https://github.com/Weichwolf/outshine/issues/143).
`summary(path)` bestimmt Bedarf ohne Achsen-/Nadelarrays, `foliage(path, max_needles)`
liefert stabile Teilgruppen, wahlweise mit `retain=False` ohne interne Akkumulation
neuer Details; bereits gehaltene Gruppen dürfen wiederverwendet werden.
Default/IDs/Rezept bleiben. `tables(max_axes, max_needles)` liefert vollständige kanonische
Cachezeilen. Beide Budgets werden vor Detailarbeit geprüft; verweigerte Anfragen
liefern `None`. Holz- und Organ-IDs bleiben innerhalb ihres vollständigen Plans
stabil. Das `needled-plant-1`-Rezept enthält alle Quellen der prozeduralen Form.

`window_summary(nominal_span_m=0.08, max_groups=64)` zählt vollständige physische
Triebfenster ohne Achsen-/Organplatzierung. `windows(max_axes, max_windows, ...)`
prüft beide Budgets davor und liefert `None` bei Überschreitung, andernfalls eigene,
unveränderliche NumPy-Arrays in einer schreibgeschützten Zuordnung. Die Zeilen enthalten
Achsen-ID/-Ordnung, ersten Nadelindex und Anzahl, Vorlagenkapazität, Ursprung,
orthonormalen lokalen Achsrahmen, nominalen Elternparameter und Phyllotaxisphase.
Längen sind Meter im Pflanzenraum. Der Rahmen hat X quer, Y tangential, Z normal;
die Phase gehört noch nicht zum Rahmen. Bereiche decken alle kanonischen Nadel-IDs ab.
Die Fenster sind statistische Bindestellen für physisch große Vorlagen: Ihre Mittelpunkte
folgen den nominalen Knotenabständen, nicht dem Jitter einzelner Nahnadeln. Teilfenster
melden eine kleinere Anzahl; deren Deckung, Silhouette und Normalen sind damit nicht
bewiesen. Fenster expandieren keine Nadeln und ändern weder Nahrezepte noch Identität;
die begrenzten Holzachsen bleiben intern wiederverwendbar. Der Katalog-/Bildvergleich
steht bei [#168](https://github.com/Weichwolf/outshine/issues/168).

`AxisLevel.whorl_size` und `angle_tip` ergänzen Astquirle und den oberen Astwinkel.
Neue Semantik sowie Holzpläne ohne `Blade` verwenden `branching-plant-3`;
vorhandene Spiralrezepte behalten ihre v2-Rezepte und kanonischen Arrays exakt.
Die botanischen Sommerproben für vier Schweizer/europäische Nadelgehölze stehen
bei [#131](https://github.com/Weichwolf/outshine/issues/131). Kronendichte,
entfernungsabhängige Darstellung, reife/Jahresformen und botanische Freigabe sind offen.

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

`buildings.interiors.Furnishing(kind, width_m, depth_m, height_m, activity_depth_m)`
beschreibt einen vollständigen Möbelgrundriss mit notwendiger Aktivitätsfläche.
`layout(room, portals, requests, identifier, reserved=(), method="wall-anchors",
max_candidates=64, beam_width=1, objective="short-paths")` platziert in einem
gelieferten Raum, ohne Portal-/Nutzungsfreiräume zu blockieren. Alternative:
`jittered-anchors`; die deterministische Breitsuche hält höchstens acht Zustände.
Die Programme vergleichen dieselben Anforderungen mit `beam_width=1|4` und
`objective="open-activity"` (Weglänge und überlagerte Aktivitätsflächen).
Keine globale Optimalitätsbehauptung. Fehlende Objekte bleiben samt Grund erhalten.
Vollständige Polygonabdeckung statt Eckentest; 0,30-m-Figurzentrum muss alle
Portal- und Aktivitätsziele im freien Raum erreichen. Wandseitige Aktivitätszone,
bei `meeting-group` ringsum. Logische Platzierung vor tatsächlicher Mesh-Emission;
3D-Höhen, Boden, Kopf- und Treppenfreiheit prüft der Aufrufer am konkreten Mesh.
XY-Eingaben sind Meter im gemeinsamen lokalen Rahmen; der Aufrufer besitzt
Quell-/Raum-IDs, Nutzung und Höhendatum. Eigene unveränderliche Koordinatenarrays;
keine impliziten Anbieterabfragen oder Welt-/Cachezustände. Kapazitäten:
256 Raumringkoordinaten, 16 Portale, 32 Objekte, 256 Kandidaten je Objekt,
40×40 Routingraster. Das sind Größenlimits, keine harte CPU-/RAM-/Frame-Garantie.
`access` und `placed` ermöglichen unabhängige Prüfungen und explizite Platzierung.
Programme, eigene Möbel, reale Gegenfälle und Bild-/Kostenbelege:
[#174](https://github.com/Weichwolf/outshine/issues/174).

`buildings.modelling.PrismVolume(identifier, polygon, low_m, high_m, material=1)`
beschreibt ein vollständiges ENU-Prisma in Metern. Polygon/MultiPolygon und
Höhen bleiben geliefert; der Konstruktor repariert oder rastert sie nicht.
`union_prisms(sources, max_triangles=262144)` entfernt innere Gegenflächen und
liefert `PrismUnion`: unveränderliche native Vertices/Dreiecke/Material-IDs,
Vertexidentitäten und eine stabile Tabelle aus Konstruktion-IDs samt Quellindices
und ursprünglichen Eingabedreiecken. Interne Bibliotheks-IDs werden nicht exportiert.
Leere Polygone bleiben in der Tabelle und erhalten `empty_polygon`; ungültige
Geometrie, doppelte IDs und Kapazitätsüberschreitungen werden ausdrücklich abgewiesen.
Input-/Output-Trianglecaps sind keine harte Laufzeit- oder Gesamt-RAM-Schranke.
Die Vereinigung und Triangulierung rechnen im gemeinsamen lokalen XY-Batchrahmen;
zur Ausgabe kehren Vertices in den deklarierten ENU-Rahmen zurück. Quellhöhen,
stabile Konstruktion-IDs und Materialherkunft bleiben erhalten. Dadurch entstehen
bei verschobenen Gebäuden keine zusätzlichen Mikrokonturen aus ungünstigen
Weltkoordinaten. Kontrollen, reale Gegenfälle und Kosten: [#142](https://github.com/Weichwolf/outshine/issues/142).
Das optionale Extra `pip install -e './lib[solids]'` verwendet Manifold3D 3.5.4.
Diese Konstruktion erhält eigene geplante Material-/Bandherkunft und ersetzt
keine allgemeinen CGA-Shape-UVs, Auto-Tags oder Material-Overrides.
Vertrag und Gebäudebelege: [#124](https://github.com/Weichwolf/outshine/issues/124).

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
`HipPlanner` besitzt als expliziter Desktop-Kontext einen wiederverwendbaren
Spawn-Prozess für dieselben Pläne. Eine Anfrage zugleich; begrenzte private
Nachrichten, externe Anfragedeadline, `cancel()` und vollständiges Verwerfen
der Verbindung nach Timeout/Abbruch. `close()` beendet den Prozess; Reaping
hat ein eigenes begrenztes Budget. Prozessstart wird separat gemessen und kann
auf OS-Ebene selbst nicht immer unterbrochen werden. Diese Referenz ist für
Desktop-Erzeuger, keine iOS-/Frame-Garantie; direktes `prepare_hip` bleibt kooperativ.
`hip_footprints(shape)` liefert die intrinsischen Quellflächen. Deren vorbereitete
`HipPlan`-Tupel können mit `Shape.roof_hip(..., prepared_plans=plans)` für mehrere
Höhen/even-/negative und positive Distanzvarianten ausgegeben werden, auch nach Schließen
des Workers. Faceanzahl und exakte intrinsische Domäne werden gebunden geprüft;
die Ausgabe wiederholt keinen Skelettsolver. Meshausgabe bleibt separat budgetiert.
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
Interpretation und keine SDK-Kontur-/Bottom-/UV-Gleichheit.
Positive `overhang`-Werte erzeugen eine kontinuierliche CDT-Dachschürze zwischen
der ursprünglichen Traufe und einem GEOS-Miter-Offset mit Limit 5. Die Eingabe
misst Dachlänge; die horizontale Distanz ist `overhang / hypot(1, slope)`.
Das innere Standard-/even-Dachfeld bleibt erhalten. Außenfacetten übernehmen
affine UV0–9 der zugeordneten ursprünglichen Traufe; ihr Chartbesitz ist beim
Schnitt auf diese Außenfacette begrenzt. Kleine Raster-Konfliktzellen werden
bis `4 * precision * perimeter` nach größter Schnittfläche zugeordnet.
`shell_depth_m=0.1` legt den Boden explizit unter die tiefste äußere Traufe;
positive Dicke verhindert Nullstärke am ursprünglichen Rand. Innere/äußere
Ränder schließen diese Hülle. `roof_info.exteriors` hält Domäne, Außenfacetten,
Bodenniveau und Raster pro Quellfläche. Beliebige planare Quellframes und
vorbereitete Pläne sind unterstützt; mehrere Quellflächen bleiben eigenständige
Hüllen, ihre Schnittvereinigung ist nicht behauptet. Die Dachschürze ist ein
eigenes Verfahren, keine SDK-Überstands-/Tessellierungs-/UV-Gleichheit.
Genaue SDK-Gratgruppen-Semantik und Trim-Planes bleiben bei
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

### Complete canonical plant plans

`PlantPlan(seed, levels, blades)` uses immutable `AxisLevel` orders and `Blade`
variants. Recipe version `branching-plant-2` binds stable UInt64 axis/leaf IDs;
the two ID domains remain separate. Organ geometry uses X transverse/Y forward/Z
normal; plant positions/frames use local ENU in metres. Circle-axis geometry is
available as `outshine_ref.vegetation.axis_surface.axis_mesh` with owned Float64
vertices/corner normals and Int32 faces; `bark=True` adds physical bark fields.
This is an analytical form
model; it does not implement the complete SpeedTree spine or joint solver.

`summary(path)` returns counts, variant area and a conservative bound without
creating axis or leaf placements. `axis(path)` requests ancestors as needed.
`leaves(path, max_leaves)` and `expand(max_axes, max_leaves)` preflight complete
requested counts; insufficient capacity returns `None` before placement work.
Budgets are Python integers in [0,100000]. These row bounds are not a total
memory/time guarantee. `Shoot.detail` shares the same bounded placement contract.
Public `AxisState` frames must be finite, orthonormal and right-handed. Shared
axis/leaf/shoot arrays have immutable bytes, and leaf mappings are read-only.

`tables(max_axes, max_leaves)` returns cached complete immutable rows: axis IDs,
parent IDs (-1 for root), seven-slot Int16 paths (-1 padding), origins, frames,
lengths, bends, parent parameters and radius pairs; leaf IDs, owner-axis IDs,
UInt8 variants, nodes, origins, frames and stem parameters. Numeric dimensions
are Float64; IDs are UInt64, parent IDs Int64. Camera and mesh detail do not
change these rows. The experiment's `canonical-plant-reference-1` manifest binds
the recipe, rows, full mesh/placement levels and procedural material files.
World instance identity/placement, source reservations and terrain remain the
caller's responsibility. See [#86](https://github.com/Weichwolf/outshine/issues/86)
for cache/material semantics, controls, costs and open botanical/LOD/wind limits.

### Frond guides, source patterns and cross-section curl

`PinnatePattern` creates own planar pinnate sources with stable organ IDs and
anchor locations, directly at the requested subdivisions. `FrondGuide` applies
physical circular guidance, width/height/roll profiles and a quadratic transverse
fold. Its recipe remains `frond-guide-1`; old source/guide arrays are preserved.
Generated products own their arrays and reject unresolved face/corner orientation.

`FrondCurl(curvature_per_m)` bends the cross-section around the local Y axis,
preserving Y and the complete X=Z=0 midline. Positive/negative curvature bend
opposite ways. Stable sinc expressions preserve the neutral k=0 case and avoid
subtracting nearly equal cosines. Points require |kX|<π and 1−kZ>0; the continuous
map is injective on this open single-turn domain. Curled mesh triangles must span
at most π/2 in kX and agree with every transformed corner normal. Oversized meshes
refuse before deformation; bounds are 1,000,000 vertices and 2,000,000 triangles.
These row bounds are not a time or application-memory guarantee.

`deform(mesh, method="cofactor")` preserves source faces, parameters/UVs and organ
IDs in owned outputs. `method="solve"` independently evaluates inverse-transpose
normals with NumPy. Call `guide.deform(curl.deform(source))` to combine the two
maps; direct near/coarse sources retain their anchors. Source patterns and guides
have complete recipes, curl uses `frond-cross-curl-1`. The central rib is a
separate own tube subjected to the same deformation. This is not an original
SpeedTree rib/property implementation, complete original section-variation system or
botanically verified species recipe; these limits belong to
[#95](https://github.com/Weichwolf/outshine/issues/95).

`LeafletShape(bend, fold, twist, variance)` adds prepared parent-position profiles
and stable seed/organ-ID variation to `PinnatePattern(leaflet_shape=...)`. Each
local blade uses analytic differential normals; identical shapes share a prototype
within one mesh request. Unique forms still require individual evaluations.
The profile/variation bounds certify regular blades and conservatively shorten
organs before meshing to keep their entire source surface inside Y∈[0,length].
`source_extent` bounds transverse/height coordinates for downstream guides.
Positive uniform scaling and rotation preserve local normals and anchor positions.

Unchanged default patterns retain `pinnate-source-1`, old identities and exact
arrays. Shaped patterns use `pinnate-source-2` with a complete `leaflet-shape-1`
recipe; seed/organ-ID samples are independent of camera, request order and mesh
subdivision. Near/coarse common points coincide. This supplies own local shape
variation, not SpeedTree's complete Section/Gravity/force system or a botanically
verified plant recipe. Contract and equal-output costs belong to
[#100](https://github.com/Weichwolf/outshine/issues/100).

`SpatialFrondGuide(spine, width, height, roll, fold_per_m, intervals_per_segment)`
binds the same source surfaces to an immutable `CubicSpine`. The local guide
starts at the origin with positive Y tangent. `ArcMeasure` and `SpineField`
prepare the metric coordinate and transported frame once, independently of detail.
`spatial-frond-guide-1` records the complete curve, profiles and preparation size.
Shared `deform` preserves source topology, UVs and organ IDs in owned outputs;
its scalar evaluator is a same-output cost reference.

The Jacobian differentiates the actual prepared map, including the ratio of
curve speed to interpolated arc speed, frame curvature/roll and transverse
profile derivatives. It does not assume an exact arc-length parameterization.
Conservative curvature × transverse extent < 1 certifies positive local
orientation before evaluation. This permits spatial bends and inflections,
but does not prove global injectivity, collisions or general LOD coverage.
The rib follows the same map as its leaflets; generator parent frames and
bounds consume the spatial guide. Circular guide arrays remain unchanged.
`ArcMeasure.fraction` now returns exact 0/1 at both endpoints to prevent
roundoff from producing an out-of-domain coordinate. Proof and limits:
[#103](https://github.com/Weichwolf/outshine/issues/103).

`GrowthSpine(length, fields, initial_direction)` prepares a complete spatial axis
from local geometric growth influences. `DirectionField` and softened
`AttractionField` carry immutable rate profiles in m⁻¹; optional `RadialFalloff`
uses linear, squared or step attenuation in metres. Raw directions survive JSON
replay; normalization belongs to their prepared values. World influences must
first be expressed in the chosen local axis frame.

The own tropism model integrates P′=T and T′=F−(F·T)T. Constant direction has an
analytic tangent and bounded Gauss integration; other fields default to RK45.
DOP853 is an independent comparison. Prepared scalar callbacks avoid repeated
small NumPy operations; public batch fields provide the reference evaluation.
This controls growth shape and does not implement elastic weight/stiffness.

`prepare(stations=33, max_evaluations=4096)` returns `GrowthResult` with a C1
`CubicSpine`, method/work counters and measured approximation errors. Lengths
are 1 µm–1000 m, at most 16 fields and 65 stations, length × rate bound ≤16.
Insufficient work budget or unresolved station detail returns `None`.
Four samples per interval check position error ≤5×10⁻⁶ length and tangent error
≤2×10⁻⁴ rad against integration samples. These are sampled comparisons, not
global bounds between probes. `growth-spine-1` records the full authoring recipe;
cache the prepared spine and complete mesh/material products for solver-free
detail/replay. Contract, reference and costs: [#105](https://github.com/Weichwolf/outshine/issues/105).

`CurlField` bends around an axis expressed in the moving lateral/tangent/normal
frame. `GnarlField` rotates the heading around a fixed axis in the declared local
space. `PlanarField` projects the heading onto its plane; `ReturnField` biases
growth toward the program's initial direction. All use immutable rate profiles
and optional radial attenuation. Zero cross products remain zero and introduce
no arbitrary direction. These smooth field magnitudes differ from SpeedTree's
normalized cross products; full property/frame-mode equality is unproven.

Curl additionally integrates a transported lateral vector A with A′=−(A·T′)T.
Its initial reference defaults to X and must be independent of the initial
heading; `initial_reference` records another explicit choice. Constant smooth
turning fields use measured DOP853 preparation; varying profiles use RK45.
Independent public batch queries take `headings` and, for curl, `lateral` arrays.

New fields/references serialize as `growth-spine-2`; unchanged direction and
attraction programs retain exact v1 recipes and prepared curves. Curve and
frame norms, independent circle/helix/plane solutions, covariance and replay
belong to [#114](https://github.com/Weichwolf/outshine/issues/114).
Cross-section twist remains controlled by the existing roll profiles; original
Twist-force modes, geometry containers/collisions, mechanics and wind remain open.

### Procedural leaf materials and diffuse transmission

`LeafOpticsTable(values, limits=(80, 10))` owns immutable raw linear-sRGB R/T
samples on a 3–65 square log1p pigment grid. Coordinates are chlorophyll and
anthocyanin in µg/cm². `sample(Nx2)` interpolates before jointly reducing gamut;
reflection and transmission retain their luminance and max(R)+max(T) ≤1.
`leaf-optics-table-1` recipes include values, limits, channel order and colour
semantics. Leaf kernels use NumPy; the package's common contracts also import
Shapely. Lookup has no spectral, solver or renderer dependency. External PROSPECT-D spectral evaluation
and CIE/D65 integration remain experiment preparation, with provider parameters
and provenance retained separately; PROSPECT N is a structural parameter.

`leaf_pigments.pigment_coordinates` uses stable seed pairs and physical XY metre
positions, independent of batch/order/mesh subdivision. Twelve bounded waves
produce 8–40 mm pigment patches; vein relief modulates chlorophyll. These are
own styling choices, not calibrated seasonal or botanical laws. Pigment queries
outside the configured table domain explicitly reject.

`leaf_normals` maps height derivatives through the actual surface metric,
filters slope/second moments and fourth-power perceptual roughness, and resolves
mean normals with isotropic variance broadening. Covariance validation permits
roundoff from existing Float32 moment caches; original valid outputs stay exact.
This approximation does not preserve multimodal or anisotropic highlight lobes.

`leaf_material_mips(optical, coverage, moments)` returns owned immutable
`LeafMaterialLevel` products from square power-of-two images up to 512². Both
six-channel R/T and six-channel normal moments are premultiplied by coverage.
Filter them jointly with coverage, then `resolve()` divides at positive coverage
and returns reflection, transmission, normal, roughness and slope covariance.
Empty texels resolve to zero optical response; they cannot darken or tint the
visible leaf at filtered borders. R/T energy and mean coverage survive box mips.
`leaf-material-fields-1` example assets store all levels, UV/frame, source organ,
optical parameters and coverage role; they contain no baked runtime light.
This is a Python product schema, not a native binary ABI.

`leaf_diffuse_response(optical, normals, views, lights)` evaluates the own
reciprocal thin-sheet baseline per unit incident radiance:
R·max(n·L,0)/π + T·max(−n·L,0)/π, with the geometric normal facing the camera.
Directions point from surface toward viewer/light. Illumination, shadow
visibility and coverage are runtime inputs. The baseline uses symmetric sides,
hemispherical R/T and no directional thickness/BSSRDF model or specular lobe.
`optical_mixture` produces bounded reflection/transmission colours and a convex
Blender preview weight; pure reflection, pure transmission and zero cases work.
Principled-plus-translucent is a preview approximation: R/T are not calibrated
MR base colour/Fresnel parameters, and blindly adding specular can change energy.
Native GLSL bindings, lobe energy allocation, atlas/geometry response and GPU/world
proof belong to master. Contract and evidence: [#121](https://github.com/Weichwolf/outshine/issues/121).

## Building structural interpretation

`buildings.structures.classify_structure` consumes one valid XY polygon in ENU
metres, delivered `render_height`/`render_min_height`/`hide_3d`, optional `class`/
`subclass`, coordinate quantization and union coverage of supplied industrial
areas. Immutable `StructureDecision` records kind, origin, alternatives, reason,
height/footprint metrics and caller-owned context evidence IDs. Source inputs are
unchanged. Names, locations and IDs cannot select a kind. Tile fragments,
courtyards, elongated houses and ordinary rectangular high-rises retain their
building interpretation. Compact shafts, round elevated sections and round
industrial bodies receive plausible technical families. Inferred types are
unproven; water/communication/occupied towers and industrial process functions
remain ambiguous without a delivered class. Defaults of 9 m/0 m are procedural.

`classify_structure_marker` recognizes a delivered POI structural class, or
returns `None`; conflicting class/subclass values remain ambiguous. It supplies
no dimensions, host assignment or whole-building type. The caller must retain
the marker and resolve its spatial/floor scope before choosing a recipe. The
supported vocabulary does not assert that OpenFreeMap actually delivers every
listed class. [#130](https://github.com/Weichwolf/outshine/issues/130) owns source
sampling, counterexamples and the measured interpretation limits. These functions
do not generate meshes, establish native asset ABI or prove global accuracy.

## Complete radial solids

`buildings.modelling.RadialProfile(identifier, centre_xy, knots, materials)` is a
complete camera-independent local ENU plan. Knots are increasing metre heights,
outer radii and inner radii. Solid endpoints may collapse to cone tips; through
channels require strictly positive separated radii throughout. Mixed/pinched
channels reject. The immutable plan retains its stable identifier and full curve.
This constructor uses ENU, unlike CGA shape scopes in the same namespace.

`radial_mesh(profile, sides=32, error_m=None, max_sides=256,
max_triangles=262144)` produces immutable corner buffers, explicit topology roots,
outer/inner/end material roles, cylindrical/cap UVs and unit corner normals. Sharp
height breaks/end rings remain separate normal boundaries. Counts are checked
before emission. Signed volume/surface are measured on the actual polygonal mesh;
the plan's circular volume is a separate analytic reference. No original CGA
UV/group/normal/ABI equivalence is asserted.

When `error_m` is supplied, directly reduce the complete profile and choose angular
sides. Piecewise-linear radial deviation plus polygon sagitta bounds the reported
metre error at every height. End heights, channels and a necessary interior anchor
between two solid tips survive reduction. The caller converts projected quality to
metres before emission; the logical profile stays unchanged. These are complete
radial products, not visibility-clipped shells or a native cache/LOD ABI.
[#136](https://github.com/Weichwolf/outshine/issues/136) owns geometry/binding/cost
proofs. Trimesh.revolve is an equal-output comparison; its absolute small-area
cutoff leaves a valid tiny tube open. The own indexed path preserves that geometry
without position welding. Runtime/device budgets remain unproven.

## Closed heightfield envelopes

`heightfield_envelope(top, base_m, wall_material=1, base_material=0,
wall_uv_scale_m=(1., 1.))` closes an existing upward ENU triangular surface.
It reuses the actual top triangles, preserves upper UV0–9, normals and face
metadata, adds the mirrored lower surface and vertical walls only along actual
domain boundaries. Existing edge-connected fans retain explicit identities;
point-contact domains remain separate. There is no interior body/roof cap and
no generic CGA derivation-tree evaluation.

`HeightfieldEnvelope` holds immutable corner buffers, topology roots, original
upper-triangle indices, top/lower/wall roles, boundary edges, domain WKB and the
exact integral of the piecewise-linear top above the lower plane. Newly created
surfaces use metre UVs and explicit material overrides; their CGA tag mask is
zero. Material tables, arbitrary channels and native cache/LOD ABI are not
inherited implicitly.

Input projections must be upward, resolved and non-overlapping. Unmatched
interior edges/T junctions, a base touching the upper surface, missing vertices,
invalid channel bindings and unsupported fields reject. Domain overlap tolerance
is 1e-8 m²; boundary residual tolerance is 1e-8 m, checked in a local XY frame.
[#152](https://github.com/Weichwolf/outshine/issues/152) owns actual provider roof
controls, independent closed composition/height slices and build-cost comparison.

## Prism openings in closed surfaces

`subtract_prisms(surface_id, surface, sources, max_triangles=262144)` subtracts
the union of declared `PrismVolume` cutters from a closed ENU mesh with explicit
`vertex_roots`. It works with `HeightfieldEnvelope`, preserving a sloped roof
while cutting actual rooms, doorways and windows below it. Coordinates are
encoded in a local XY frame; exact topology roots remain separate from position
welding.

Immutable `PrismCut` holds the corner mesh, topology roots, ordered source IDs,
source index and original triangle per output face, omissions, volume and area.
Source zero is the original surface. Original UV0–9 interpolate across source
triangles; cutter UVs are explicit zeros, cutter materials follow their declared
volume and normals are rebuilt from actual output triangles. Empty cutters are
reported; complete erasure returns an empty mesh. Stable source relations are
independent of backend ID allocation.

Groups, CGA tags/resources, authored normals and recursive Shape Boolean
semantics remain outside this primitive. Unknown property fields reject;
documented face metadata is not inherited implicitly. Input/output cardinality
checks do not prove a backend time or memory bound. The optional `solids` extra
provides Manifold3D. [#156](https://github.com/Weichwolf/outshine/issues/156)
owns independent structure/slice, real opening/actor and source/wheel checks;
general CGA Boolean conformance remains
[#88](https://github.com/Weichwolf/outshine/issues/88).

## Compact facade coverage

`FacadeRepeat(width_m, height_m, columns, rows, opening_width, opening_height)`
stores immutable metrical axes. `coverage(centres_m, footprints_m)` returns the
exact area mean of centred rectangular openings inside an explicit rectangular
filter. The finite facade domain contributes zero outside its bounds. No window
geometry, derivation tree or texture bitmap is expanded.

`rectangle_coverage(centres, footprints, fractions, counts=None)` evaluates the
same field in unit-cell coordinates; omitting counts selects an infinite
periodic domain, including negative coordinates. Fractions allow zero and one.
Counts are positive uint32 values on `FacadeRepeat`. Invalid dimensions,
filter footprints and domain bindings reject. A fully contained small box
returns exactly one instead of subtracting nearly equal antiderivatives.

Independent explicit rectangles verify area, finite boundaries, subpixel
queries, periodic translations, whole-domain energy and replay. The previous
experiment `facade_plan.coverage` imports this core. Rectangular coverage is a
declared surface filter, not an EWA, shaded image-error or native LOD bound.
[#161](https://github.com/Weichwolf/outshine/issues/161) owns context-scoped real
tile facade plans, physical CPU binding and costs; full programmes/near room
transitions remain #31/#33, native integration #48.
