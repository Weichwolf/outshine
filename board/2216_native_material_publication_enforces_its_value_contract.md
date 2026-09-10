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
Native Materialpublikation/-ersatz, Importkonvertierung, Animation, Bilder/UVs,
Platzierung und Generatorprodukte durch Regressionen und Negativkontrollen geprüft.
Einzelne Nachweise stehen in Git; vollständige Corpus-/Weltabnahme bleibt offen.
## GroundMaterials-Katalog
Load liest über ReadTextFile mit 1-MiB-Budget und publiziert erst den vollständigen
Kandidaten. Fehler erhalten den alten Katalog. Sortierter Namensindex löst eindeutige
Namen, Reibungs- und Litter-Referenzen ohne Umordnung; Vorwärts-/Selbstreferenzen gültig.
Reibungswerte und Quotienten vor Float-Verengung auf positiven darstellbaren Bereich
prüfen. kWet/Feuchte [0,1], optionale Modellobjekte und streng aufsteigende Float-edges.
Smoothstep sättigt außerhalb des Intervalls vor Division. Rauheit, Bedeckung und
Albedokanäle [0,1]; sichtbarer/breitbandiger Quotient darf >1 sein, Ergebnis-Albedo nicht.
Fehlende Werte behalten Defaults, vorhandene falsche Typen werden abgelehnt.
Altcode verletzt jeweilige Negativkontrollen; Fehlererhaltung, analytische optische
Randwerte, Retry und ausgelieferter Katalog bestehen. Gültige Arithmetik unverändert.

## Geometrische Katalogwerte
Korngröße und Höhenamplitude: endliche nichtnegative Meter, Float-Bereich vor Cast;
null bleibt für Wasser gültig. Detailmaßstäbe: positive darstellbare Meter und echtes
Paar. Optional fehlendes Paar behält Defaults; vorhandene falsche Typen ablehnen.
Hangintervall: Zahlenpaar mit 0 <= min <= max <= 90 Grad, vor Float-Cast geprüft.
Nur Maximum wird aktuell weitergegeben; GroundSurf/LitterSurf werden vorbereitet,
aber Detailmaßstäbe aktuell nicht vom Ground-Shader ausgewertet. Kein Rendernachweis
für prozedurales Mikrorelief behaupten. Altcode verletzt die Negativkontrolle;
Fehlererhaltung, gültige Grenzen und ausgelieferter Katalog bestehen. Vollständige
Generator-/Renderintegration bleibt separat offen.

## Atomarer Animationsaufbau
Pose::Build darf bestehende Posen bei ungültiger Auswahl oder späten Kanalfehlern
nicht löschen. Kandidat mit Phasen für Ruhepose, Ziel-/Samplerprüfung und Kurvenaufbau;
Track-Spans behalten stabile Channel-Besitzer. Direkt in besitzende Puffer dekodieren.
Konkurrierende Ziele über geordneten Index statt quadratischem Scan erkennen.
Nachweis: gültige Fixture, isolierte Fehlerproben, Konflikte/Kombination und Retry;
Altcode verletzt Erhaltung, Korrektur und vier Importerregressionen bestehen.
Zeitgitter-/Wertevalidierung in Track/Keyframes bleibt separat offen.

## Geprüfte Kurven
Formatunabhängige Keyframes nach base/math verschieben; geprüfte span-Factory statt
öffentlichem Rohzeigerkonstruktor. Endliche, streng steigende Zeiten, endliche Werte,
gültige Interpolation und dimensionssichere Rechnung vor Veröffentlichung erzwingen.
Track lehnt unbekannte Pfade/negative glTF-Zeiten ab und erhält gültige Vorgänger.
Zu kleine Sample-Puffer und nichtendliche Abfragen ohne Schreibzugriff ablehnen.
Analytische STEP/LINEAR/CUBICSPLINE-Kontrollen, Fehlererhaltung und Importerregressionen.
