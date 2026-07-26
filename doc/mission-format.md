# Mission format (.fbm)

Zero-dependency, zeilenbasiertes Textformat für den nativen Missions-Runner (`gpu_native --mission
FILE`). Parser: `sim/src/core/FBMissionFile.h` (`FBParseMissionFile`) — reine
Text-rein/`FBMission`-raus-Funktion, kein File-I/O (das macht die App).

## Syntax

Eine Anweisung pro Zeile, `#` leitet einen Kommentar bis Zeilenende ein, Leerzeilen werden ignoriert.
Reihenfolge: `name`, dann `module`, dann `runway` (muss vor `takeoff`/`land` stehen), dann beliebig
viele `wp`/`takeoff`/`land`, `timeout` irgendwo.

```
name payerne-takeoff-1
module f16                                 # FBModuleRegistry-Key -> FBF16Module
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
| `module`  | Rest der Zeile | Modulname, per `FBModuleRegistry` aufgelöst (heute nur `f16` registriert) — bestimmt sowohl das `FBModule` als auch den JSBSim-Aircraft-Ordnernamen (`vendor/jsbsim/aircraft/<module>`) |
| `runway`  | lat lon elevM trueHdgDeg lengthM | Schwellen-Geometrie (`FBRunway`); Breite ungenutzt (0, Default-Fallback im Runner) |
| `takeoff` | — | fügt einen `FBWaypoint` vom Typ `Takeoff` AN der Runway-Schwelle in den Flugplan ein |
| `wp`      | lat lon altM speedKt | `FBWaypoint` vom Typ `Enroute` |
| `land`    | — | `FBWaypoint` vom Typ `Land` AN derselben Schwelle |
| `timeout` | Sekunden (>0) | Sim-Zeit bis TIMEOUT, falls die Mission nicht vorher endet |

`name`, `module`, `runway` und `timeout` sind Pflicht; jede unbekannte Zeile, ein fehlendes `module`
oder ein `takeoff`/`land` vor der `runway`-Zeile ist ein Parse-Fehler (`FBParseMissionFile` liefert
`false` + `"line N: ..."` in `*err`); ein `module`, das `FBModuleRegistry` nicht kennt, ist ein
Laufzeit-FAIL des Runners (nicht des Parsers — die Registry ist Runner-seitig, nicht im Parser).

## Ergebnis

`FBMission` = `Name` + `ModuleName` + `FBRunway` (mit `HaveRunway`) + `FBFlightPlan` (die `wp`/
`takeoff`/`land`-Zeilen in Dateireihenfolge) + `TimeoutS`. Der Runner (nativer `--mission`-Pfad wie
`fb-gym`) löst `ModuleName` über `FBModuleRegistry::Create` in ein `std::unique_ptr<FBModule>` auf und
hält von da an ALLES nur über die generischen `FBModule`-Accessoren (`Autopilot()`/`FlightControl()`/
`PilotSystem()`/`Controls()`/`Displays()`/`AirDataSystem()`/`FlightPlan()`/`Telemetry()`) — der Runner
selbst nennt nie einen konkreten Modultyp. Ground-Spawn sitzt am Boden auf der Runway-Schwelle
(Position/Heading aus `runway`), die Bodenhöhe kommt aber aus dem injizierten `FBElevationProvider`
(der Elevation-Hook: `--elev tiles|const|swiss`, siehe `sim/src/core/FBElevationProvider.h`) neu — die
Datei-Elevation ist nur Doku/Fallback.

Heute beschreibt EINE `.fbm`-Datei genau EIN steuerbares Modul (ein `module`/eine `runway`/ein
`FBFlightPlan`). Das ist eine Eigenschaft des heutigen Runners, keine feste Grenze des Datenmodells:
eine künftige Mehr-Einheiten-Mission (ein Verband aus mehreren Modulen/Fraktionen, z.B. F-16 vs.
MiG-29 im Gym) ist eine Mission mit einer LISTE solcher Pro-Einheit-Blöcke (`module`+`runway`/Spawn+
Flugplan+Fraktion), keine Neukonstruktion dieses Formats — noch nicht gebaut, nur nicht durch
Annahmen wie "genau ein Modul" tief im Parser/Runner verbaut.

## Beispiel

`sim/missions/payerne-takeoff.fbm` — Payerne (LSMP) Runway 23; Schwellen-Koordinaten und -Länge gegen
`fb-tiles`' `/elev`-Endpunkt geprüft (DEM ~441 m an der Schwelle, Datei aktualisiert).
