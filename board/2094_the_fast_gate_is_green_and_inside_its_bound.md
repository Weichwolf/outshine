Type: bug
State: active
Area: test, gate
Tags: measured, gate
Depends: 2093, 2131, 2152

# The gates distinguish a clean result from a missing check

## Beleg und aktueller Stand

Quellaudit 2026-09-08: test/lint.sh ignoriert den Exitstatus von run-clang-tidy mit
`|| true` und zählt danach warning-Zeilen. Bei null Befunden bricht es ausdrücklich
ab („the analysis found NOTHING ... it did not run“). Ein sauberer Lauf kann so nie
grün werden; ein teilweise fehlgeschlagener Lauf mit verbleibenden Warnungen wird
nicht zuverlässig als unvollständig erkannt. Die fest genannte Unit-Anzahl ist kein
Ausführungsnachweis. EveryItem-/Build-Claims sind aktuell 66/66 grün; ältere rote
Claim-Tabellen hier sind überholt. make test als Ganzes wurde nicht neu abgenommen.
Aktuelle Lint-Gruppen rot: Format, Tidy, öffentliche Dokumentation, Writer-Coverage.

## Entscheidung

Benchmark: nachgewiesene Prüfabdeckung und explizite Tool-Ergebnisse statt Zählerheuristik.
Compile-Datenbank gegen erwartete analysierte Units abgleichen; Status, Toolfehler und
Diagnosen getrennt erfassen. Ein erfolgreich vollständig analysierter Stand mit null
Diagnosen ist grün. Abgebrochene/fehlende Analyse ist rot, unabhängig von Warnungszahl.
Keine künstliche Warnung als Lebenszeichen und kein Unterdrücken echter Befunde.
Shader-Coverage nach 2152 auf GLSL-Artefakte umstellen. Scanner-Tests vor Quellmutation,
Client-Katalogtests und bestehende Fachorakel im Make-Gate erhalten.

## Abnahme

- [ ] Vollständiger warnungsfreier Fixture-Lauf grün; echte Tidy-Diagnose rot.
- [ ] Fehlendes Tool, leere/falsche Compile-Datenbank, Parsefehler und abgebrochene
      Unit rot; auch dann, wenn eine andere Unit reguläre Warnungen liefert.
- [ ] Jeder Gate-Teil berichtet Abdeckung, Ergebnis und Dauer; kein grüner Leercheck.
- [ ] make test und make lint vollständig grün; Zeitgrenzen aus gemessenem Umfang
      begründen. Langsame Gate-Teile reparieren, nicht aus der Pflicht entfernen.
- [ ] Negativkontrollen schlagen wegen des jeweiligen Vertrages fehl, nicht wegen
      eines fehlenden Harness oder einer unverwandten Kompilierpanne.

## Aktiver Schritt: vollständige clang-tidy-Ausführung

Compile-Datenbank enthält aktuell alle src/*.cpp außer src/client/Main.cpp. BuildTools
muss auch den tatsächlich gebauten Einstieg mit seiner echten Compile-Konfiguration
registrieren. Runner gleicht Dateimenge, Compile-Einträge und abgeschlossene Toolaufrufe
ab, bewahrt Status/Diagnosen je Unit und akzeptiert null Befunde nur bei vollständigem
Erfolg. Tests mit realem clang-tidy prüfen saubere Quelle, Diagnose, Parsefehler und
fehlende Unit; injizierter Toolabbruch darf nie als sauberer Nullbefund gelten.
