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

Generationwechsel invalidiert ClassField auch ohne Features; Featureklassifikation
ist vom inkrementellen Ingest getrennt. Deklarierte OsmField-Eingaben respektieren
den Layerfilter ohne Layer-0-Fallback; ausgeschlossene Features ändern keine Identität.
EmptyGenerationReplacesPreviousClasses belegt beide Tiers und Entfernung der alten
Klasse ohne Kamerabewegung. Gegenprobe ohne Stale-Invalidierung schlägt in beiden
Checks fehl. Generation-/Storage-/Street-Regressionen bestehen.
Wien ohne Vegetation vor/nach Invalidierung visuell geprüft: 0/921600 Pixel verändert.
Das belegt Regressionserhalt; flache Materialien und harte Kontraste bleiben Bildlücken.

## Überholte Builder-Ergebnisse

Collect prüft nach Rückgabe Generation und Eingabezähler und ingestiert Änderungen.
Überholte Ergebnisse werden nicht publiziert; der betroffene Tier wird erneut gebaut.
Test: Fine-only-Straße während eines laufenden Jobs verschoben; vorher fehlten Neubau
und aktuelle Klasse bei Complete, jetzt bestehen beide Checks. Positionsfehlerregression
bleibt grün. Wien ohne Vegetation visuell geprüft und zur Vorversion pixelgleich.
Offen bleiben allgemeine Arrival-order-Abnahme und vollständige Input-Manifeste.
