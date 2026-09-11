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

Lay validiert vollständige kanonische Koordinaten, nichtnegative endliche Parameter
und kumuliertes Punktbudget vor Mutation; Corridors reicht Fehler weiter.
Factory prüft positive endliche Radien/Zellen und darstellbare 32-Bit-Indizes.
Within/Nearest liefern expected, sperren nach Lay bis Weave und bewahren Fehlerpuffer;
große Suchen wählen vor Cast den Vollscan. Leeres Netz bleibt gültig abfragbar.
Tests/Gegenproben: Einfügefehler/Recovery, Konfigurationsgrenzen, unabhängige sphärische
Trefferzahlen/nächste Distanz, Pol/Datumsgrenze/DBL_MAX. Vollständiger alter Überlaufpfad
scheitert, isolierter Cast hier nicht. conventions instrumentiert Engine nicht mit
Sanitizern: Nachweis offen. Letztes lint 187/333, 32 Claims grün; Wien-PNG bytegleich.

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

ReconstructRoute mit benanntem RouteTrace: Kette bis kMaxRouteLegs zählen, dann einen
Leg-Puffer anlegen. Rückwärts direkt in endgültige Reihenfolge schreiben; temporäre
Knotenliste und reverse entfallen. Metrische Stationen separat vor Veröffentlichung
prüfen; Überlauf als Fehler, keine teilweise veröffentlichte Route. Private expected-
Funktion trennt Suchzustand von Ergebnis. Analytische meridionale Dreipunktkette
prüft Reihenfolge/Stationen/Länge/Attribute; entfernte Reihenfolge muss scheitern.
Vier Routingtests grün, vertauschte Reihenfolge scheitert; lint 187/333, 32 Claims grün.
Offen: globales Routenbudget als Laufzeittest; Alternativpfade bei Kostenüberlauf.
Kein produktiver Router-Aufruf im Client-Renderpfad.

RouteSearch kapselt temporäre Kosten/Vorgänger/Queue und geliehene Endpunktmengen.
Plan bleibt Validierung/Anbindung/Diagnose/Rekonstruktion. Seed, Expand und Offer
trennen Initialisierung, zulässige Turns und Kostenupdates. Unendlich statt 1e30;
überlaufende g/f-Kosten nicht einreihen, andere endliche Pfade weiter suchen; ohne
Ergebnis numerischen Fehler von fehlender Verbindung unterscheiden. Analytische
skalierte Kugel mit endlichen Wegen >1e30 widerlegt den alten Sentinel. Routingtests,
Gegenprobe, make lint; keinerlei neue öffentliche API oder dauerhafter Suchzustand.
Ergebnis: fünf Routingtests grün; endliche Kosten >1e30 und tatsächlicher Überlauf
unterschieden, alter Sentinel scheitert. Benannte Candidate-Felder verhindern
Index-/Vorgängerverwechslung. Plan-Komplexitätsbefund entfernt: lint 186/333,
32 Claims grün. Keine neuen Render-Abhängigkeiten oder dauerhaften Suchdaten.

## Geprüfte Layer-Grenze
StreetField castet double ungeprüft und ignoriert textuelle Layer. OsmField erhält
Integer: fehlend optional, Text vollständig parsen, Zahl endlich/ganzzahlig/int32.
StreetField zählt/verwirft ungültige Features; fehlend bleibt 0. Featureaufbereitung
von Tile-Fortschritt trennen. MVT-Fixtures prüfen Grenzen, Brüche, Text und Wiederholung.
