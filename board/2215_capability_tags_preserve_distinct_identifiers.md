Type: defect
State: active
Parent: 2188
Area: include, world, engine
Tags: correctness, identity, api

# Capability tags preserve distinct identifiers

## Befund
TagCatalogue::under maskiert Ordinals mit 0xff und verschiebt sie um 16 Bits.
Assembly interned alle Capability-Namen fortlaufend. Name 257 aliasiert Name 1;
fehlende Fähigkeiten können dadurch scheinbar vorhanden sein. Keine Boundsprüfung.

## Entscheidung
Zweistufige Tags: obere 8 Bits Familie, untere 24 Bits Kind-ID. Null bleibt ungültig;
Familientags matchen ihre Kinder, Kind-Tags nur sich selbst. Kein unbewiesener
allgemeiner Hierarchieanspruch. Factory constexpr/noexcept/expected mit ausdrücklichen
InvalidFamily-/InvalidOrdinal-Fehlern; Assembly propagiert Fehler vor Publikation.
24 Bit erlauben 2^24-1 = 16777215 verschiedene Kind-IDs pro Familie.
Bestehende Capability-Namen sind semantische Identitäten; binäre Tagwerte sind keine
persistenten Save-IDs. Runtime-/Importer-Konventionen bleiben davon unabhängig.

## Abnahme
- [ ] IDs 1/257 und 255/256 bleiben verschieden, letzte gültige ID wird erhalten.
- [ ] Null, übergroße ID und verschachtelte Kind-Familie werden ausdrücklich abgelehnt.
- [ ] Familienmitgliedschaft und Selbstvergleich constexpr und zur Laufzeit geprüft.
- [ ] Öffentliche Assembly mit mehr als 255 Capability-Namen ohne Aliasierung.
- [ ] Bisherige Codierung scheitert am Identitätsoracle, vollständige Assembly-Regression.
- [ ] API-Verträge dokumentiert, Lint geprüft; keine Ausgabe-Bitwerte als stabile IDs versprechen.
