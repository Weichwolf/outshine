Type: bug
State: active
Area: test, harness
Parent: 2188
Depends:

# Repository claims will enforce demonstrable contracts

## Entscheidung

Nutzerauftrag 2026-09-08: Claims korrigieren, sinnlose Regeln löschen.
Markenpflicht EveryItemNamesTheBenchmark und Wortverbot TheEngineNamesNoSubject
entfallen: weder beweist ein Markenname Recherche noch ein Fachbegriff falsche Kopplung.
Historischer Aktivierungszähler AnItemReachesClosedThroughActive entfällt als
Build-Gate: vergangene Prozessverstöße sind nicht im aktuellen Code reparierbar.
Aktivierung vor Implementierung bleibt Arbeitsregel, Git bleibt historischer Nachweis.

OnePlaceDeclaresWhatAShaderBinds zählt heute null direkte SDL-Shaderkonstruktoren,
weil ShaderFile.cpp ShaderCross-Reflection nutzt. Gegen tatsächlichen Consumer und
falsche Binding-/Workgroup-Deklarationen prüfen, keine Fundstellenzahl erzwingen.
TheLayeringIsDeclaredOnce durch Prüfung des tatsächlichen Tiergraphen ersetzen;
Buildwerkzeuge dürfen eigene Includes besitzen. Verbotene Kanten/Zyklen nachweisen.
Include-Guards und eindeutige Suite-Zuordnung behalten, Parser und Funde prüfen.
Build-Closure-Claim reparieren, nicht seine fehlgeschlagenen Builds ausblenden.
Unerreichte Symbole sind ausdrücklich heuristische Kandidaten: sichtbarer Bericht,
kein hartes Nullziel. Toolfehler bleiben Fehler. Format/Tidy/API/Schema behalten.

## Abnahme

- [ ] Gültige Fachbegriffe und WIs ohne Markennamen werden nicht abgewiesen.
- [ ] Shadervertrag mit absichtlich falschen Ressourcen-/Workgroup-Werten rot.
- [ ] Layergraph mit verbotener Kante und Zyklus rot; gültiges Fixture grün.
- [ ] Suite-/Guard-/Build-Claims prüfen ihre tatsächlichen Verträge.
- [ ] Verdachtsbericht vollständig erhalten, Analysefehler verweigern.
- [ ] make lint einschließlich clang-tidy; übrige echte Fehler nicht kaschieren.

Keine vollständige Engine-Abnahme: Bildqualität und API-Lifecycle bleiben in ihren WIs.
