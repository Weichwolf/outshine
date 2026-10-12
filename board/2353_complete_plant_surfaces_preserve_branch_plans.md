Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, plants, surfaces

# Complete plant surfaces preserve branch plans

## Ergebnis und Besitzer
Die bestehenden Ast-/Organpläne liefern eine vollständige Testpflanze mit zusammenhängender Holzoberfläche.
2346 besitzt Pflanzenhierarchie/Organe; 2352 lokal organische Gabeln; 2348 SpeedTree-Funktionsumfang.
Funktionstest vor weltweiten Artenrezepten, keine botanische Abnahme oder native Performancefreigabe.

## Verfahren und Entscheidung
Ein globales Gitter passt nicht zu meterlangen Stämmen mit submillimeterdünnen Zweigenden.
Oberflächenvereinigung auf denselben bestehenden Rohrmeshes gegen sequentielle Vereinigung vergleichen.
`manifold3d==3.5.4` als externe Bibliothek in `.venv`; eigene Eingaben/Prüfungen, kein gebündelter Fremdcode.
Python steuert den nativen Bibliothekskern; Ausgabe-Doubles und getrennte Ecknormalen erhalten.
Analytische Rohrnormalen als Vertexattribute durch Schnittinterpolation übernehmen und normalisieren.
Neu berechnete Meshnormalen erzeugten lange Schattierungsstreifen am Stamm und wurden verworfen.
Vor Expansion höchstens 4.096 Achsen/eine Million Quelldreiecke; keine heimliche Inhaltsauslassung.
Schnitt-Splitter mit 10⁻⁸ m Bibliothekstoleranz vereinfachen; geometrische Prüfgrenzen bleiben erhalten.
Geschlossene Vereinigung und organische Ausrundung sind getrennte Anforderungen; keine Gleichsetzung.
Ghidras ComputeWeldRing/WeldHit (2352) verwendet Profile und Elternabfragen; kein Beleg für Mesh-Booleans.
[Interactive and Robust Mesh Booleans, SIGGRAPH Asia 2022](https://arxiv.org/abs/2205.14151)
begründet den Verfahrensvergleich; PDF-Seite 1 tatsächlich optisch geprüft, Ablage im Quellenregister.
[Manifold](https://manifoldcad.org/docs/html/) implementiert einen eigenen Algorithmus; keine Gleichsetzung.

## Geprüfter Zwischenstand
Drei Seeds, jeweils 64 Achsen/571 Blätter, gleiche 16-seitige/24-längsgeteilte Rohrmeshes.
Gemeinsame Vereinigung lokal 271–287 ms; Achsaufbau separat, drei Wiederholungen je Methode/Seed.
Je Ergebnis 54.798–54.818 Dreiecke, eine geschlossene außenorientierte Komponente, Eulerzahl zwei.
192 unabhängige Raumwinkelproben je Seed/Methode, davon 128 innerhalb aller dünnen Achsen.
Sechs analytische Würfelvereinigungen mit Überdeckung/Gleichheit/Flächenkontakt stimmen im Volumen.
Die Methoden stimmen zusätzlich im Kernel-Differenzvolumen überein; das ist kein unabhängiges Oracle.

Gesamtpflanze und Anschluss-Nahbild optisch geöffnet: unveränderte dünne Zweige und Blattgruppen.
Die korrekte Vereinigung allein verbessert die organische Bildwirkung nicht; kein Qualitätsgewinn behauptet.
Sie ersetzt innere Rohrflächen durch einen gemeinsamen Körper. Nächster Bildbedarf ist lokale Ausrundung,
nicht weitere Optimierung der unveränderten scharfen Anschlussform.

## Abnahme und offene Lücke
Alle 64 Holzachsen/571 Blätter des bestehenden Jungbaums, gleiche stabile IDs, Anker und Formen.
Geschlossene orientierte Oberfläche, sichtbare dünne Enden und keine inneren Rohrkappen;
analytische Boolesche Prüffälle, gemeinsame Eingaben und getrennte Erzeugungs-/Produktkosten.
Belaubte Gesamtansicht plus Anschluss-Nahbild optisch prüfen; keine reine Verwaltungsverbesserung.
Organische Ausrundung, Rinde/UVs, Wind und vollständige LODs bleiben offen.
