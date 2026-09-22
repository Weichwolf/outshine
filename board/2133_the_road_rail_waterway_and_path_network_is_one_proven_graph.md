Type: feature
State: active
Area: world, navigation
Tags: webcam, measured
Parent: 2188
Depends: 2173

# A logical transport map preserves connectivity independently of rendered geometry

## Verbindliche Trennung, 2026-09-07

Die logische 2D-Karte dient Navigation, NPCs, Fernsimulation und Kartenanzeige. Sie trägt
IDs, Verbindungen, Modi, Fahrtrichtung, Zugangs-/Abbiege-/Spurregeln, Layer und Strukturbezug.
Sie benötigt kein Render-Mesh. 2D bezeichnet ihre planimetrische Darstellung, nicht einen
planaren Graphen: Überführungen kreuzen sich im Bild und sind topologisch getrennt.
Die frühere Forderung „2D AND FLAT“ sowie gemeinsame Knoten an Brücken/Quais ist so korrigiert.
Ein Bahnübergang koppelt Konfliktregeln, erlaubt aber keinem Auto, aufs Gleis abzubiegen.

`StreetField.cpp` zählt im aktuellen Pfad numerisch markierte Tunnel und überspringt sie
per `continue`. Rendering-Ausblendung darf niemals einen Verkehrsweg aus der logischen
Karte entfernen. `Path::Network` in `src/base/spatial/Wayfinding.*` und
`Corridors::MapOf` sind vorhandene Träger, noch kein vollständig geprüfter multimodaler Router.

## Implementierung

1. OSM-IDs/Relations und Modi road/rail/walk/cycle/water samt Einschränkungen erhalten.
   Knoten aus expliziter Konnektivität; XY-Nähe oder Segmentkreuzung allein verbindet nicht.
   Fehlende IDs im Providervertrag lösen, quantisierten XY-Snap nicht als ID ausgeben.
2. Crossings separat: verbundenes Junction, grade-separated crossing, kontrollierter
   Bahnübergang, Furt, Transfer. `layer` ist Ordnung, keine Höhe in Metern. Tunnel bleibt
   routbar, auch wenn keine Oberflächengeometrie sichtbar ist.
3. Stabile Tile-/Rand-IDs und versionierte Snapshots; Graph-Tiles unabhängig von Render-LOD
   halten und für Routen laden. Kein Anspruch, den gesamten Weltgraph im RAM zu halten.
   Fernrouting über Hierarchie/Overlay, lokales Lane-/Turnrouting nach erlaubtem Modus.
4. Abgeleitetes räumliches Alignment in 2175: (Edge-ID, s, t) → Position/Tangente/Träger.
   Logische Route bleibt bei Höhen-/Mesh-Verfeinerung identisch. NPC-Lane-Follower verwendet
   Alignment; freie Fußgänger benötigen zusätzlich lokale begehbare Flächen/Off-mesh-Links
   (2127), nicht ausschließlich einen Straßengraphen.
5. Kartenansicht ist eigener Consumer: Brücke/Tunnel/Ebene als Symbolik; kein Render-Mesh
   zurück in Navigation rasterisieren. Geometrie darf Darstellung vereinfachen, keine Turns ändern.

## Abnahme

- [ ] Kreuzung, Überführung, dreistöckiges Autobahnkreuz, Bahnübergang, Tunnel mit Abzweig,
      Treppe/Fußweg, Einbahnstraße und Turn-Restriction als kleine unabhängige Fixtures.
- [ ] Route/Erreichbarkeit bleiben bei ausgeschaltetem Renderer, Mesh-LOD-Wechsel und
      Tile-Eviction der Darstellung identisch. XY-Snap über Ebenen erzeugt roten Shortcut-Test.
- [ ] Verbotene Turns/Moduswechsel unmöglich; erlaubter Tunnel bleibt erreichbar.
      Randnaht-/Richtungsfehler haben eigene Negativkontrollen, Komponenten werden begründet.

Wahl: [SUMO OSM-Import](https://sumo.dlr.de/docs/Networks/Import/OpenStreetMap.html) als
lesbare Semantikreferenz, [ASAM OpenDRIVE](https://www.asam.net/standards/detail/opendrive/)
für getrennte Road-/Lane-Links und Alignment. Unreal/RAGE dienen als Sandbox-Benchmark;
kein Ableiten des logischen Netzes aus einem sichtbarkeitsabhängigen Mesh.

## Aktueller Router und verbleibende Verträge

2173 blockiert Providerintegration, nicht lokale Graphkorrekturen. Oneway und Splicing
bewahren gerichtete Kanten; Index dedupliziert physische Segmente, lose Enden richten
sich nach physischer Nachbarschaft. Einbahn-/Abzweigmatrix und Gegenproben bestehen.
WeakComponents verwendet Union-Find; Reaches bleibt gerichtet. OSM reverse/-1 im Adapter
normalisieren. Referenz: https://wiki.openstreetmap.org/wiki/Key:oneway.

Plan nutzt h(n)=max(0,d(n,Zielzentrum)-Zielradius): konsistente untere Schranke durch
Dreiecksungleichung, analytische Mehrziel-Gegenprobe besteht. Breite Graphen zusätzlich
gegen Dijkstra prüfen. Freie Startseeds in 250 m können Barrieren/Fahrtrichtungen
überspringen; explizite zulässige Anbindung bleibt offen. A*-Referenz:
https://www.boost.org/doc/libs/1_61_0/libs/graph/doc/astar_search.html.

Lay/Factory und räumliche Abfragen besitzen Validierungs-/Recovery-Tests für
Koordinaten, Budgets, Indizes und sphärische Suche. Diese lokalen Belege ersetzen
keine Sanitizer-, Streaming- oder vollständige Router-Abnahme; Verlauf steht in Git.

Offen: OSM-IDs/Modi/Restrictions/Streaming statt Legacy-Snap; Zustandsverträge anderer
Graphabfragen, Trefferidentitäten/Rasterränder, TieReach aus Straßenbreiten, Budget/
Abbruch für Routing und vollständiger atomarer Graphaufbau. ApartM hat iterative
Längengradnormalisierung; ungültige direkte Eingaben können nicht terminieren.

## Router-Verantwortlichkeiten

LocalTurnAllowsRadius trennt die lokale Kreisbogennäherung von A*. atan2 erhält kleine
Winkel; t=R*tan(theta/2) begrenzt den Radius bei reservierten halben Nachbarkanten.
Gespiegelte kleine/rechte Winkel und exakte Gerade geprüft; acos-Gegenprobe scheitert.
Kein Fahrbarkeitsnachweis: sphärische Tangenten, Spurbreite, Clearance, verbundene Kurven
und Klothoiden bleiben 2175. Referenz für Linien/Bögen/Spiralen:
https://www.asam.net/fileadmin/Standards/OpenDRIVE/ASAM_OpenDRIVE_BS_V1-7-0.html.

ReconstructRoute prüft Stationen vor Veröffentlichung; RouteSearch kapselt Suchzustand
und unterscheidet große endliche Kosten von Überlauf. Analytische Routingkontrollen
bestehen; globales Laufzeitbudget und Alternativpfade bei Überlauf bleiben offen.
Kein produktiver Router-Aufruf im Client-Renderpfad.

Grounds erneuert World.Network bisher nur bei geänderter Wegeanzahl. Auch bei
gleicher Anzahl neu bauen, wenn publizierte gegenüber angeforderter Region,
Vektorgeneration, Straßen-Tilezahl oder DEM-Residenz wechselt; `MapOf`-Fehler
müssen den Kandidaten ablehnen, statt ein leeres Netz zu publizieren.
Elevate übernimmt derzeit NaN/Inf und ruft auch leere HeightSource auf. Eingabegrenze
prüft optional + isfinite; ungültig zählt wie fehlend als Refused (vorige Weghöhe/0).
Profile vor/nach Weave: 138 Checks zu ±Höhen, NaN/±Inf, fehlender/leerer Quelle.
Ohne Endlichkeitsprüfung scheitern 20 Checks; Routenstationen bleiben geprüft.
Jede Knotenhöhe einmal je Elevate abfragen, auch fehlende; Node.HeightM entfällt.
Drei Tests grün, wechselnde Quelle verletzt im alten Code drei Checks; Wien pixelgleich.
Revisionsinvalidierung und explizite Gültigkeit statt Ersatzhöhe bleiben offen.

## Geprüfte Layer-Grenze
OsmField::Integer trennt fehlend, gültig und Fehler; vollständiger Dezimaltext oder
endliche ganze int32-Zahl. StreetField zählt/verwirft ungültige Features, fehlend ist 0.
Tile-Fortschritt und Featureaufbereitung getrennt; Fixture-Checks und Gegenprobe geprüft.

## Modulgrenze aus Strukturaudit

src/base/spatial/Wayfinding.* enthält geographische Strecken, Breite, Steigung und
Radiusregeln. Transportregeln und Netzbesitz nach src/world/navigation trennen; nur
allgemeine Graphsuche, räumliche Indizes und Mathematik bleiben base. Vor Verschiebung
Generator-Consumer inventarisieren: world darf nicht zurück auf generators zeigen.
Logisches Netz ist der native Vertrag; Road-Generator liest es für Geometrie, ohne es
zu besitzen oder Render-LOD zur Navigationsentscheidung zu machen. Bestehende analytische
Routing-/Topologieprüfungen migrieren; keine neue Suchstrategie in diesem Schritt.
