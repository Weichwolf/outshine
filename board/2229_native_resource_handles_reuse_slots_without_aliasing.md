Type: defect
State: active
Parent: 2224
Depends:
Architecture: ready
Area: engine, render, test
Tags: ownership, handles, memory, streaming

# Native resource handles reuse slots without aliasing GPU addresses

## Befund und Ziel

`Live::{Pieces_,HeightPages_}` wächst pro Aufnahme; Release leert Nutzdaten, aber keine
Slots werden wiederverwendet. Kandidaten kopieren/scannen historische Einträge.
`HeightSheets::TileOf` kodiert native Page-ID als Float in `Render::GroundTile`;
`Live::PublishesGroundLattice` wandelt zurück und ersetzt sie durch die GPU-Adresse.
Damit vermischen sich native Identität und Renderer-ABI. Slotreuse ohne Generation
würde alte Handles auf neue Ressourcen umleiten.

## Festgelegte Architektur

- Interne, unterschiedliche `PieceHandle` und `HeightPageHandle` in `src/engine/`.
  Slot als uint32, Generation als uint64; ungültiger Default, typisierte Gleichheit,
  keine implizite Integer-/Float-Konvertierung. Gültigkeitsbereich ist die native
  Weltlinie einschließlich ihrer Ersatzkandidaten, nicht eine fremde Engine/Deklaration.
  Ressourcenhalter bei neuer Deklaration vollständig zurücksetzen; keine globalen IDs.
- `Live` besitzt Slotmetadaten, Freiliste und Payloads. Lookup prüft Bereich, belegt
  und Generation. Release verwirft Payload und GPU-Resident, erhöht Generation und
  gibt Slot frei; wiederholtes Release/alter Handle ist wirkungslos. Generation darf
  nie umbrechen: ausgeschöpften Slot sperren, Kapazitätsfehler explizit zurückgeben.
- Upload vor Slotpublikation. Fehlgeschlagener Upload verändert belegte Slots und
  Generationen nicht; vorbereitete temporäre GPU-Ressourcen werden freigegeben.
  Kandidaten kopieren native Slotidentitäten/Freiliste, rekonstruieren GPU-Residents
  ausschließlich für belegte Slots. Abbruch verändert das Original nicht.
- Renderer behält seine eigenen `Render::PieceId`/`PageId`. Nur Live übersetzt gültige
  native Handles; kein Generationsteil in Shaderdaten und kein Shader-ABI-Umbau.
- Native Ground-Tile-Beschreibung enthält typisiertes PageHandle separat von den
  geometrischen Instanzdaten. `HeightSheets`, Live-Snapshots und ihre Prüfsummen nutzen
  diese Beschreibung. Erst beim Upload entsteht `Render::GroundTile`; GPU-Page muss
  vor Float-Konvertierung exakt darstellbar sein. Ungültig/zu groß bedeutet Fehler.
- Freiliste begrenzt normale Metadaten auf die maximale gleichzeitige Belegung,
  nicht die Zahl der historischen Aufnahmen. Ein absolutes Engine-Speicherlimit
  folgt separat aus WI 2228; diese Änderung darf keines behaupten.

## Umsetzung in vollständigen Schritten

1. Neue Aufnahmefunktionen liefern `[[nodiscard]] expected<Handle, string>` mit
   besitzendem Fehlertext; ungültiger Handle ist kein zweiter Fehlerkanal.
   Piece-Handle samt Slots in `Live.h/.cpp`; alle Consumer `TilePieces`, `CrownPieces`,
   `CrownAtlas`, `PieceRows` migrieren. `Render`-interne IDs unangetastet lassen.
2. HeightPage-Handle, native Tile-Beschreibung, `HeightSheets.h/.cpp`, Live-Ground-
   Snapshots und Übersetzung migrieren. Suche nach allen `Render::PageId`/`.Page`-
   Verwendungen im Engine-Verzeichnis; keine heimliche Float-Zwischenrepräsentation.
3. Kandidaten/Rebinding und optionale Speicherdiagnostik vervollständigen. Kleine
   interne Slotverwaltung nur für tatsächlich gemeinsame Logik; kein Public-Pool-API.

## Widerlegbare Abnahme

- Kleine analytische Slotfixture: A anlegen, freigeben, B im selben Slot mit anderer
  Generation; alte Mutation/Freigabe darf B nicht verändern. Typen nicht vertauschbar.
- Wiederholte Aufnahme/Freigabe mit fester Spitzenbelegung: Slotanzahl und reservierte
  Metadaten wachsen nach Warmup nicht weiter, CPU-Payload nach Release null.
- Ungültig, out-of-range, zweimal Release, Generation nahe Maximum, Uploadfehler und
  unmittelbarer Retry. Kleine synthetische Grenzen statt Milliarden Schleifendurchläufen.
- Belegten Snapshot vorbereiten, verwerfen, erneut vorbereiten und publizieren;
  alte gültige Handles bleiben verwendbar, freigegebene bleiben ungültig.
- Ground-Seite mit ungültigem Handle erhält alte Lattice. Renderer-Floatgrenze
  analytisch testen, keine riesige GPU-Allokation zur Erzeugung der Grenzadresse.
- Bestehende Live-/GroundWorldCandidate-/StructureTilePublication-Tests erweitern;
  GPU-Neuvergabe erzwingen, damit versehentlich gleiche Indizes Fehler nicht verdecken.
- `make format`, fokussierte Suiten, `make lint`; Graz ohne Vegetation pixelgleich
  bei identischer Eingabe. PNG öffnen; abweichende Pixel vor Abschluss erklären.
