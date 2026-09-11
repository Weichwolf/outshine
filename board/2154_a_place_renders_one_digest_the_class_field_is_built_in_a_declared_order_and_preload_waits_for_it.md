Type: bug
State: active
Area: world, engine
Parent: 2169
Depends:

# Identical declared inputs will reproduce the same world and image

## Befund

Place-Digests wechseln teilweise ohne Quelländerung. Providerantworten, Cachezustand
und Seeds waren nicht vollständig eingefroren; Scheduling als alleinige Ursache
ist deshalb nicht bewiesen. Ein warmer Cache allein deckt keine Arrival-order-Fehler ab.

Vorhanden: stabile Klassifikationsreihenfolge sowie Preload-Warten auf Klassen,
angelegte Klassenversion und Generator-Vektorkacheln. Historische Gegenprobe ohne
Vektor-Wartebedingung und mit teilweise kaltem Cache war rot. Weitere wechselnde
Digests trotz dieser Reparaturen lassen das Gesamt-WI offen.

**Benchmark**: Unreal-Screenshotautomation und RAGE-Replay liefern den Maßstab.
Übernommen wird deterministische Eingabe/Wiederholung, nicht das Gleichsetzen
unterschiedlicher Live-Providerantworten mit demselben Szenario.

## Umsetzung

1. Manifest über Providerbytes, Generierungs-/Shader-Version, Kamera, Uhrzeit,
   Wetter, Seeds und relevante Szenario-Features erstellen.
2. Eingaben identisch wiedergeben; IO-Ankunftsreihenfolge gezielt variieren.
   Ergebnisse nach stabilen IDs übernehmen, nie nach Fertigstellungsreihenfolge.
3. Bei gleicher Eingabe Klassen-/Sheet-/Geometrie-/Material-Identitäten vergleichen;
   erst danach Rasterung, Coplanar-Ties und Filter untersuchen. 2179 trennt Mipfehler.
4. Preload muss alle deklarierten Produkte erfassen. Ein späterer Callback darf
   keinen vermeintlich abgenommenen Stand still verändern.

## Abnahme

- [ ] Identisches Manifest reproduziert PNG und logischen Zustand bei mehreren Läufen.
- [ ] Warm, kalt und teilweise kalt sowie variierte IO-Reihenfolge sind abgedeckt.
- [ ] Negativkontrolle entfernt eine erforderliche Bereitschaftsbedingung und geht rot.
- [ ] Geänderte Providerbytes sind explizit andere Eingaben, keine kaschierte Regression.
- [ ] Alle Places und Bewegung prüfen; erst nach terminalem Run-Ergebnis berichten.

Historische Digestlisten und diagnostische Sackgassen stehen in der Git-Historie.

## Leere OSM-Generation
ClassField::Ingest leert Arrays bei Generationwechsel, setzt Stale aber erst nach
Feature-Ingest. Leere Folgegeneration kann die alte Klassifikation behalten.
Entscheidung: Generationwechsel invalidiert die Klassifikation unabhängig von Features;
Ingest in Geometrieübernahme und Featureklassifikation trennen.
Test: deklarierte Fläche vollständig bauen, durch ausgeschlossene Layer ersetzen,
ohne Kamerabewegung beide Tiers erneut bauen und alte Klassifikation entfernen.
Zusatzbefund: OsmField deklariert unbekannte Layer als Layer 0 und vergleicht sie
ebenfalls so. Deklarierte Eingaben müssen denselben Layerfilter wie Providerdaten nutzen;
ausgeschlossene Features beim Identitätsvergleich überspringen, leere Identität stabil halten.
