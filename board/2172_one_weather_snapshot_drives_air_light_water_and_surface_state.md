Type: feature
State: open
Area: world, scenario, render
Tags: webcam, measured
Depends: nothing

# One weather snapshot drives air, light, water and surface state

## IST

`PlaceCamera.cpp` setzt Haze = 0 für sämtliche Places. Alle neun IST-Himmel sind wolkenlos,
auch neben bedeckten Webcam-Bildern. `src/world/weather/WeatherProvider.h` hat Wind,
CloudLayers und Visibility; `CalmWeather.h` liefert klare, windstille Defaults.
Eine verfügbare Deklaration belegt noch keinen Verbraucher im Bild.

## Implementierung

1. Zeitgestempelter räumlicher Wetter-Snapshot aus Provider/Scenario, mit Quelle, Einheiten,
   Höhenbezug, Gültigkeit und Auflösung; Replay friert genau diese Daten ein. Vorhandene
   Scenario-Felder wiederverwenden, keine zweite konkurrierende Wetterdeklaration.
2. Wind (NED → Weltbasis), Sichtweite, Wolkenanteile/Basis, Niederschlag und Temperatur
   konsistent an Atmosphäre, 2140, 2167, 2129 und Materialien liefern. Nicht vorhandene
   Größen explizit als plausible Ableitung kennzeichnen; keine historische Messung erfinden.
3. Sichtweite auf Aerosolextinktion abbilden, Rayleigh nicht doppelt zählen. Homogene
   Näherung: beta_total = -ln(0.02)/V = 3.912/V; Aerosolanteil aus Gesamtextinktion minus
   Molekülanteil mit gültigen Grenzen. Höhenprofile/Nebel getrennt deklarieren.
4. Feuchte-/Schnee-/Pfützenzustand zeitlich integrieren, nicht Regen-Bool auf alle Flächen
   setzen. Ohne Wettervorgeschichte plausiblen Anfangszustand deklarieren. Wolkenfeld aus
   Seed, räumlicher Korrelation und Wind erzeugen; seine einzelne Form ist erfunden.
5. Asynchrone Abfrage, begrenzte räumliche Updates und stetige zeitliche Übergänge.
   Uhrzeit, Sonnenrichtung, Belichtung, Wolkenlicht und Boden benutzen denselben Snapshot.

## Abnahme

- [ ] Klar/bedeckt/Regen/Nebel/Schnee bei identischem Ort ergeben physikalisch zusammenhängende
      Änderungen. Keine Forderung nach der einzelnen Wolke des Webcam-Fotos.
- [ ] Winddrehen bewegt Wolken und Wasser konsistent; Sichtweitenreihe hat monotonen
      Kontrastverlust. Snapshot-Replay liefert identischen Zustand; absichtliches Abklemmen
      eines Verbrauchers scheitert am jeweiligen Wirkungsoracle.
- [ ] Keine Wetter-IO/Generierung blockiert einen Frame; 2092 misst Wechsel unter Bewegung.

Wahl: eine deklarative Wetterquelle mit physikalischen Verbrauchern statt handgemalter
Place-Looks. Unreal-Atmosphäre ist die technische Referenz, RAGE-Timecycle nur das
vergleichbare Organisationsprinzip, keine übernommene proprietäre Implementierung.
