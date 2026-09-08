Type: defect
State: open
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
