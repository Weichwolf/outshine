# Missions

Eine **Mission** ist externalisierte Config, identisch geladen von den **E2E-Tests** (headless) und
vom **Command Center** (per URL: `http://localhost:8080/?mission=<name>`, `<name>` = Dateiname ohne
`.json`). Sie beschreibt einen kompletten Flug — Start-Flughafen+Bahn → Wegpunkte → Ziel-Flughafen+Bahn
— den iNav **nativ** ausführt (LAUNCH / WP / RTH / LAND); das Command Center übersetzt nur.

## Format

```json
{
  "aircraft": "c172p",
  "takeoff": { "airport": "EDDH", "runway": "05" },
  "waypoints": [
    { "lat": 53.70, "lon": 10.10, "alt_agl": 300 },
    { "lat": 53.75, "lon": 10.30, "alt_agl": 500 }
  ],
  "land": { "airport": "EDDH", "runway": "23" }
}
```

- **`aircraft`** — Plugin-Verzeichnis (`aircraft/models/<name>`). Wählt eeprom + physics + profile.
- **`takeoff`/`land`** — ICAO + Runway-Ident. Koordinaten der Schwelle **und Ausrichtung (Heading)**
  kommen aus der Flughafen-DB (OurAirports), nicht aus der Mission. Der Takeoff-Flughafen setzt den
  Sim-Origin.
- **`waypoints[].alt_agl`** — Höhe **über Grund**. Nur das Command Center kennt AGL (DEM); es rechnet
  je WP `alt_agl + Bodenhöhe(lat,lon)` → ASL und gibt iNav reine GPS-Höhen (`MSP_SET_WP`).

## Die 3 E2E-Flieger/Flughäfen (gegen OurAirports validiert)

| Flieger | Flughafen | Start-Bahn | Lande-Bahn | Bahnlänge |
|---|---|---|---|---|
| `f16` | EDDF Frankfurt | 07C (hdg 69.6°) | 25C (249.6°) | 13123 ft ASP |
| `c172p` | EDDH Hamburg | 05 (hdg 50.3°) | 23 (230.3°) | 10663 ft ASP |
| `minisgs_e` | EDNY Friedrichshafen | 06 (hdg 60°) | 24 (240°) | 7729 ft ASP |

Headings aus `runways.csv` (`le/he_heading_degT`). Die Missions-Dateien referenzieren nur
`airport`+`runway`; Koordinaten/Heading löst der DB-Loader (Paket J) auf.
