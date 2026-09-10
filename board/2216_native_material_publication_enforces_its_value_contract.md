Type: defect
State: active
Parent: 2150
Area: scene, base, import, render
Tags: validation, materials, ownership
Depends:

# Native material publication enforces its value contract

## Erreichter Vertrag
MaterialValidation.h prüft Faktoren gemäß include/scene/Material.h, AlphaMode,
Sampler-/UV-Enums, finite UV-Transformationen und owner-lokale Bildbindungen.
HDR-Emission, IOR 0 und positive unendliche AttenuationDistance bleiben zulässig;
Schichtdicken sind nichtnegativ und geordnet. Keine stillen Clamps/Ersatzmaterialien.
Geometry::wellFormed prüft vollständige Material-/Meshprodukte einschließlich
zugewiesener Materialindizes. Engine::setGeometry prüft vor der Übernahme.
SubjectDraw::ValidateMaterials bleibt getrennte Geräte-/Renderpass-Fähigkeitsprüfung.

setSurface liefert expected<void, MaterialError>: fehlender Slot und ungültige
Werte/Bindungen unterschieden, alle Prüfungen vor nichtallokierender Kopie. Fehler
bewahren vorherige Werte/Namen/Indizes. Kein vorwärtsreferenzierender Ersatz.

Subject::Handed liefert expected<Geometry,string> mit Asset-/Mesh-Phasen und reicht
geprüfte Bild-/Material-/Vertex-/Indexsetter-Fehler weiter, statt Teilprodukte auszugeben.
Der Importer bindet Texturen und sampelt Materialfaktoren auf diesem Kandidaten;
Handed wird erst danach ersetzt. Keine zusätzliche Geometriekopie.
Kameraposen haben getrennte Arbeits-/Publikationspuffer; bei Erfolg Swap. Fehlgeschlagene
Clip- und Variantenwahl stellt die vorige Auswahl wieder her. Geometrieansichten,
veröffentlichte Kameras und Clipdauer bleiben bei den geprüften Fehlern erhalten.

addSurface liefert expected<MaterialInstance, MaterialError>. Intrinsische Werte,
Enums und UVs sowie int-/Container-Kapazität werden vor Kopie/Allokation geprüft.
Anlegefehler erhalten Bestand, Namen und Slotvergabe. Vorwärtsreferenzen auf Bilder
bleiben beim Aufbau erlaubt; vollständige Publikation verlangt aufgelöste Bindungen.
Import-/Generator-/Geländeaufrufer reichen Anlegefehler weiter. Models und Corridors::Lay
melden Fehler; Ground-Kandidaten werden bei diesen Fehlern nicht zum Renderer übergeben.

## Verbleibende Arbeit
- Indexkapazitätsgrenze unabhängig prüfen, ohne Milliarden Materialien anzulegen.
  Die aktuelle Prüfung begrenzt den int-Materialzähler und vector::max_size;
  sie ist keine Laufzeitprüfung unter tatsächlicher Speichererschöpfung.
- Verbleibende Kopier-/Indexverengungen und Bindungsauflösung im Importadapter auditieren.
  Gemeinsame native Regeln für Importer, Generatoren und direkte API-Aufrufer.
- Späte Fehler im gesamten Weltaufbau prüfen: frühere Änderungen an World.Pieces und
  anderen Begleitdaten sind durch das Verwerfen des Geometry-Kandidaten nicht zurückgerollt.
  Vollständige Transaktion einschließlich aktiver Welt durch unabhängige Fehlerfälle belegen.
- Alle Werte-/Bindungskombinationen und Corpus-/Generatorprodukte prüfen.
  Vollständige Asset-/Instanzmigration bleibt WI 2150; Runtime-Ausnahmen WI 2194.

## Import-Vorprüfung
Subject::Assemble prüft lokale Indizes und Attribute jetzt vor Clear und partieller Kopie.
ValidatePart/ValidateAssembly liefern expected; destruktives Refuse wird vor Mutation vermieden.
Vorflight für alle Parts: vorhandene Attributregeln plus lokale Indexgrenzen,
Gesamtvertex-/Komponentenkapazität und uint32-Adressierbarkeit vor Datenänderungen.
Konservative Obergrenze: Eingangsvertices + zweimal Indexanzahl, weil Normalen- und
Tangentenbildung jeweils höchstens einen Clone pro Ecke anlegen; keine Vorausallokation
dieser Obergrenze. Größenrechnung geprüft, reale Speichererschöpfung nicht nachgewiesen.
Erst danach Attribute kopieren; affine Platzierung als getrennte Phase nach Mat4-
Vertrag (Punkte, inverse-transponierte Normalen, Tangenten und gespiegeltes Winding).
Negativfall: zweiter Part mit ungültigem Index erhält vorherige Daten/Ansichten;
gültiger Retry und vorhandene unabhängige Spiegelungs-/Skalierungsoracles sind grün.
Altcode verletzt den Erhaltungstest; leerer Input und falsche Attributlängen ebenfalls geprüft.
Keine zusätzliche Geometriekopie. Spätere Normalen-/Tangentenfehler und Append bleiben
separate Transaktions-/Kapazitätslücken; keine vollständige Importtransaktion behaupten.

## Abnahme
- [x] Publikations-/Ersatzfehler, Quellerhaltung und gültiger Retry durch native Tests geprüft.
- [x] Fehler spät in Materialanimation erhält frühere Materialien und Geometrieansichten.
- [x] Fehlgeschlagene Probe/Clipwahl erhält Kameras, Clipdauer und Nutzbarkeit der alten Auswahl.
- [x] Variante mit fehlender Textur wird abgelehnt; folgende Probe nutzt vorherige Auswahl.
- [x] Anlegefehler erhalten Materialbestand/Namen/Indexvergabe; gültiger Retry geprüft.
- [ ] Kapazitätsgrenze unabhängig geprüft.
- [ ] Alle intrinsischen Grenzen und Bindungskombinationen unabhängig geprüft, einschließlich
      HDR, IOR-Sonderfall, +infinity-Absorptionsdistanz, NaN, ungültige Enums und Schichtreihenfolge.
- [ ] Später Import-/Generatorfehler publiziert kein Teilprodukt und erhält aktive Welt.
- [ ] Khronos-Corpus und Generatorprodukte bleiben gültig; Bildänderungen mit PNG-Orakeln
      prüfen, keine Referenzanpassung zur Kaschierung von Fehlern.
- [ ] make format, passende Tests und make lint/clang-tidy ohne Suppression.

## Nachweise
Native Materialpublikation/-ersatz, Importkonvertierung, Asset-Roundtrip, Baumgenerator,
Animation, native Bilder/UVs und Platzierung als Regressionen geprüft. Negativkontrollen
verletzen Erhaltungs-/Publikationsoracles ohne Buildfehler. Variantenfall: sieben Checks
grün, Altcode scheitert am anschließenden Retry. Kamera-/Clipfall: 15 Checks grün,
Altcode verletzt vier Garantien. Anlegeprüfung: Negativkontrolle ohne Werteprüfung
scheitert an den Vertragschecks, nicht am Build; danach sieben gezielte und dreizehn
Regressionstests grün (ein gemeinsamer Fall, zusätzlich validierter Gerätearm).
Materialübernahme als eigene Importphase; vier Importregressionen einschließlich
Khronos-Texturtransformationen grün. Keine vollständige Corpus-/Weltabnahme.
Letzter Lint: 178 tidy, 251 Dokumentationsdiagnosen, 32 Repository-Tests grün;
drei rote Gruppen bleiben. Einzelverläufe stehen in Git, nicht als fortlaufendes Tagebuch.

## GroundMaterials-Katalog
Load leert vor IO den Katalog und publiziert Klassen vor vollständiger Validierung.
Datei über ReadTextFile mit 1-MiB-Katalogbudget lesen; JSON/Klassen/Referenzen als
Kandidat aufbauen, erst bei Erfolg ersetzen. Klassen-Decoding von Katalogauflösung
trennen; gültige Materialarithmetik unverändert. Negativkontrolle: später Klassen-/
Referenzfehler erhält alle alten Materialien, fehlende/überlange Dateien ebenso.
Numerische Material-/Modellgrenzen und unbekannte Litter-Verweise bleiben separat offen.
