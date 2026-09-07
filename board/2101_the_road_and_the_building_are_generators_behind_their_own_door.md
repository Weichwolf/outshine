Type: debt
State: active
Area: engine, generators
Tags: webcam, measured
Depends: 2133, 2121

# Generators derive separate map, alignment and structure products

## Stand und Grenze

`Generators::Corridors` und `Corridors::MapOf` existieren; Teile der Ableitung sind bereits
in `src/generators/road/`. Das ist noch keine vollständige Trennung von logischer Karte,
räumlichem Alignment, Geländeänderung, Darstellung und Kontakt.

| Produkt | Inhalt | Verbraucher / Lebensdauer |
|---|---|---|
| logische Karte, 2133 | Topologie, Ebenen, Modi, Regeln, stabile IDs | Navigation/NPC/Map; unabhängig von Render-LOD |
| Alignment, 2175 | Lage/Höhe/Querschnitt/Strukturanschlüsse | Lane-Follower, Render-/Kontakt-Bake; daten-/regelabhängig |
| Structure | Mesh/Material/Bounds/Stamp-Anfrage | Renderer/Terrain; sichtbarkeitsabhängige LOD |
| Kontakt, 2127 | analytische Flächen oder Kollisionsgeometrie | Physik; eigener Fehler-/Nähebedarf |

Gemeinsame IDs/Version, keine gemeinsamen Mesh-Indizes als API. Engine orchestriert
Daten/Snapshots/Budgets; Generator besitzt Konstruktion. Karte ohne Renderer nutzbar,
Render-Tile kann verschwinden, während NPC-Route bleibt. Keine Route aus sichtbarem Mesh.

Offen: Corridors/Laying/StructureBake auf diese Produkte umstellen, Whole-ring-Bau durch
Tile-/Struktur-Jobs mit Nachbarschaftshalo ersetzen; 2124 besitzt Scheduling/Upload.
Die alte pauschale „Straße kann wegen Skirt nie schweben“-Lösung ist aufgehoben:
Fundament/Stützwand nur wo konstruktiv sinnvoll; Brücke/Tunnel müssen darunter offen bleiben.

- [ ] Headless-Kartenquery benötigt weder Mesh noch Renderer; identische Route bei jeder LOD.
- [ ] Straße, Schiene, Weg, Brücke und Tunnel gehen durch denselben versionierten Produktvertrag;
      entsprechende Konstruktionen/Fixturen in 2175, keine Feature-spezifischen Engine-Mesher.
- [ ] Ein geänderter Tile/Anschluss invalidiert nur seine abhängigen Produkte; Version-Mismatch
      scheitert am Snapshot-Oracle. Keine halb aktualisierte Kontakt-/Bildkombination.

Wahl: PCG-/Streaming-Trennung wie öffentliche Unreal-Konzepte, explizite logische Karte
wie vereinbart. RAGE bleibt Ergebnisbenchmark, kein behaupteter interner API-Vertrag.
