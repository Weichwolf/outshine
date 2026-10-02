Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends: 2188
Area: world, generators, render
Tags: weather, clouds, astronomy, seasons

# Weather and astronomy drive a coherent sky and world

## Ergebnis und vorhandene Fähigkeit
Atmosphäre, Wolken, Sonnen-/Mondlicht, sichtbare Planeten und Sterne passen zu Ort/Zeit.
Wind, Regen, Nässe, Schnee und Schmelze verändern dieselbe Welt. SkyStage besitzt
Atmosphären-LUTs; Wetterdeklarationen besitzen Werte, aber Open-Meteo-Anschluss,
Wolkenrenderer und kohärenter Materialzustand fehlen. Himmel umfasst häufig 1/3–2/3
des Bildes; Wolken sind eine zentrale Lieferung, kein später Restbudget-Effekt.

## Nächste Lieferung und Besitzer
`world/weather/WeatherProvider` ist derzeit ein internes Abfrageinterface ohne UTC,
Gültigkeit und Herkunft. Durch den öffentlichen Snapshot aus 2188 ersetzen; keine
zweite parallele Wetterwelt.
2188 liefert den noch fehlenden öffentlichen Wetter-Snapshotvertrag.
Open-Meteo nutzt öffentliche Provider-/IO-/Quellcache-Verträge; fehlende/alte Werte
bleiben sichtbar. Zuerst Snapshot und eine begrenzte Wolkenschicht samt Weltlicht anbinden.
Generatoren besitzen Dichtefelder; Render besitzt Integration/History und Kameraantwort.
Bedeckte Stadtbilder brauchen diffuse Himmelsfüllung und weiche Schatten; klare
Bergbilder brauchen räumlich getrennte Luftschichten statt eines globalen Nebelfilters.

## Umsetzung und Invarianten
- Sonne, Mond und sichtbare Planeten aus astronomischen Modellen/UTC/Beobachterposition;
  Phase, scheinbare Richtung/Helligkeit und Verdeckung berücksichtigen. Merkur/Venus/
  Mars/Jupiter/Saturn, situationsabhängig Uranus/Neptun; keine weitere Live-Quelle.
  Fester lizenzierter/versionierter Sternenkatalog, atmosphärische Extinktion und Wolkenverdeckung.
- Weltverankerte Wolkendichte, Windadvektion und begrenztes Raymarching mit reduzierter
  Auflösung/temporaler Rekonstruktion. Disocclusion und Quellenwechsel verwerfen History.
  Transmittanz/Radiance einmal komponieren; dieselbe Dichte liefert Wolkenschatten/Himmelsfüllung.
- Regen/Nässe, Schnee/Schmelze folgen Wetterhistorie und Untergrund/Exposition. Ein
  Tagesdatum oder pauschale weiße Höhenmaske ersetzt keinen Zustand; bestehende Materialien nutzen.
- Nebel/feuchte Luft folgen Höhen-/Wetterzustand; Atmosphäre verarbeitet Tiefe und
  Verdeckung konsistent mit Weltlicht. Wolkenformen bleiben plausible Ergänzung.
- Wetter ändert Wasser/Vegetation über denselben Snapshot. Quellenwechsel erneuert keine
  unveränderte Stadt; Himmel/Wolken konkurrieren nach Bildgewinn im gemeinsamen Budget.

## Abnahme
Bedeckter Himmel, Dunst, Nacht und Winter-/Schmelzfall verbessern passende reale Places.
Sonne/Mond/Planeten besitzen plausible Positionen; bewegte Wolken beleuchten auch den
Boden kohärent. Fehlende Mess-/Wetterdaten bleiben benannt, keine exakte Wolkenrekonstruktion behaupten.
