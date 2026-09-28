Type: chore
State: open
Architecture: planned
Priority: P2
Area: generators, world
Tags: measured, determinism
Depends:

# Generation seeds follow stable world identity and explicit streams

## Problem und vorhandene Fähigkeit

Historischer Audit 2026-09-04 findet lokale Mixer in BuildingShape, Structures,
AlpineLimit und TreeRandom sowie Tile::Seed. Ähnliche 24-bit-Ausgaben beweisen weder
identische Zuständigkeit noch einen Fehler. Aktuelle Caller vor Migration erneut prüfen.
Ein Zustandsstream für Baumwachstum und ein positionsbasierter Hash sind verschiedene
Verträge. Ihre bloße Vereinheitlichung rechtfertigt keinen weltweiten Bildwechsel.

## Entscheidung

Seeds stammen aus deklarierter Szenario-/Weltidentität, stabiler Objektidentität und
expliziter Generator-/Streamversion. OSM-Objekte nutzen stabile Quellen-IDs; flächige
prozedurale Population nutzt kanonische Weltzellen/Positionen mit deterministischer
Grenzzuordnung. Kamera, Place-Fixture-ID, Arrival-Reihenfolge und Workerzahl sind keine Seeds.
Ein OSM-Objekt darf beim Wechsel seiner Streamingkachel nicht neu gewürfelt werden.
Für zustandsabhängige Streams bleibt die Ziehungsreihenfolge explizit und versioniert.

Zuerst tatsächliche Instabilität oder inkonsistente Grenzzuordnung reproduzieren.
Dann kleinste gemeinsame Seed-Ableitung in base/generators mit benannten Inputs festlegen;
kein generischer Random-Service und keine unbelegte Behauptung über RAGE-Innereien.
Unterschiedliche PRNGs dürfen bleiben, wenn ihr Vertrag und ihre Nutzung begründet sind.
Seed-/Algorithmuswechsel invalidiert abgeleitete Caches und wird als bewusster visueller
Versionswechsel abgenommen. Unveränderte Seeds bewahren bestehende Produkte.

## Widerlegbare Abnahme

- [ ] Dasselbe Objekt nach anderer Arrival-Reihenfolge, Workerzahl, Kamera und
      Partitionierung liefert identische deklarierte Eigenschaften/Geometrie.
- [ ] Nachbarkacheln haben weder doppelte noch fehlende grenznahe Population.
- [ ] Unabhängige feste Seed-Vektoren; Kamera-/Counter-basierte Mutation verursacht FAIL.
- [ ] Änderungen an Weltseed oder Generatorversion wirken deterministisch und
      invalidieren nur betroffene Produkte. Kein Runtime-Sonderpfad für Place-Namen.
- [ ] Betroffene Client-PNGs öffnen und begründete Unterschiede nennen; fokussierte
      Orakel, format und full lint. Reproduzierbare Bytes allein beweisen keine Verteilung.
