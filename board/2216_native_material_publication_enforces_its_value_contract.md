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

## Import-Transaktion

`Subject::Assemble` baut jetzt vollständig in einem lokalen Subject und übernimmt ihn
nur durch nachweislich nichtwerfenden Move. Vorflight und spätere Normalen-/Tangentenarbeit
können damit keinen publizierten Owner mutieren; der Fehlertext bleibt beim alten Owner erhalten.
Keine Geometriekopie und kein Rollback. Der Erhaltungsoracle deckt Fehler vor Publish,
Retry sowie die andere Speicheradresse des erfolgreichen Kandidaten ab; Altcode baut
im aktiven Speicher und scheitert daran. Die heute explizit erreichbaren Tangenten-Eingabefehler
deckt `ValidatePart` vor dem Kandidaten ab; die Transaktion schützt künftige spätere Fehlerpfade
ohne eine künstliche Fehlerbedingung zu behaupten.

Ein leerer `Append` setzte zuvor `Refuse` ein und löschte den veröffentlichten Subject.
Er schreibt nur seine lokale Diagnose; Daten und geliehene Views bleiben gültig, ein
gültiger Append retryt. `RejectedAppendPreservesPublishedSubject` ist die Negativkontrolle.

`Geometry`/MaterialValidation bleiben die alleinigen Werteverträge. `MaterialValueAndBindingContracts`
übt jeden Vektorfaktor, HDR-Emission, IOR-Sonderfall, positive unendliche Absorption,
ungültige Enums, Transformwerte sowie alle sieben Bindungen durch die öffentliche API.
Ground-Gesamtpublikation bleibt WI 2224. Reale OOM bleibt fatal gemäß WI 2194.

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
Keine zusätzliche Geometriekopie. Assemble-Kandidat und leerer Append sind oben getrennt
abgedeckt; Restauftrag ist späte Fehlerpropagation bis zur Engine-Publikation.

## Abnahme
- [x] Publikations-/Ersatzfehler, Quellerhaltung und gültiger Retry durch native Tests geprüft.
- [x] Fehler spät in Materialanimation erhält frühere Materialien und Geometrieansichten.
- [x] Fehlgeschlagene Probe/Clipwahl erhält Kameras, Clipdauer und Nutzbarkeit der alten Auswahl.
- [x] Variante mit fehlender Textur wird abgelehnt; folgende Probe nutzt vorherige Auswahl.
- [x] Anlegefehler erhalten Materialbestand/Namen/Indexvergabe; gültiger Retry geprüft.
- [x] Gemeinsame Index-/Containergrenzen mit synthetischen Zählern geprüft.
- [x] Alle intrinsischen Grenzen und Bindungskombinationen unabhängig geprüft, einschließlich
      HDR, IOR-Sonderfall, +infinity-Absorptionsdistanz, NaN und ungültige Enums.
- [ ] Später Import-/Generatorfehler publiziert kein Teilprodukt und erhält aktive Welt.
- [ ] Khronos-Corpus und Generatorprodukte bleiben gültig; Bildänderungen mit PNG-Orakeln
      prüfen, keine Referenzanpassung zur Kaschierung von Fehlern.
- [ ] make format, passende Tests und make lint/clang-tidy ohne Suppression.

## Nachweise
Native Materialpublikation/-ersatz, Importkonvertierung, Animation, Bilder/UVs,
Platzierung und Generatorprodukte durch Regressionen und Negativkontrollen geprüft.
Der glTF-Adapter erhält Bild- und Materialfehler aus `Geometry` bis zu seinem owned
`expected`-Text; fehlende, ungültige und Kapazitätsursachen werden nicht mehr zu einem
pauschalen Veröffentlichungsfehler zusammengezogen.
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
