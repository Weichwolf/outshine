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
