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

## Auswahl der 3 E2E-Flieger/Flughäfen (provisorisch, DB-validiert)

| Flieger | Charakter | Flughafen (Vorschlag) | Grund |
|---|---|---|---|
| `f16` | schneller Jet | EDDF Frankfurt | lange Bahn |
| `c172p` | GA-Prop | EDDH Hamburg | mittlere Bahn |
| `minisgs_e` | Motorsegler | EDNY Friedrichshafen | kleiner/GA-Platz |

Runway-Idents/Headings werden gegen die Flughafen-DB validiert, sobald die (Paket J) steht.
