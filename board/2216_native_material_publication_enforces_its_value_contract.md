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

setSurface liefert expected<void, MaterialUpdateError>: fehlender Slot und ungültige
Werte/Bindungen unterschieden, alle Prüfungen vor nichtallokierender Kopie. Fehler
bewahren vorherige Werte/Namen/Indizes. Kein vorwärtsreferenzierender Ersatz.

Subject::Handed liefert expected<Geometry,string> mit Asset-/Mesh-Phasen und reicht
geprüfte Bild-/Material-/Vertex-/Indexsetter-Fehler weiter, statt Teilprodukte auszugeben.
Der Importer bindet Texturen und sampelt Materialfaktoren auf diesem Kandidaten;
Handed wird erst danach ersetzt. Keine zusätzliche Geometriekopie.
Kameraposen haben getrennte Arbeits-/Publikationspuffer; bei Erfolg Swap. Fehlgeschlagene
Clip- und Variantenwahl stellt die vorige Auswahl wieder her. Geometrieansichten,
veröffentlichte Kameras und Clipdauer bleiben bei den geprüften Fehlern erhalten.

## Verbleibende Lücke
Geometry::addSurface kopiert noch ungeprüft und verengt die Slotzahl auf int.
Subject::Flatten/CopyNativeAssets ignorieren seinen Rückgabewert. Engine::State::Models
ist void; Laying nutzt Materialindizes unmittelbar. Structures/Corridors sowie
TreeGeometry/CrownAtlas legen Generator-Materialien an. 15 C++-Testdateien verwenden
addSurface. Material-/Indexfehler dürfen nicht zur Defaultoberfläche werden.

## Nächste vollständige Migration
- addSurface als expected<MaterialInstance, MaterialError>; MaterialUpdateError zum
  gemeinsamen Fehlervertrag erweitern. Intrinsische Werte/Enums/UVs und Indexkapazität
  vor Allokation/Kopie prüfen; Fehler verbraucht keinen Slot/Namen.
- Bildreferenzen bei vollständiger Asset-Publikation prüfen: Subject::Flatten legt
  Materialien vor Bildern an, Handed kopiert Bilder vor Materialien. Legitime
  Vorwärtsreferenzen beim Aufbau erhalten; keine zweite Materialrepräsentation.
- Import-/Generator-/Geländeaufrufer vollständig auf Fehlerweitergabe migrieren.
  Keine unchecked Dereferenzierung, value_or-Defaultmaterialien oder erfolgsmeldende
  leere Geometrie. Später Fehler darf kein teilweise erzeugtes Produkt veröffentlichen.
- Verbleibende Kopier-/Indexverengungen und Bindungsauflösung im Importadapter auditieren.
  Gemeinsame native Regeln für Importer, Generatoren und direkte API-Aufrufer.
- Öffentliche Fehler-, Ownership-, Invalidierungs- und Kostenverträge aktualisieren.
  Vollständige Asset-/Instanzmigration bleibt WI 2150; Runtime-Ausnahmen WI 2194.

## Abnahme
- [x] Publikations-/Ersatzfehler, Quellerhaltung und gültiger Retry durch native Tests geprüft.
- [x] Fehler spät in Materialanimation erhält frühere Materialien und Geometrieansichten.
- [x] Fehlgeschlagene Probe/Clipwahl erhält Kameras, Clipdauer und Nutzbarkeit der alten Auswahl.
- [x] Variante mit fehlender Textur wird abgelehnt; folgende Probe nutzt vorherige Auswahl.
- [ ] Anlegefehler erhalten Materialbestand/Namen/Indexvergabe; Kapazitätsgrenze geprüft.
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
Altcode verletzt vier Garantien. Keine vollständige Corpus-/Weltabnahme behauptet.
Letzter Lint: 180 tidy, 282 Dokumentationsdiagnosen, 32 Repository-Tests grün;
drei rote Gruppen bleiben. Einzelverläufe stehen in Git, nicht als fortlaufendes Tagebuch.
