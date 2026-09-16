Type: defect
State: active
Parent: 2150
Area: scene, base, import, render
Tags: validation, materials, ownership
Depends:
Architecture: ready
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

## Nächster ausführbarer Schritt: Import-Transaktion

`src/import/Subject.cpp` und seine Assemble-/Append-Implementierungen bleiben
Importadapter. Native `Geometry` und MaterialValidation besitzen die gemeinsamen
Werte-/Bindungsregeln; keine zweiten Khronos-Regeln in der Runtime implementieren.
Zuerst die späten Fehlerpfade von Normalen-/Tangentenaufbau und Append verfolgen.
Vorflight allein garantiert keine Transaktion: alle potenziell fehlschlagenden Schritte
müssen vor Mutation veröffentlichter Geometrie/Ansichten abgeschlossen sein.
Bestehenden Importkandidaten verwenden; kein Snapshot-Rollback und keine weitere
vollständige Geometriekopie nur zur Fehlerkaschierung. Borrowed Views bleiben bei
Ablehnung gültig, bei Erfolg gilt der dokumentierte Invalidierungsvertrag.

Abnahme unter `test/outshine/src/import/Subject/`: gültiges Asset A, Fehler erst im
zweiten Part oder in abgeleiteten Normalen/Tangenten, Geometrie/Material-/Kamerawerte
von A unverändert, anschließend gültiger Retry. Test muss gegen alten fehlerhaften
Pfad scheitern. Kann ein vermuteter Fehler nicht entstehen, belegte Vorbedingungen
festhalten statt einen künstlichen Defekt zu behaupten. Kapazitätsrechnung vor
Verengung mit vorhandenen nativen Hilfen; reale OOM bleibt fatal gemäß WI 2194.
`make format`, diese Importtests und `make lint`; bei veränderter gültiger Geometrie
zusätzlich glTF-Clientrender mit gepinntem Bildvergleich, keine neuen Referenzpins.

Ground-Gesamtpublikation gehört WI 2224; Material-/Shaderausbau und Assetmigration
bleiben getrennt. Anschließend fehlende intrinsische Werte-/Bindungskombinationen
gegen `include/scene/Material.h` ergänzen, nicht vorhandene Tests duplizieren.

## Native-Konstruktionsfehler
`addImage` liefert expected mit Maß-/Bytezahl-/Kapazitätsfehler; Erhaltung und Retry sind geprüft.
`addLamp` liefert expected mit Kapazitätsprüfung; native/Import-/Spotlichtprüfungen grün.
`addPart` liefert expected. Gemeinsame Zählerprüfung: Null-, Container-, int- und size_t-Grenzen geprüft.
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
- [x] Gemeinsame Index-/Containergrenzen mit synthetischen Zählern geprüft.
- [ ] Alle intrinsischen Grenzen und Bindungskombinationen unabhängig geprüft, einschließlich
      HDR, IOR-Sonderfall, +infinity-Absorptionsdistanz, NaN, ungültige Enums und Schichtreihenfolge.
- [ ] Später Import-/Generatorfehler publiziert kein Teilprodukt und erhält aktive Welt.
- [ ] Khronos-Corpus und Generatorprodukte bleiben gültig; Bildänderungen mit PNG-Orakeln
      prüfen, keine Referenzanpassung zur Kaschierung von Fehlern.
- [ ] make format, passende Tests und make lint/clang-tidy ohne Suppression.

## Nachweise
Native Materialpublikation/-ersatz, Importkonvertierung, Animation, Bilder/UVs,
Platzierung und Generatorprodukte durch Regressionen und Negativkontrollen geprüft.
Einzelne Nachweise stehen in Git; vollständige Corpus-/Weltabnahme bleibt offen.
## GroundMaterials-Katalog
Katalog lädt bounded in einen Kandidaten. Werte, Namen, Vorwärtsreferenzen und
Float-Verengungen werden vor Publikation validiert; Fehler erhalten den alten Katalog.
Analytische optische Grenzen, Retry und ausgelieferter Katalog sind geprüft.

## Geometrische Katalogwerte
Metrische Größen, Detailpaare und Hangintervalle validiert; Fehler erhalten Bestand.
GroundSurf/LitterSurf sind vorbereitet, Detailmaßstäbe aber noch nicht im Shader
integriert. Keine Behauptung eines geprüften prozeduralen Mikroreliefs.

## Animation und Kurven
Pose::Build bereitet Ruhepose, Kanäle und Kurven als Kandidat vor; Spans behalten
stabile Channel-Besitzer. Konflikte, Fehlererhaltung und Retry sind geprüft.
Formatunabhängige Keyframes in base/math: geprüfte span-Factory, endliche streng
steigende Zeiten/Werte, gültige Interpolation und Sample-Grenzen. Analytische
STEP/LINEAR/CUBICSPLINE- und Importerprüfungen bestehen. Abgeleitete Überläufe und
Rotationsnormen bleiben separat zu prüfen; keine vollständige Animationsabnahme.
