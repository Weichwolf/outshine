Type: feature
State: active
Parent: 2169
Area: world, scenario, render
Tags: webcam, measured
Depends:

# One weather snapshot drives air, light, water and surface state

## IST

`PlaceCamera.cpp` setzt Haze = 0 für sämtliche Places. Alle neun IST-Himmel sind wolkenlos,
auch neben bedeckten Webcam-Bildern. `src/world/weather/WeatherProvider.h` hat Wind,
CloudLayers und Visibility; `CalmWeather.h` liefert klare, windstille Defaults.
Eine verfügbare Deklaration belegt noch keinen Verbraucher im Bild.

## Weltweiter Licht- und Atmosphärenvertrag

Volumetrisches Licht, Schatten, Luftperspektive und Farbgestaltung sind eine gemeinsame
Kernkompetenz. Ort, Datum, Uhrzeit, Höhe und Wetter treiben denselben konsistenten
Zustand für direkte/indirekte Beleuchtung, Medium und Belichtung. Sonnen- und lokale
Lichtquellen einschließlich Abschattung integrieren; klare Luft braucht subtile Tiefe,
Nebel/Gegenlicht stärkere Streuung. Keine fest eingestellten Place-/Sonnenuntergangs-Looks.

Abnahmematrix: beide Hemisphären, Äquator, mittlere Breiten, Polarregionen und Hochgebirge;
Jahreszeiten, Morgen/Mittag/Abend/Nacht, Polartag und Polarnacht; klar/bedeckt/Nebel.
Astronomisch unmögliche Kombinationen ausschließen. Repräsentative Fälle prüfen den
allgemeinen Vertrag, ohne Vollabdeckung sämtlicher Orte/Zeitpunkte zu behaupten.
Bei Kamera-, Zeit- und Wetterübergängen auf Flimmern, Nachziehen, Kontrast-/Farbsprünge
prüfen; lesbare Schatten und stabile Tiefenstaffelung erhalten. 2092 misst Frame-/Speicherkosten.
Zuständigkeiten: 2167 indirektes Licht, 2128 Schatten/Lichter, 2140 Wolken, 2155 Farbantwort;
2213 verbindet Sonne, Mond und Sternenhimmel mit demselben Weltzustand.

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

## Deklarationsvertrag und Export
Scenario::Weather dokumentieren: dimensionslose Wolkenanteile, Basis AGL, Wind in
m/s und meteorologische Herkunftsrichtung; derzeit keine Cloud-/Wind-Verbraucher.
Haze skaliert nur Mie-Streuung/Extinktion, nicht Rayleigh/Ozon; >1 ist möglich.
Writer erhält jetzt alle sieben Wind-/Wolkenfelder; Altcode verletzt den Roundtrip.
Sechs Writer-Tests samt exakten Wetterwerten und Nullgrenzen bestehen. Gemeinsame
Werte-/Typprüfung an Import-, Declare- und Exportgrenze bleibt erforderlich; derzeit
keine vollständige Wettervalidierung oder Wetterwirkung behaupten.

## Gemeinsame Wettergrenze
Ein internes Feldschema verbindet XML-Namen, Member und Grenzen für Import/Export
und API-Vorprüfung: Wolken [0,1], AGL/Windgeschwindigkeit >=0, Windrichtung endlich
und unnormalisiert, Haze [0,float-max] für die aktuelle GPU-Verengung. Keine Clamps.
XML verlangt vollständige endliche Zahlen; fehlende Attribute behalten Defaults.
Ablehnung vor Veröffentlichung, einschließlich inaktiver Ground-Deklarationen.
Abnahme: alle Felder mit NaN/Inf/negativen Grenzen, Parser-Tokens, Erhaltung und Retry.
