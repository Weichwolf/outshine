Type: defect
State: active
Parent: 2131
Area: engine, world, scenario
Tags: architecture, audit, data-driven
Depends:

# Declared providers control the actual world sources

## Beleg und Auswirkung

Asking.cpp State::Composes übergibt an GroundStack::Open stets Data::ShippedProviders()
statt Session.Declared.Providers. DeclaredSources.cpp kompiliert diese Liste als Array;
RegisterDeclared wählt nur nach Kind und ignoriert Provider::Pin/Rank/WhenAbsent.
TerrariumDem/VersatilesVector kompilieren konkrete URLs, Source-ID, Zoom-/Cache-/Retry-
Policy. EngineHeld::Unacted führt deklarierte providers als nicht umgesetzt auf.
Eine scheinbar gültige Szenariokonfiguration bestimmt somit nicht die benutzte Datenquelle.
Offline wird in Composes schon vor dem Cache-/Providerzugriff pauschal abgelehnt.

## Entscheidung

Benchmark: deklarative Provider-Registry, explizite Source-Identität und Offline-Resolver.
Vorhandene SourceSet/RegisterDeclared/Transport/ContentStore verwenden. Decoder/Protokoll-
Fähigkeiten bleiben Code; konkrete Endpunkte, Datenstände, Rang und Fehlstrategie werden
Daten. Defaults als ausgelieferte Konfiguration, kein versteckter kompilierter Ersatz.
Unbekannte/nicht unterstützte Konfiguration vor IO ablehnen. Offline darf vorhandene
Cache-/Dateiprodukte lesen; Cache-Miss meldet fehlende Daten ohne Netzwerkversuch.
Source-Version/Pin muss Cache-Identität und Replay tatsächlich bestimmen.

## Abnahme

- [ ] Zwei deklarierte Testprovider liefern unterscheidbare Daten; Auswahl/Rang ändern das Ergebnis.
- [ ] Pin-/Policy-/URL-Wechsel ohne Rebuild, kein unbeabsichtigter Standard-HTTP-Aufruf.
- [ ] Offline mit vollständigem Cache erfolgreich; Miss explizit, null Netzwerkanfragen.
- [ ] Negativkontrolle ShippedProviders statt Deklaration wird durch Request-Oracle erkannt.
- [ ] Datenherkunft im Rendermanifest; gleicher Snapshot reproduzierbar, Make-Lint grün.

## Deklarationserhaltung
Writer erhält Providers und Compositors mit Reihenfolge und allen Feldern.
XML-Escaping, int-Ranggrenzen, Pixelbudgets und bool sind unabhängig geprüft.
Compositor-On als kanonisches true/false schreiben, unabhängig vom offenen yes/no-Fix.
Öffentliche Typen dokumentieren Besitz und derzeit fehlende Runtime-Wirkung.
Diese Erhaltung ersetzt weder Provider-Registry noch Compositor-Implementierung.
Abnahme: Reader/Writer-Fixture mit mehreren Einträgen, Escaping, Rangextrema, bool
und endlichen Pixelbudgets; Altwriter scheitert am Inhaltsvergleich. Fünf Regressionen grün.
Gemeinsame Compositor-Wertevalidierung siehe unten; Registry-/Runtime-Wirkung bleibt offen.

## Rang-Import
Provider::Rank wird direkt als vollständiger dezimaler int-Token geparst; optionales
Plus bleibt erlaubt. Überlauf, Suffix, Leerraum und Brüche werden abgelehnt, fehlend
bleibt 0. Negativkontrolle bestätigt; vier Regressionen prüfen Dokumenterhaltung,
Int-Extrema, gültigen Retry, Layer und Export.

## Gemeinsamer Compositor-Wertevertrag
BudgetPx endlich und nichtnegativ; Kategorie nicht leer. Einen allokationsfreien
Validator in Reader, Engine::declare und Writer verwenden. XML-Zahlentoken vollständig
parsen. Ungültige Deklaration erhält vorherigen Zustand; Export liefert Fehler statt
kaputtem XML. Gültige Null/Bruchwerte bleiben erhalten, unabhängig von On.
Public-API-Negativkontrolle bestätigt. Runtime-Implementierung bleibt offen;
Wertevalidierung behauptet keine Ausführung. Import-/Export-Negativfälle und Retry geprüft.
