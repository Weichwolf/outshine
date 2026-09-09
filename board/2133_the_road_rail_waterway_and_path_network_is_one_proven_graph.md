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

## Aktueller Router: vor Integration korrigieren

2173 blockiert vollständige Providerintegration, nicht lokale Graphkorrekturen.
EdgesFromWays berücksichtigt Oneway; Kantenindex und Aufteilung erhalten Richtung.
Referenz: https://wiki.openstreetmap.org/wiki/Key:oneway — Richtung folgt der
Punktreihenfolge; reverse/-1 muss im Importadapter ausdrücklich normalisiert werden.
Eine einzelne gerichtete Gerade muss nur vorwärts routbar sein; Gegenprobe ist
identische Geometrie ohne Oneway. Mehrere Kanten, Umkehr der Punktfolge und gemischte
Knoten prüfen. Keine Rendergeometrie erforderlich.

SpliceInto prüft vorhandene Richtungen vor Mutation und teilt nur diese auf. Der
Kantenindex dedupliziert physische Segmente unabhängig von Knotenreihenfolge; lose
Enden werden über physische Nachbarschaft erkannt, nicht über Ausgangsgrad. Nähe darf gemäß Zielmodell
keine OSM-Verbindung erfinden; die Ablösung des Legacy-Snaps bleibt offen.

Plan akzeptiert mehrere Zielknoten, verwendet als Heuristik aber Distanz zum einzelnen
nächsten Zielknoten. An anderen akzeptierten Zielen kann h>0 sein: Optimalität nicht
bewiesen. Gegenbeispiel mit unterschiedlich langen Wegen zu mehreren Zielkandidaten
gegen unabhängige Dijkstra-Lösung konstruieren; zulässige Heuristik zur Zielmenge.
Start-Reichweite von 250 m kann ebenfalls Barrieren/Fahrtrichtungen überspringen;
explizite zulässige Anbindung statt freier räumlicher Seeds erforderlich.

TransportNetworkPreservesOneWay prüft die gerichtete Gerade und eine Abzweigmatrix:
beide Punktreihenfolgen, Ein-/Zweirichtungs-Hauptstraße, ein-/ausgehender Spur. Routen
und genaue Kantenzahlen bestehen. Alte Indexfilterung und alte Splice-Funktion
scheitern jeweils ohne Buildfehler. WeakComponents trennt physischen Zusammenhang
von gerichtetem Reaches; OSM-IDs/Modi/Restrictions/Streaming und Startanbindung bleiben offen.

Komponenten: InPieces verwendet gerichtete BFS und globales Seen; konvergierende
Einbahnzweige werden dadurch je Knotenreihenfolge in falsche Teilnetze zerlegt.
Als WeakComponents/ComponentStatistics benennen: physischer Zusammenhang ohne
Fahrtrichtung, getrennt von Reaches. Union-Find mit Pfadkompression und Union nach
Größe benötigt O(V) Scratch, keine zweite Adjazenzliste. Tests für konvergierende
Einbahnzweige, Richtungsumkehr, getrennte Komponente, Einzelknoten und leeren Graph.

Mehrziel-A*: Gegenbeispiel mit zwei direkten Zielkanten und Fortsetzungen verhindert
Legacy-Splicing. Kugelgeometrie unabhängig per acos(cos(lat)*cos(lon)) geprüft;
der alte Router wählt ca. 1112 m statt ca. 1015 m (R=6371008,8 m).
Heuristik h(n)=max(0,d(n,Zielzentrum)-Zielradius). Alle akzeptierten Ziele liegen
im selben Radius wie Within; Dreiecksungleichung liefert h(n)<=d(n,jedes Ziel).
Die Schranke ist konsistent und kostet O(1) je Bewertung, auch bei vielen Zielen.
A*-Referenz: https://www.boost.org/doc/libs/1_61_0/libs/graph/doc/astar_search.html.
Test in beiden gespiegelten Lagen; ursprüngliche Heuristik muss scheitern. Dies
beweist nicht die Zulässigkeit der bestehenden räumlichen Start-/Zielanbindung.
