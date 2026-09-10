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

Plan verwendet eine konsistente untere Entfernungsschranke zum gesamten Zielbereich.
Analytische Direktkanten widerlegen die frühere Einzelzielheuristik; breitere Graphen
mit Umwegen/Turn-Regeln zusätzlich gegen unabhängige Dijkstra-Lösung prüfen.
Start-Reichweite von 250 m kann ebenfalls Barrieren/Fahrtrichtungen überspringen;
explizite zulässige Anbindung statt freier räumlicher Seeds erforderlich.

Vorhandene Nachweise: TransportNetworkPreservesOneWay prüft Richtung und Splicing;
TransportComponentsIgnoreDirection prüft Union-Find gegen konvergierende gerichtete
Zweige. TransportSearchUsesAllGoals widerlegt die alte Einzelzielheuristik mit
analytischen Direktkanten in zwei gespiegelten Lagen. Alte Implementierungen scheitern.
Heuristik h(n)=max(0,d(n,Zielzentrum)-Zielradius) ist durch Dreiecksungleichung eine
konsistente untere Schranke; sie legitimiert nicht die räumliche Zielanbindung.
A*-Referenz: https://www.boost.org/doc/libs/1_61_0/libs/graph/doc/astar_search.html.

Routing-Eingabegrenze: Plan validiert vor ApartM/Nearest/Within beide Koordinaten
(endlich, Lon [-180,180], Lat [-90,90]) und Mindestradius (endlich, >=0).
Fehler liefert leere Route mit Diagnose; gültige Folgeabfrage bleibt nutzbar.
NaN/±Inf, Bereichsverletzungen und negative Radien testen. Tieferliegende Lay-,
Sphere-/Snap-, Nearest-/Within-Verträge separat härten; ApartM hat noch iterative
Längengradnormalisierung, die für nichtendliche Eingaben nicht terminiert.

Lay erhält nodiscard expected<void,string_view>: mindestens zwei vollständige
Lat/Lon-Paare, endliche kanonische Koordinaten, nichtnegative endliche physische
Way-Parameter und nichtnegative Spurzahl. Kumuliertes Punktbudget vor Zugriff/
Allokation prüfen; Fehler erhalten bestehenden Graph und Bereitschaft. Corridors
reicht Lay-Fehler und ungültige Punktspannen weiter, statt Wege still zu verlieren.
Tests für späte ungültige Koordinate, odd/empty/short, Parameter, Budget und Recovery.
Validen Wien-Aufbau zur Integrationskontrolle ohne Vegetation rendern.

## Nächster Schritt: gültige räumliche Indizes ab Erzeugung

Befund: Network(Snap,Sphere) akzeptiert Null/NaN/Inf. RowOver/ShapeRowOver casten
ungeprüfte Quotienten nach int64; KeyAt packt Zeile und Spalte in je 32 Bit.
Within castet ceil(reach/Snap) und quadriert die Zellzahl vor dem Vollscan-Fallback.
Eine gültige Konstruktion allein verhindert diesen unabhängigen Abfrageüberlauf nicht.

Entscheidung: private Konstruktion, nodiscard expected<Network,string_view>-Factory;
Corridors reicht Erzeugungsfehler weiter. Positive endliche Basiswerte und abgeleitete
Größen prüfen; darstellbare Zellindizes aus der tatsächlichen Bitbreite herleiten.
Auch grobe Zellen, Polnähe und das aus Straßenbreiten abgeleitete TieReach prüfen.
Within/Nearest erhalten explizite Fehlerverträge; große gültige Suchradien wählen
vor Integer-Casts einen begrenzten Vollscan. Keine stillen leeren Treffer bei Fehlern.
Vorhandene Graph-/Geometriefähigkeiten behalten; erwartetes gültiges Render unverändert.

Abnahme: Null, negative Werte, NaN/±Inf, Subnormale, maximale Double-Werte und
abgeleitete Überläufe; gültige Grenzfälle samt Pol/Datumsgrenze. Räumliche Treffer
gegen unabhängigen Distanz-Vollscan prüfen. Entfernte Eingabeprüfung muss scheitern;
kein Integerüberlauf im Sanitizer. Routingtests, make lint und Wien ohne Vegetation.
