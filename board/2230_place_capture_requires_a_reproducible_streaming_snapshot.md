Type: defect
State: open
Architecture: question
Parent: 2218
Depends:
Area: client, engine, test
Tags: determinism, streaming, capture

# Place captures need a reproducible streaming snapshot

## Beleg

Zwei aufeinanderfolgende Läufe desselben Codes nach 947d891d9:
`make shots PLACE='--no-vegetation --preload-seconds 120 Graz'`.
Ausgaben `Graz-e1e6a2b6.png` und `Graz-3869196b.png`: 68/921600 Pixel verschieden,
49 um mehr als 1/255; maximale Kanalabweichung 32/255. Nur linker Hang,
x=14..378, y=351..434. Zweiter Lauf ist pixelgleich zum älteren Ausgangsbild.
Beide PNGs geöffnet. Reproduzierbarkeit fehlt; die Ursache ist noch nicht isoliert.
Referenzen: `build/shots/reference/empty-tile-publication/Graz-before.png` und
`Graz-after-first.png`. Keine Toleranzerhöhung und kein Neupinnen zum Kaschieren.

## Architekturfragen

- Frage: Unterscheiden sich publizierter Streamingstand, deterministische Produktdaten
  oder ausschließlich GPU-Auswertung beim Capture? Der Befund beweist noch keinen
  bestimmten dieser Mechanismen und keine Regression durch den Tile-Fix.
- Vorschlag: Capture an expliziten veröffentlichten Snapshot binden; begrenzte
  Bereitschaftsbedingung auf benötigte Revisionen statt abgelaufene Wartezeit stützen.
  Zuerst bestehende Shot-/Readiness-Verträge und Produktdigests verfolgen; Quell-/Ground-
  Revision, Kamera, Zeit, Wetter und Samples der betroffenen Hangdaten gezielt vergleichen.
- Alternative: längere feste Wartezeit. Kostet Durchlaufzeit und garantiert weder
  gleichen Datenstand noch gleiche Publikationsreihenfolge; deshalb nicht als Lösung.
- Blockiert: strikte deterministische Place-Bildabnahme, nicht analytische/GPU-
  Vertragsprüfungen oder die unabhängige Implementierung von WI 2224/2216.

## Untersuchung und Abnahme

1. Produktionspfad von Make shots zum Client-Capture verfolgen, keine zweite Render-
   Implementierung bauen. Logs nur für die eingegrenzten Revisionen/Produkte ergänzen.
2. Gleiche Eingaben offline aus bestehendem Cache, gleicher Kamerastand und Zeitpunkt;
   kleine Höhen-/OSM-Fixture mit kontrollierbar vertauschter Worker-Fertigstellung.
3. Bei abweichenden CPU-Produkten zuerst Reihenfolge/Revision isolieren; bei gleichen
   Produkten GPU-Eingaben und Readback prüfen. Ursache und Vertragsentscheidung im WI
   ersetzen, danach `Architecture: ready` setzen und erst dann strukturell umbauen.
4. Gleicher Snapshot liefert gleiche deklarierte Bildmetrik bei Wiederholung und
   vertauschter Worker-Reihenfolge. Nichtfertige/fehlgeschlagene Quelle führt bounded
   zu einem expliziten Fehler, niemals zu erfolgreichem halbfertigem Capture.
5. Negativkontrolle gegen belegten alten Fehler; Graz-PNGs und gezielte Tests, Lint.
   Backendübergreifende Bitgleichheit wird damit nicht behauptet.
