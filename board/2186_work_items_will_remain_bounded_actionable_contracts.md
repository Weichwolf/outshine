Type: debt
State: active
Area: board, harness
Parent: 2169
Depends:

# Work items will remain bounded actionable contracts

WI-Inhalt: Ziel, aktueller relevanter Befund, Umsetzung, Abnahme, Parent und
blockierende Abhängigkeiten. Historische Abläufe stehen in Git, nicht als Anhang.
`Parent` ist Zugehörigkeit; `Depends` bedeutet blockiert durch. Keine Eltern-Kind-
Abhängigkeit allein wegen Zugehörigkeit, keine Verweise auf gelöschte WIs.

**Benchmark**: Neither Unreal nor RAGE decides our issue size. The choice is mine:
120 Zeilen und 12 KiB pro WI [SET]. Das lässt Ziel, Metadaten, Lösung und Abnahme
auf wenigen Bildschirmseiten zu; die Bytegrenze verhindert Umgehung durch Langzeilen.
12 KiB = 12 × 1024 Bytes. Keine Ausnahmen für bestehende WIs.

- [ ] Claim prüft jedes WI gegen beide Grenzen, inklusive negativer Grenzfälle.
- [ ] `make lint` übernimmt einen fehlgeschlagenen Claim in seinen Exitstatus.
- [ ] Alle übergroßen WIs verdichten oder fachlich aufteilen, offene Anforderungen erhalten.
- [ ] Parent/Depends pflegen, aktuelle Reihenfolge aus 2169 erhalten.
- [ ] Nach jedem Änderungsschritt `make lint` einschließlich clang-tidy ausführen.

Ausgangsbefund: acht WIs über 120 Zeilen; 2111 mit 1160 Zeilen ist der größte.
