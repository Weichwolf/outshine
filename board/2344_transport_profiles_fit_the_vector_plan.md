Type: feature
State: open
Architecture: planned
Priority: P0
Parent: 2169
Depends: 2343
Area: generators, world
Tags: infrastructure, solver, recipes

# Transport profiles fit coarse DEM on actual vector geometry

## Ergebnis und fehlender Vertrag
2343 liefert den ebenen Bauteilplan mit gemeinsamen Ports/Querschnitten.
Grobes DEM liefert einen weichen Höhenbezug; konstruktive Profile dürfen davon abweichen.
Besitzer: generators/road; finales Terrain konsumiert die geplanten Kontakte.

## Umsetzung und Abnahme
1. Eine Ebene: gemeinsame Anschlusshöhen, plausible Längs-/Querneigung und Kurven zunächst lokal lösen.
2. Zusätzliche Stützpunkte nur nach Krümmung/Profilfehler; keine starre Raumrasterauflösung.
3. Kurze/unplausible Abschnitte mit einfacherem Rezept oder geänderter Lage ersetzen.
   Kleine begrenzte Korrekturen; danach ungelöste lokale Teile protokollieren und gültiges Netz liefern.
4. Dasselbe Netz auf grobem DEM, danach finales Terrain darunter; ohne Terrain separat rendern.
Klassenabhängige praxistaugliche Profile, stetige Anschlüsse, keine Stufen/verdrehten Fahrbahnen.
Ausgegebene ganze Querschnitte prüfen; richtige Mittellinien allein reichen nicht.
Keine Fixierung auf Originalhöhen oder globale optimale Rekonstruktion. Nutzungsgrad/Bild/Kosten entscheiden.
[Profile/Envelopes](../test/experiments/road_profile_envelopes.py),
[Procedural Roads, Eurographics 2010](../doc/references/infrastructure/eurographics/2010-procedural-generation-of-roads.pdf).
