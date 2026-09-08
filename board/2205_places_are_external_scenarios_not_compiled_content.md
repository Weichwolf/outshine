Type: defect
State: active
Parent: 2188
Area: client, assets, test
Tags: architecture, data-driven
Depends:

# Places are external scenarios, not compiled content

## Problem und Entscheidung

PlaceCamera.cpp kompiliert neun Ortsnamen, Kameraposen und Aufnahmezeiten in kPlaces;
ScenarioFor baut daraus Szenarien. Nutzerkorrektur: Inhalt gehört ausschließlich in Daten.
Vorhanden: öffentliche Engine::readScenario/declaration und derselbe run-Szenariopfad.
Diese Fähigkeit nutzen, keinen zweiten JSON-Kameravertrag oder eingebauten Fallback erfinden.

- Je Place eine normale .scenario-Datei unter src/assets/places; Name aus Dateistamm.
  Welt, Renderparameter, Zeit, Licht und Kamera stehen dort. Bestehende Werte unverändert
  migrieren; ihre Kalibrierung/Höhenunsicherheit bleibt ausdrücklich WI 2170.
- Katalog aus einem übersteuerbaren Verzeichnis laden, deterministisch nach Name sortieren.
  Keine feste Anzahl, kein Ortsname oder Ersatzkatalog im C++; owned Dokumente und Namen.
- Client shots/places/prepare/roundtrip nutzen denselben geladenen Katalog und öffentliche API.
  Neue Dateien ohne Rebuild auffinden. Fehlende/leere/ungültige Daten ausdrücklich ablehnen.
- Aufnahmegrößen aus Szenarien respektieren; keine versteckte Überschreibung durch Shot-Konstanten.
- Abnahme-WIs verweisen auf Datendateien. Dokumentation erklärt Katalogwahl und Datenherkunft.

## Nachweis

- [ ] Make baut den Client; vorhandene neun Kameras behalten ihre deklarierten Werte.
- [ ] Ein zur Laufzeit erstellter fremder Place wird ohne Neukompilation geladen;
      geänderte Pose/Rendergröße kommt aus der Datei. Entfernen macht ihn unbekannt.
- [ ] Fehlendes/leeres Verzeichnis, malformed Szenario und ungültige Kamera scheitern
      mit Diagnose. Unabhängiger Test prüft Werte, nicht nur Writer-Roundtrip.
- [ ] Keine eingebauten Ortsdaten; negativer Test mit fehlendem Katalog darf nicht
      auf den alten Inhalt zurückfallen. Regressionstest bleibt im regulären Make-Gate.
- [ ] make lint einschließlich clang-tidy; betroffene Make-Suite; Logs im System-Temp.
