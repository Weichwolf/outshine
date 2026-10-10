Type: feature
State: open
Architecture: planned
Priority: P0
Parent: 2169
Depends: 2342, 2344
Area: generators, world
Tags: infrastructure, solver, recipes

# Grade-separated recipes preserve usable local transport networks

## Ergebnis und fehlende Verträge
2342 liefert Quellbezug/Nutzungsbilanz; 2344 liefert konstruktive Profile samt gemeinsamen Ports.
Einfache Überführung, Unterführung/Tunnel und Rampe bilden zunächst zwei, danach drei lokale Ebenen.
Besitzer: generators/road; Gebäude/Terrain konsumieren den Freiraum.

## Umsetzung und Abnahme
1. Einen Zweiebenenfall mit festen Rezepten liefern: Anschluss oder Überführung ausdrücklich unterscheiden.
2. Durchfahrt/Deckdicke an echten Bauteilflächen prüfen; Layer ordnet Ebenen, bestimmt keine feste Höhe.
   Fehlende Tags plausibel ergänzen; andere Lage/längere Rampe/einfacheres Rezept sind erlaubt.
3. Danach drei Ebenen: Konflikt lokal isolieren, begrenzte Varianten probieren, ungelöste Bauteile ausweisen.
4. Wien, Basel Badischer Bahnhof, Zürich HB; Tunnel gestrichelt, Brücken blau, Freiräume sichtbar rendern.
Durchgängige verwendete Anschlüsse, funktionale Durchfahrten/Portale und plausible Steigungen.
Zeit/Varianten/Speicher begrenzen; mehr Ebenen nur nach gemessenem Wachstum und Bildgewinn.
Keine unbegrenzte Suche nach einer einzigen korrekten Lösung; Ergebnis/Nutzungsgrad vor Solver-Aufwand.
[Rezepte](../test/experiments/infrastructure_recipes.py),
[Primärquellen/Referenzprojekte](../doc/references/infrastructure/README.md).
