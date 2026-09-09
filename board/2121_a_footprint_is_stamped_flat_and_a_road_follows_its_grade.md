Type: debt
State: active
Area: world, render
Tags: webcam, measured
Parent: 2169
Depends:

# Ground contacts are solved locally without collapsing separate structures

## Stand / Zuständigkeit

Stamping und Korrekturen an der East/North-Abbildung existieren; alte „kein Stamp“-Befunde
sind überholt. `Laying.cpp`, HeightSheets/Press und `generators/road/Corridors.*` bleiben
die Träger. Die damalige Median/MAD-Empfehlung ist widerlegt und in 12ceb790 entfernt.
Aktuelle Ufer-/Nahwandbilder zeigen weiter unplausible Erdformen. Die reine Deckung eines
Rasterknotens beweist keine korrekte Fläche zwischen Knoten oder einen gültigen Anschluss.

Dieses WI besitzt bodengebundene Pads/Korridore/Junction-Kontakte. 2175 besitzt das räumliche
Alignment und Brücken/Tunnel; 2133 die logische Konnektivität. Keine gegenseitige Abhängigkeit.

- Lokale Randbedingungen für Gelände, Fundamente, Fahrbahn und Wasser priorisiert lösen;
  Profile/Flächen als Eingang akzeptieren. Nur tatsächlich verbundene ebenerdige Junctions
  gemeinsam nivellieren. Getrennte Ebenen behalten ihre Träger und stempeln nicht übereinander.
- Pads/Fundamente, Subgrade/Fahrbelag, natürliche Böschung und gebaute Stützwand unterscheiden.
  Kein universelles Alles-plan-/Alles-level-querschnitts-Gebot: Entwässerungsquerneigung,
  Gleisüberhöhung, Treppen und geneigte Plätze als deklarierte Konstruktion erhalten.
- Residuen/aktive Grenzen und Ursache pro Feature ausgeben. Keine feste DEM-Fehlerstatistik
  als garantierte 4-m-Grenze sämtlicher Quellen verwenden. Fehlende Rastertreffer durch
  kantenkonforme Geometrie/Verfeinerung behandeln; Fundament kaschiert keinen falschen Weg.
- Finale Oberflächen an 2166/2144 liefern; Wasser in 2145 benutzt denselben Kontaktvertrag.

- [ ] Bestehende Floor-/Corridor-Orakel erhalten, zusätzlich schmale zwischen Rasterknoten
      liegende Fläche und gestapelten Brückenfall prüfen. Absichtlich falscher Stamp geht rot.
- [ ] Junction-Restfehler, Kerb-/Subgrade-Anschluss und maximale Erdänderung ableiten/messen;
      kein globaler Grade-Clamp, der eine Straße ab ihrer ersten Steigung schweben lässt.
- [ ] Husum/Feldkirch/Wien und ältere OldTown/Heidelberg-Fälle visuell mit Querschnitt abnehmen.

Wahl: lokale konstruktive Randbedingungen statt wiederholter globaler Relaxation.
Unreal/RAGE sind Bildbenchmarks; outshine muss den sonst authorierten Geländekontakt generieren.

## Begrenzte Ribbon-Erzeugung

Sweep konvertiert bisher Stationsquotienten vor der Größenprüfung nach size_t.
Endliche Section-/Intervallwerte und Stationsbudget vor Cast/Allokation prüfen;
Validierung, Längsverbindungen und Endkappen fachlich getrennt halten. Gleiche
Vertex-/Indexreihenfolge für gültige Eingaben. Unabhängige Gerade mit Restintervall,
NaN/Inf, Quotientenüberlauf und ungültige Breiten prüfen; Negativkontrolle muss scheitern.

Carriageway-Normalen aus Ableitungen derselben parametrischen Fläche bestimmen:
S(s,t) = (C_E+t*L_E, H-t*tan(bank), C_N+t*L_N). Die Längsableitung enthält
(1-curvature*t) und -t*sec²(bank)*bankRate. Analytische Geraden/Kreisbögen mit
Steigung und Bankwechsel unabhängig differenzieren; Deck als natives GLB ausgeben
und über outshine-client vor/nach Korrektur visuell prüfen. Kein Driving-Gesamtnachweis.

Bauwerke führen die lokale Terrain-Anpassung: erklärte Fundament-/Bodenplattenhöhe,
Auflager, Zufahrtsgradienten und Freiräume bestimmen Abtrag, Aufschüttung, Planierung,
Böschungen und Stützwände. Terrain nicht bloß mitteln und das Gebäude darauf verschieben.
Konflikte baulicher Bedingungen explizit lösen oder diagnostizieren; keine stillen
Überlagerungsregeln. Decks nur an Auflagern/Rampen anbinden, Tunnel volumetrisch öffnen.
Render- und Kollisionsprodukte aus denselben versionierten Terrain-/Bauwerksbedingungen.

Prüfhypothese Zellen/Bauweisen: endlicher Katalog parametrisierter Gelände-/Bauweisen
(Planierung, Einschnitt, Aufschüttung, Stützwand, Auflager, Portal), kontinuierliche
Maße und explizite Randverträge. Zellen organisieren Jobs/Residency; Bauwerke und
Alignments dürfen mehrere Zellen binden. Kleine Hang/Straße/Gebäude-Fixture zuerst:
widersprüchliche Anschlüsse, unveränderte Ergebnisse bei anderer Tile-Reihenfolge,
begrenzte Konfliktlösung/Neuplanung und Kosten messen. Noch keine beschlossene Runtime.
Referenz: [Merrell, Model Synthesis](https://paulmerrell.org/wp-content/uploads/2021/06/thesis.pdf),
Nachbarschafts-, Maß- und globale Verbindungsbedingungen; keine Echtzeitgarantie ableiten.
Ribbon-Randnormalen aus flächengewichteten Dreiecken ableiten, Deck-/Randvertices
für harte Kanten getrennt lassen. Gerade mit Steigung/Bank gegen alle Dreiecke und
Innenpunkt prüfen; Null-Schulter darf keine NaNs liefern. O(Vertex+Index), temporärer
Double-Puffer nur für Randvertices. Degenerierte Schulterbänder, Offset-Faltungen,
Wasserdichtheit und gemessene Produktionsbudgets bleiben eigene offene Nachweise.

Null-Schulter: nur das mittlere Querschnittsband triangulieren, vorhandene Bänder
in Deck/Unterseite und Endkappen identisch auswählen. Fester Vertexlayout-Vertrag
bleibt erhalten. Geometrisch verschweißte Kanten brauchen genau zwei gegenläufige
Flächen; geneigtes Prisma zusätzlich gegen analytisches Volumen prüfen.

Horizontale Offset-Regularität: maxAbsCurvature über das gesamte abgefragte Intervall
analytisch aus Segment-Endwerten bestimmen (Gerade/Bogen konstant, Klothoide linear).
Teilintervalle clippen, keine Stationsstichprobe als Beweis. Sweep vor Allokation
ablehnen, wenn Gesamt-Halbbreite inklusive Schulter den minimalen Radius erreicht.
Tests: beide Drehrichtungen, Grenzradius, schmale gültige Straße und verborgene
Krümmungsspitze zwischen Meshstationen. Globale Selbstüberschneidung bleibt offen.

ReferenceLine-Grundvertrag nachziehen: Lay prüft Ausgangskoordinaten, Winkel und
Segmentdaten nicht vollständig auf Endlichkeit; Fasten prüft Stationen, aber keine
Knot.Value/RatePerM. Lay baut Ersatz getrennt auf; abgelehnte Lay/Rise/Bank erhalten
Geometrie und Profile, erfolgreiche Updates löschen alte Diagnosen. Unabhängiger
Zustandstest scheitert am Altstand und besteht mit der Korrektur. Endliche Ergebnisse
und numerisch darstellbare Segment-/Profilintervalle bleiben zu prüfen.

kTangentTolerance wird derzeit für Krümmung (1/m) und Profilstationen (m) benutzt,
ist aber aus einem Winkel abgeleitet. Dimensionsrichtige Grenzwerte und den
gewünschten Kontinuitätsvertrag separat festlegen; keine unbegründete C2-Pflicht
als allgemeine Format-/Geometriebedingung übernehmen.
