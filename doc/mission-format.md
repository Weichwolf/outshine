# Mission format (.fbm)

Zero-dependency, zeilenbasiertes Textformat für den nativen Missions-Runner (`gpu_native --mission
FILE`). Parser: `sim/src/core/FBMissionFile.h` (`FBParseMissionFile`) — reine
Text-rein/`FBMission`-raus-Funktion, kein File-I/O (das macht die App).

## Syntax

Eine Anweisung pro Zeile, `#` leitet einen Kommentar bis Zeilenende ein, Leerzeilen werden ignoriert.
Reihenfolge: `name`, dann `runway` (muss vor `takeoff`/`land` stehen), dann beliebig viele `wp`/
`takeoff`/`land`, `timeout` irgendwo.

```
name payerne-takeoff-1
runway 46.84335 6.91523 441.0 228.0 2889   # lat lon elevM trueHdgDeg lengthM
takeoff
wp 46.75 6.80 2500 350                     # lat lon altM speedKt (enroute)
wp 46.90 7.05 3000 350
land                                        # zurueck auf die runway-Zeile oben
timeout 600                                 # Sim-Sekunden bis TIMEOUT
```

| Keyword | Felder | Bedeutung |
|---|---|---|
| `name`    | Rest der Zeile | Missionsname (Telemetrie/Logs) |
| `runway`  | lat lon elevM trueHdgDeg lengthM | Schwellen-Geometrie (`FBRunway`); Breite ungenutzt (0, Default-Fallback im Runner) |
| `takeoff` | — | fügt einen `FBWaypoint` vom Typ `Takeoff` AN der Runway-Schwelle in den Flugplan ein |
| `wp`      | lat lon altM speedKt | `FBWaypoint` vom Typ `Enroute` |
| `land`    | — | `FBWaypoint` vom Typ `Land` AN derselben Schwelle |
| `timeout` | Sekunden (>0) | Sim-Zeit bis TIMEOUT, falls die Mission nicht vorher endet |

`name`, `runway` und `timeout` sind Pflicht; jede unbekannte Zeile oder ein `takeoff`/`land` vor der
`runway`-Zeile ist ein Parse-Fehler (`FBParseMissionFile` liefert `false` + `"line N: ..."` in `*err`).

## Ergebnis

`FBMission` = `Name` + `FBRunway` (mit `HaveRunway`) + `FBFlightPlan` (die `wp`/`takeoff`/`land`-Zeilen
in Dateireihenfolge) + `TimeoutS`. Der native Runner spawnt am Boden auf der Runway-Schwelle (Position/
Heading aus `runway`), lädt die Runway-Höhe aber aus dem fb-tiles-DEM neu (die Datei-Elevation ist nur
Doku/Fallback).

## Beispiel

`sim/missions/payerne-takeoff.fbm` — Payerne (LSMP) Runway 23; Schwellen-Koordinaten und -Länge gegen
`fb-tiles`' `/elev`-Endpunkt geprüft (DEM ~441 m an der Schwelle, Datei aktualisiert).
