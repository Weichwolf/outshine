# Mission format (.fbm)

Zero-dependency, zeilenbasiertes Textformat für den Missions-Orchestrator (`gpu_native --mission FILE`,
`fb-gym --mission FILE`). Parser: `sim/src/core/FBMissionFile.h` (`FBParseMissionFile`) — reine
Text-rein/`FBMission`-raus-Funktion, kein File-I/O (das macht die App).

Der Orchestrator (`FBMissionRunner.cpp`) führt genau vier Schritte aus und kennt dabei KEINE
Missions-Spezifika: Mission laden → Welt mit ihren Akteuren aufsetzen (Elevation auflösen, Modul
spawnen) → Akteure ausführen (Modul takten, beide Monitore füttern) → Welt validieren (die Monitore
haben das Urteil längst gefällt). Der Anfangszustand einer Unit ist reine Daten-Deklaration — kein
Boden-/Luft-Sonderfall im Code: `spawn` trägt Position + Höhe(-oder-Boden) + Kurs + Speed, eine EINZIGE
IC-Anwendung (`FBMissionBoot.h`s `FBMissionApplySpawn`) für beide Fälle.

## Syntax

Eine Anweisung pro Zeile, `#` leitet einen Kommentar bis Zeilenende ein, Leerzeilen werden ignoriert.
Reihenfolge: `name`, dann `module`, optional `runway` (muss vor `spawn threshold`/`land` stehen), dann
`spawn` (Pflicht), beliebig viele `wp`/`land`/`set`, `timeout` irgendwo.

### Bodenstart (Beispiel: `sim/missions/payerne-takeoff-only.fbm`)

```
name payerne-takeoff-only-1
module f16                                 # FBModuleRegistry-Key -> FBF16Module
runway 46.84335 6.91523 441.0 228.0 2889   # lat lon elevM trueHdgDeg lengthM
spawn threshold ground 228.0 0             # 'threshold' = die runway-Zeile oben, 'ground' = auf dem Fahrwerk
wp 46.75 6.80 2500 350                     # lat lon altM speedKt (enroute)
wp 46.90 7.05 3000 350
land                                        # zurueck auf die runway-Zeile oben
timeout 600                                 # Sim-Sekunden bis TIMEOUT
```

### Luftstart (Beispiel: `sim/missions/payerne-airstart.fbm`)

```
name payerne-airstart-1
module f16
spawn 46.73202 6.73455 2500 48.0 300       # lat lon altM(ASL) hdgDeg speedKt -- kein runway, kein Boden
set gear up                                 # bereits eingefahren (ein echter Luftstart hat keinen Taxi)
set fuel_pct 60                             # 60% der modelleigenen Tankkapazität
wp 46.84335 6.91523 2500 300
wp 46.95000 7.05000 2800 300
timeout 500
```

| Keyword   | Felder | Bedeutung |
|---|---|---|
| `name`    | Rest der Zeile | Missionsname (Telemetrie/Logs) |
| `module`  | Rest der Zeile | Modulname, per `FBModuleRegistry` aufgelöst (heute nur `f16` registriert) — bestimmt sowohl das `FBModule` als auch den JSBSim-Aircraft-Ordnernamen (`vendor/jsbsim/aircraft/<module>`) |
| `runway`  | lat lon elevM trueHdgDeg lengthM | Optionale Landing-Geometrie (`FBRunway`) — nötig für `spawn threshold`, `land`, und `FBMissionMonitor`s Off-Runway-Touchdown-FAIL-Prüfung; eine reine Luftstart-Mission ohne Landeabsicht braucht keine `runway`-Zeile. Breite ungenutzt (0, Default-Fallback im Monitor). |
| `spawn`   | `<lat lon \| threshold>` `<altM \| ground>` `hdgDeg` `speedKt` | Pflicht, genau einmal: die deklarative IC der Unit — Position, Höhe-ODER-Boden, Kurs, Speed. `threshold` übernimmt lat/lon der zuvor deklarierten `runway`-Zeile (reine Schreib-Convenience, keine zweite Positions-Syntax). `ground` löst die Höhe aus Gelände + Fahrwerksgeometrie auf (sitzt auf dem Fahrwerk, wie ein Bodenstart es immer tat); ein numerischer Wert ist eine LITERALE ASL-Höhe (ein Luftstart). Beide Fälle durchlaufen dieselbe eine JSBSim-IC-Anwendung — kein getrennter Code-Pfad. |
| `set`     | `key value...` | Systemzustand als Missionsdaten — der Runner parst nur die KV-Liste und reicht sie im Spawn-IC-Fenster an `FBModule::ApplySetup(key, value)`; das MODUL interpretiert seine eigenen Schlüssel. Ein unbekannter Schlüssel ist ein Laufzeit-FAIL (Exit 1, `SET_UNKNOWN_KEY`-Event), kein Parse-Fehler. F-16 kennt heute: `gear` (`up`/`down`), `fuel_lbs` (absolute Tankmenge, lb), `fuel_pct` (0..100, Anteil der modelleigenen Gesamtkapazität — auf jeden Tank proportional zu dessen eigener Kapazität verteilt). |
| `wp`      | lat lon altM speedKt | `FBWaypoint` vom Typ `Enroute` |
| `land`    | — | `FBWaypoint` vom Typ `Land` AN der Runway-Schwelle (braucht eine vorherige `runway`-Zeile) |
| `timeout` | Sekunden (>0) | Sim-Zeit bis TIMEOUT, falls die Mission nicht vorher endet |

`name`, `module`, `spawn` und `timeout` sind Pflicht; `runway` ist optional (nur nötig für
`spawn threshold`/`land`/die Off-Runway-Prüfung). Jede unbekannte Zeile, ein fehlendes `module`, ein
`spawn threshold`/`land` vor der `runway`-Zeile oder ein `set` ohne Wert ist ein Parse-Fehler
(`FBParseMissionFile` liefert `false` + `"line N: ..."` in `*err`); ein `module`, das
`FBModuleRegistry` nicht kennt, oder ein unbekannter `set`-Schlüssel ist ein Laufzeit-FAIL des Runners
(nicht des Parsers).

**Konsistenz-Validierung beim Aufsetzen:** eine physikalisch widersprüchliche Deklaration ist ein FAIL,
sobald der Runner die Elevation aufgelöst hat (nicht schon im Parser, der keine Geodaten kennt) — heute:
eine explizite `spawn`-Höhe unterhalb des aufgelösten Bodens. `v=0` in der Luft ist dagegen KEIN Fehler
(legal, die Unit fällt dann eben — `FBFlightMonitor` urteilt darüber wie über jeden anderen Flugzustand).

## Ergebnis

`FBMission` = `Name` + `ModuleName` + `FBSpawn` (`HaveSpawn`) + optionale `FBRunway` (`HaveRunway`) +
`FBFlightPlan` (die `wp`/`land`-Zeilen in Dateireihenfolge) + `SetKV` (die `set`-Zeilen, Dateireihenfolge)
+ `TimeoutS`. Der Orchestrator (`FBMissionRunner.cpp`, geteilt von `fb-gym` und `gpu_native --mission`)
löst `ModuleName` über `FBModuleRegistry::Create` in ein `std::unique_ptr<FBModule>` auf und hält von da
an ALLES nur über die generischen `FBModule`-Accessoren (`Autopilot()`/`FlightControl()`/
`PilotSystem()`/`Controls()`/`Displays()`/`AirDataSystem()`/`FlightPlan()`/`Telemetry()`/
`ApplySetup()`) — der Runner selbst nennt nie einen konkreten Modultyp und enthält keinen
missions-spezifischen Code (kein Runway-Schwellen-Spawn, kein Wegpunkt-Advance — beides sitzt in
`FBMissionBoot.h::FBMissionApplySpawn` bzw. `systems/FBNavSystem::AdvanceWaypoint`, dem Modul selbst).
Die Bodenhöhe für einen `ground`-Spawn kommt aus dem injizierten `FBElevationProvider` (der
Elevation-Hook: `--elev tiles|const|swiss`, siehe `sim/src/core/FBElevationProvider.h`) — die
Datei-Elevation der `runway`-Zeile ist nur Doku/Fallback.

Das Missions-URTEIL (Wegpunkte erreicht, Bodenkontakt abseits der Runway, Timeout, Landung auf der
Ziel-Runway) fällt `core/FBMissionMonitor`, das FBFlightMonitor-Geschwister: dieselbe unbestechliche
Struktur (Runner-/App-eigen, ein Modul sieht ihn nie), aber die MISSIONS-Frage statt der physikalischen.
Er trägt seine EIGENE Kopie von `FBFlightPlan`/`FBRunway` (aus der Missionsdatei, nie das Modul-eigene,
live mutierte Exemplar) und liest Fortschritt nur aus beobachteter Position — ein Modul kann sich nicht
per Selbstauskunft zu SUCCESS melden. `core/FBFlightMonitor` bleibt daneben unverändert die physikalische
K.O.-Instanz (Absturz/LOC) — zwei Instanzen, zwei Fragen, nie vermischt; beide laufen in JEDEM Client
(Runner UND der WASM-App-eigene Frame-Loop).

Endet der Flugplan auf einer `land`-Zeile (`FBWaypointType::Land`, immer die Runway-Schwelle), gilt für
DIESEN letzten Wegpunkt eine andere SUCCESS-Regel als für `wp`: kein einfaches Capture-und-weiter, sondern
**Stillstand auf der zugewiesenen Runway** — Fahrwerk mit Bodenkontakt, Groundspeed unter ~2 kt, Position
innerhalb des Runway-Footprints (0 m Längs-, 15 m Quer-Marge). Ein bloßes Überfliegen der Schwelle in
Fluggeschwindigkeit ist noch keine Landung. `systems/FBPilot`s Phasenmaschine liefert dazu die
Flugführung — `Approach` (`FBAutopilot::Course`, ein Lokalizer/Glidepath-artiges Linienverfolgen der
verlängerten Runway-Achse, `doc/f16/navigation-ils.md`) → `Flare` (Sinkrate kurz vor der Schwelle
brechen) → `Rollout` (Aerobrake-Haltung bis zur Ausroll-Geschwindigkeit, dann Bugrad runter, Bremsen +
Bugrad-Lenkung bis zum Stillstand) — aber das URTEIL bleibt beim Monitor, nicht beim Piloten: ein Modul
kann sich auch hier nicht per Selbstauskunft zu SUCCESS melden.

Heute beschreibt EINE `.fbm`-Datei genau EIN steuerbares Modul (ein `module`/ein `spawn`/ein
`FBFlightPlan`). Das ist eine Eigenschaft des heutigen Runners, keine feste Grenze des Datenmodells:
eine künftige Mehr-Einheiten-Mission (ein Verband aus mehreren Modulen/Fraktionen, z.B. F-16 vs.
MiG-29 im Gym) ist eine Mission mit einer LISTE solcher Pro-Einheit-Blöcke (`module`+`spawn`+
Flugplan+Fraktion), keine Neukonstruktion dieses Formats — noch nicht gebaut, nur nicht durch
Annahmen wie "genau ein Modul" tief im Parser/Runner verbaut.

## Beispiele

- `sim/missions/payerne-takeoff-only.fbm`, `sim/missions/payerne-takeoff.fbm` — Payerne (LSMP) Runway
  23, Bodenstart; Schwellen-Koordinaten und -Länge gegen `fb-tiles`' `/elev`-Endpunkt geprüft (DEM
  ~441 m an der Schwelle).
- `sim/missions/payerne-airstart.fbm` — derselbe Luftraum, aber ein reiner Luftstart (~10 nm SW von
  Payerne, 2500 m ASL, 300 kt, Fahrwerk eingefahren, 60% Tankfüllung) — keine `runway`-Zeile, kein
  Taxi/Rollout.
- `sim/missions/payerne-landing.fbm` — das Landetrainingsgelände (Phase 3): Luftstart ~9 nm auf
  RWY23-Endanflug ausgerichtet, ~500 m AGL (unter dem nominellen 3°-Glidepath — ein realistischer
  Glidepath-Capture-von-unten-Fall), dann `land` — SUCCESS = Stillstand auf der Runway.
- `sim/missions/payerne-full.fbm` — die Ziel-Mission: Bodenstart Payerne → drei Wegpunkte (SW-Climb-out,
  Schleife über die Broye/Jura-Vorberge, Rückkehr auf RWY23-Endanflug) → `land` auf derselben Runway wie
  der Start.
