Type: feature
State: open
Area: generators, scenario
Tags: webcam, measured
Depends: 2173, 2126, 2127, 2133

# Sandbox population is plausible, seeded and independent of the reference photo

## Ziel und IST

Die Webcam-Szenen leben auch von Fahrzeugen, Hafeninventar, Wegenutzung und Menschen.
OSM/DEM/Zeit/Wetter liefern nicht deren beobachteten Echtzustand. Die Sandbox muss ihn
auch nicht erzeugen: Sie platziert eine plausible eigene Population mit reproduzierbarem Seed.
Die neun aktuellen Places wirken leer; das ist eine Generierungs-, keine Rekonstruktionslücke.

## Implementierung

- Straßen-/Nutzungs-/Hafenflächen aus OSM in erlaubte Platzierungsräume überführen;
  Abstand, Ausrichtung, Kollisionsfreiheit, Rampen, Brückenfreiheit, Wassertiefe soweit
  vorhanden und Fußgängerzugang prüfen. Unbekannte Tiefe nicht als vermessen ausgeben.
- Generische Stadtmöblierung/Markierungen prozedural; Fahrzeuge, Menschen, Boote aus
  deklarierten glTF-Assets gemäß Generator/Import-Grenze. Keine Foto-/Assetpflicht für Gelände.
  Szenario kann Population explizit leeren oder besetzen; Kamera darf Spawn nicht reseeden.
- Nutzungs-/Zeit-/Wetterabhängige Dichten mit Budget und stabilen IDs. Bewegte Akteure
  später an 2134/2136 anschließen; statische glaubhafte Belegung benötigt keine LLM-Minds.
- Persistente Nahbereichsakteure, zusammengefasste Fernsimulation, Pooling/Instancing/LOD;
  Streaming-Reentry darf belegte Parkplätze nicht zufällig neu würfeln.

## Abnahme

- [ ] Husum/Malcesine/Olympiaturm tragen plausible eigene Belegung; kein Vergleich konkreter
      Boots-/Personenzahlen oder Kennzeichen mit dem Foto.
- [ ] Same seed + gleiche Daten/Events = gleiche Population; anderer Seed ändert Belegung,
      erhält Nutzungs-/Kontaktregeln. Absichtlich unerlaubtes Spawn scheitert am Oracle.
- [ ] 2092/2143 messen Dichteleiter, Rückkehr und Speicher; keine Population außerhalb Budget.

Wahl: generische PCG-Platzierung wie das öffentliche Unreal-Konzept; RAGE ist visueller
Sandbox-Benchmark, keine Quelle für erfundene Implementierungsdetails.
