Type: defect
State: active
Parent: 2094
Area: test, build
Tags: lint, source-policy
Depends:

# Lint enforces comment policy without changing tokens

Nutzerregel: src/ ohne Kommentare, include/ ausschließlich Doxygen, test/ alle Kommentare.
make lint ruft bereits strip über db/crown-provenance auf, aber src/client ist ausgenommen.
Der Scanner entfernt außerdem bei inline-Blockkommentaren notwendige Tokentrenner und
bearbeitet Leerraum innerhalb von Raw-Strings durch globale Nachbereinigung.

- Client-Ausnahme entfernen; Pfadpolitik an Projektwurzeln binden. test/ unverändert lassen.
- Kommentare lexikalisch ersetzen, Trennung von Tokens und Präprozessorzeilen erhalten.
  String-/Zeichen-/Raw-Literale, Zahlentrenner und fortgesetzte Kommentare prüfen.
- Keine globale Textbereinigung innerhalb von Literalen. Fehlgeschlagenes Formatieren melden.
- Regressionstests vor jeder automatischen Quellmutation im Make-strip/Lint-Pfad ausführen.
- AGENTS.md und Make-Hilfe an tatsächliche Regel anpassen.

## Abnahme

- [ ] Scanner-Tests beweisen alle Pfadklassen, Literale, Tokenabstände und Idempotenz;
      ursprünglicher Scanner scheitert an mindestens einem unabhängigen Gegenbeispiel.
- [ ] make lint entfernt auch Doxygen aus src/client und erhält Doxygen in include.
- [ ] Keine veränderten Stringinhalte; Make-Build nach der Entfernung erfolgreich.
