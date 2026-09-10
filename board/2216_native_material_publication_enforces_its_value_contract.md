Type: defect
State: active
Parent: 2150
Area: scene, base, import, render
Tags: validation, materials, ownership
Depends:

# Native material publication enforces its value contract

## Beleg
include/scene/Material.h dokumentiert Wertebereiche, Farbräume und Lebensdauer.
Geometry::addSurface kopiert ungeprüft und verengt die Slotzahl auf int;
setSurface prüft jetzt Zielindex, Werte und Bildbindungen mit typed expected vor Kopie.
wellFormed prüft vollständige Material-/Meshprodukte einschließlich Bindungen. SubjectDraw::ValidateMaterials prüft Geräte-/Pass-
Fähigkeiten, keine physikalischen Faktoren. Diese Prüfungen sind nicht austauschbar.

## Lösung
Gemeinsame nichtallokierende native Validierung vor Materialveröffentlichung;
Importer, Generatoren und direkte API-Aufrufer durch denselben Besitzer führen.
Zwei Ebenen: eigentliche Materialwerte und referenzielle Gültigkeit im Assetbesitzer.
- Faktoren gemäß Material.h: finite Werte, zulässige Intervalle, gültiges AlphaMode;
  nichtnegative Emission/Schichtdicken, geordnete Irideszenzdicken, IOR-Konventionen.
  Positive unendliche AttenuationDistance ausdrücklich zulässig, NaN niemals.
- Texturbindungen: gültige Sampler-/UV-Enums und finite UV-Transformationen;
  gebundene Bilder im Besitzer vorhanden. Negative Bildindizes bleiben ungebunden.
- Aufbau-Reihenfolge der Importer prüfen: Referenzen bei vollständiger Asset-Publikation
  validieren, ohne legitime vorbereitete Vorwärtsreferenzen zu verlieren.
- Fehler typisiert/expected, kein stilles Clamp oder Ersatzmaterial. Ungültiger Ersatz
  erhält vorherige Werte; fehlgeschlagenes Anlegen verbraucht keinen Slot/keinen Namen.
  Indexkapazität vor Allokation/Verengung prüfen. API-Dokumentation/Consumer migrieren.
- Renderer-Fähigkeitsprüfung bleibt getrennt: gültiges Material ist nicht automatisch
  auf jedem Backend bzw. in jedem Renderplan unterstützt.

## Abnahme
- [ ] Default, Intervallgrenzen, HDR-Emission und positive unendliche Absorptionsdistanz gültig.
- [ ] NaN/Inf, ungültige Enums, Dickenreihenfolge und verwaiste Bildbindung abgelehnt;
      Negativkontrollen verletzen unabhängige Oracle ohne Buildfehler.
- [ ] add/replace-Fehler erhalten Materialbestand und Indizes; erfolgreicher Retry geprüft.
- [ ] Äquivalente Import-/Generator-/direkte API-Fälle haben gleiche Ergebnisse.
- [ ] Unterstützte Khronos-Materialien und native Generatorprodukte bleiben gültig;
      Bildänderungen mit PNG-Orakeln prüfen, keine Referenzanpassung zur Kaschierung.
- [ ] make format, passende Tests, make lint einschließlich clang-tidy; keine Suppression.

Kein neues paralleles Materialmodell. Vorhandenen nativen Besitzer und dokumentierte
Verträge vervollständigen. Vollständige Asset-/Instanzmigration bleibt WI 2150.

Aufbaureihenfolge geprüft: Subject::Flatten legt Materialien vor Bildern an; Handed
kopiert Bilder vor Materialien. Referenzprüfung am vollständigen Geometry-Produkt in
wellFormed, vor Engine::setGeometry-Übernahme. Intrinsische Materialwerte, alle Bindungen
und Materialindizes dort gemeinsam prüfen; Setter-Fehlerverträge bleiben separat offen.

Publikationsprüfung umgesetzt: wellFormed validiert Materialwerte, alle sieben
Texturbindungen und zugewiesene Materialindizes; Engine::setGeometry lehnt vorher ab.
81 Material-/API-Checks grün, Altcode scheitert ohne Buildfehler. Native Geometrie,
animierte Importmaterialien, Baumgenerator und Asset-Roundtrip als Regressionen grün.
Abschluss-Lint 180 tidy/282 Doxygen, 32 Repository-Tests grün, drei rote Gruppen.
Setter-expected-/Rollback-Verträge und vollständige Corpus-/Bildabnahme bleiben offen.

setSurface wird validierender Ersatz: expected<void, MaterialUpdateError>, noexcept,
getrennte Fehler für fehlenden Slot und ungültige Werte/Bindungen. Alle Prüfungen vor
Kopie, vorheriges Material bei Fehler erhalten. Importer aktualisieren nach Bildaufbau.
Publikations-/Export-Negativfixtures erzeugen ungültige Aufbauzustände weiter über
addSurface; ihre Oracles bleiben bestehen. Neuer Erhaltungstest muss am Altsetter scheitern.

Ersatz-Abnahme: fehlender Slot und ungültige Werte/Bindungen liefern getrennte Fehler,
keine Allokation/Mutation vor erfolgreicher Prüfung. Erhaltung/Retry, Publikation, Export,
animierte Importmaterialien und native Bilder/UVs grün. Altsetter verletzt Erhaltungsoracle
ohne Buildfehler. Export-/Publikations-Negativfälle bleiben über Aufbaupfad erhalten.
Abschluss-Lint 180 tidy/282 Doxygen, 32 Repository-Tests grün; drei rote Gruppen.

## Migration des Anlegevertrags
Aufruferinventar: Subject::Flatten/Handed ignorieren noch addSurface. Handed reicht
Bild-/Vertex-/Indexsetter-Fehler jetzt per expected weiter und prüft Materialwerte. Engine::State::Models ist void und Laying nutzt
Materialindizes unmittelbar. Structures/Corridors sowie TreeGeometry/CrownAtlas legen
Materialien für Generatorprodukte an. 15 C++-Testdateien verwenden addSurface.
Anlegen als expected<MaterialInstance, MaterialError>: intrinsische Werte/Enums/UVs
und Indexkapazität vor Kopie prüfen; Bildreferenzen erst am vollständigen Produkt.
MaterialUpdateError zu gemeinsamem Fehlervertrag erweitern, keine zweite Fehlerhierarchie.
Subject::Handed braucht einen durchgehenden fehlbaren Konvertierungspfad für Bilder,
Materialien und Meshdaten; Generator-/Geländehelfer müssen Fehler bis zum Aufrufer
weiterreichen. Keine value_or-Defaultoberfläche, unchecked Dereferenzierung oder
erfolgsmeldende leere Geometrie als Ersatz. Aufrufer in einem vollständigen Schritt
migrieren; ungültige Material-Fixtures dann an der frühesten garantierten Grenze prüfen.
Fehler beim späteren Element mit bereits aufgebauten Vorgängern testen: kein teilweise
konvertiertes Produkt als Erfolg und keine Veröffentlichung in die aktive Welt.

Handed-Konvertierung abgenommen: expected<Geometry,string>, getrennte Asset-/Mesh-
Phasen; kein teilkonvertierter Rückgabewert bei geprüftem Bild-/Material-/Setterfehler.
Importer ersetzt Handed erst nach erfolgreicher Konvertierung. Später Materialfehler,
Quellerhaltung/Retry sowie Bild-/Platzierungs-/Animationsregressionen grün. Mutation
ohne Materialprüfung scheitert ohne Buildfehler. Lint 180/282, 32 Repository-Tests
grün; drei rote Gruppen. Gesamter Import-/Animations-Rollback bleibt separat offen.

Materialanimation publiziert Handed bisher vor Wears/SampleMaterials; ein später
ungültiger Faktor kann bereits geänderte frühere Materialien sichtbar lassen. Beide
Phasen auf dem vorhandenen Konvertierungskandidaten ausführen, erst bei Erfolg nach
Handed verschieben; keine zusätzliche Geometriekopie. Test mit zwei Materialien und
später ungültiger Animationsprobe erhält vorherige Faktoren und Geometrieansichten.
Kamera-/Clipzustand und vollständiger Import-Rollback bleiben gesondert zu prüfen.
