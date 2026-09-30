Type: feature
State: active
Architecture: planned
Priority: P1
Parent: 2169
Depends:
Area: public-api, world, scenario, render
Tags: webcam, open-meteo, weather

# One public weather provider drives a coherent world state

## Ergebnis und Iststand
Open-Meteo liefert das Wetter für Atmosphäre, Licht, Wolken, Wasser und Materialien.
Bibliotheksnutzer können ihren eigenen Wetterprovider über denselben öffentlichen
Vertrag einsetzen. Die Umsetzung folgt auf DSM und die Wiederherstellung der OSM-Places.
Die vorhandenen Scenario-Felder bewahren sieben Wolken-/Windwerte; Haze erreicht bereits
EarthMedium. Wolken-/Windwerte besitzen noch keine Renderwirkung. Die privaten
WeatherProvider/CalmWeather-Typen sind weder Live-Beschaffung noch öffentliche Erweiterung.

## Öffentlicher Quellen- und Snapshotvertrag
- Die bestehende Providerregistrierung und der gemeinsame Source-/Transportpfad erhalten
  die native Datenart Wetter. Open-Meteo-JSON endet im Adapter; keine private zweite Factory.
  Vor Umsetzung öffentliche Nachfrage nach Ort/UTC-Zeit und WeatherSnapshot festlegen.
  API ist Greenfield; bestehende Scenario-/API-Aufrufer vollständig migrieren.
- Ein unveränderlicher Snapshot besitzt Quelle, Revision, Ort, räumliche Auflösung,
  Gültigkeitszeit, Einheiten und Höhenbezug. Fehlende, deklarierte und aus Messwerten
  abgeleitete Größen bleiben unterscheidbar. Replay pinnt genau diesen Zustand.
- Der Client nutzt ausschließlich Open-Meteo. Forecast-/Archivantworten müssen zum
  geforderten Place-Zeitpunkt passen; Modellwerte werden keine behaupteten Stationsmessungen.
  Nur empfangene Originalantworten persistent cachen. Begrenzte parallele IO verwendet
  denselben Transport wie DSM/OSM; Interpretation läuft auf dem einzigen Compute-Worker.
- Engine publiziert zusammenhängende Snapshots an Tick-/Framegrenzen; Fehler erhalten
  den letzten gültigen Zustand. Veraltete Antworten überschreiben keinen neueren Bedarf.
  Keine IO oder unbeschränkte Arbeit im Frame; keine erfundene Live-Calm-Weather-Antwort.

## Wirkung und Besitzer
world/weather normalisiert Einheiten, UTC und meteorologischen Wind-from in die native
Raumbasis. Niederschlag, Temperatur, Sichtweite, Wind und Wolkenanteile/-basis treiben
Atmosphäre, 2140 Wolken, 2167 indirektes Licht, 2129 Wasser und Materialien gemeinsam.
Haze-/Klarluftverhalten erhalten; Wolkenform folgt Seed, räumlicher Korrelation und Wind.
Ihre einzelne Form bleibt prozedural. Feuchte/Schnee/Pfützen werden zeitlich integriert;
Wettervorgeschichte und Anfangszustand besitzt 2325. Kein Regen-Bool für alle Oberflächen.

Homogene Sichtweitennäherung: beta_total = -ln(0.02)/V = 3.912/V, mit V in Metern.
Rayleigh-/Aerosolanteile getrennt bilanzieren; kein doppelt gezählter Molekülanteil.
Höhenprofile und lokale Nebelvolumen erhalten eigenen räumlichen Zustand. Sonne/Mond,
Schatten, Luftperspektive und Belichtung folgen demselben Weltzustand; keine Place-Looks.
Lokale Nebelbänke, Tunnelabluft und Lichtkegel integrieren dieselben Lichter/Caster aus
2128. 2213 besitzt den astronomischen Himmel, 2155 dessen Farb-/Belichtungsantwort.

Wetter-/Zeitrevisionen invalidieren begrenzte Renderinputs, LUTs und History, keine
Terrain-/Geometrieprodukte. Hillaire-Sky-/Aerial-LUTs und Bruneton-Streuung anhand gleicher
Klar-/Dunstfälle, Hochlage, Zenit/Horizont und Wechsel messen. Konsistente Sky-/Ground-
Irradiance und Transmittance sind verbindlich; Hostzeiten beweisen kein A18-Pro-Budget.
Referenzen: https://sebh.github.io/publications/egsr2020.pdf ;
https://ebruneton.github.io/precomputed_atmospheric_scattering/ ;
https://advances.realtimerendering.com/s2019/index.htm .

## Abnahme
Eigener Provider nur mit öffentlichen Includes erreicht das Wetter im Client-Bild.
Klar/bedeckt/Regen/Nebel/Schnee und Windwechsel ergeben zusammenhängende Wirkung,
monotonen Sichtweiten-Kontrastverlust und reproduzierbaren Snapshot-Replay. Prüfung
über Hemisphären, Äquator, Polarregion, Hochlage, Jahres-/Tageszeiten und Polartag/-nacht.
Nachtstraße/Lichtkegel reagieren auf Nebel und Caster; Schwenks erzeugen kein Ghosting.
Wetterwechsel erhalten Weltgeometrie und Uploads. Fehlende Daten bleiben sichtbar;
physikalisch ungültige Werte, Einheiten und wind-from-Umkehrungen werden erkannt.
Quelle/Gültigkeit, Place-PNG und getrennte CPU/GPU-/Speicherbudgets nach 2092 belegen
Integration. Erhaltene sieben Scenario-Werte und API-/Replay-Verträge bleiben vollständig.
