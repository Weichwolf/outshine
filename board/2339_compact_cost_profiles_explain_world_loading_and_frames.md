Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: diagnostics, generators, world, render, client
Tags: performance, budgets

# Compact cost profiles expose the work behind loading and frames

## Ergebnis und Ist
Ein Place erklärt seine Lade- und Framekosten mit wenigen Zeilen, ohne laufende Textausgabe.
Bestehende Zähler liefern Quellbytes, Cachetreffer und Render-Hostphasen; unterbrochene
Terrainjobs fehlen in Arbeitszeiten, Generatorphasen zeigen nur Maxima, Renderstufen nur
letzte Werte. Dynamische Metriknamen und lineare Suche erzeugen selbst Diagnosearbeit.

## Besitzer und Umsetzung
- `world/data/ContentStore`: Hits/Misses, gelesene/geschriebene Bytes und IO-Dauer;
  `SourceSet` besitzt Providerstarts, Retries und Lieferungen je Quelle.
- `world/ground/TilePool`: jede ausgeführte Field-/Mesh-Arbeitsportion zählen,
  einschließlich abhängiger Wiederholungen; fertiggestellte Produkte getrennt zählen.
- Generatoren und `engine/FrameMeasurements`: vorhandene Phasen zu Anzahl/Summe/Maximum
  ergänzen; Eltern enthalten ihre Kinder. Überlappende IO-/Workerzeiten nicht addieren.
- `render/SceneRenderer`: Hostphasen und CPU-Encoding je Stufe mit festen Aggregaten;
  nur erfolgreich eingereichte Frames veröffentlichen. GPU-Debuggruppen tragen Stufennamen.
  SDL_GPU bietet keine Zeitstempel: echte GPU-Passzeiten brauchen einen Backendprofiler;
  fehlende Zeiten ausdrücklich kennzeichnen, Fence-Warten separat ausweisen.
- `engine/DiagnosticLedger`: Namen einmal registrieren, wiederholte Lookups ohne linearen
  Scan; vorhandene Konfliktprüfung und öffentliche Kamera-/Messverträge erhalten.
- `client`: Zusammenfassung nach dem Messfenster, feste Einheiten und eindeutige Fenster.
  Bestehende explizite Detail-/STAT-Ausgaben bleiben verfügbar. Keine GPU-Readbacks zur
  Zeitmessung. Hot Path formatiert keine Profilzeilen und speichert keine unbeschränkten Events.

## Abnahme
Analytische Zählfälle und Fehlerpfade prüfen Teiljobs, Bytes und erfolgreiche Einreichungen.
Place-Bild/Inhalt und exakt deklarierte Framezahl bleiben gleich. Ausgabe trennt CPU-Encoding,
IO-Dauer, Wartezeit und fehlende GPU-Zeit. Messkosten mit festem Profil prüfen und benennen;
keine unbelegte Behauptung, Instrumentierung sei kostenlos. Allgemeine Gates in AGENTS.
