# Mission format (.fbm)

Zero-dependency, zeilenbasiertes Textformat für den Missions-Orchestrator (`gpu_native --mission FILE`,
`fb-gym --mission FILE`). Parser: `sim/src/core/FBMissionFile.h` (`FBParseMissionFile`) — reine
Text-rein/`FBMission`-raus-Funktion, kein File-I/O (das macht die App).

Eine Mission beschreibt einen **VERBAND**: missionsweite Daten (Name, optionale Runway, Timeout) plus
eine **Liste von Akteursblöcken** (`unit <callsign>`). Jeder Block ist genau eine simulierte Einheit
(`units/FBSimUnit`) mit eigenem Modul, eigener Fraktion, eigenem Anfangszustand und eigenen Zielen. Ein
Einzelflug ist der Sonderfall „ein Block" — kein zweiter Dialekt, kein Sonderpfad im Code.

Der Orchestrator (`FBMissionRunner.cpp`) führt genau vier Schritte aus und kennt dabei KEINE
Missions-Spezifika: Mission laden → Welt mit ihren Akteuren aufsetzen (je Akteur Elevation auflösen,
Modul spawnen) → Akteure ausführen (jedes Modul takten, beide Monitore je Einheit füttern) → Welt
validieren (die Monitore haben das Urteil längst gefällt). Der Anfangszustand einer Unit ist reine
Daten-Deklaration — kein Boden-/Luft-Sonderfall im Code: `spawn` trägt Position + Höhe(-oder-Boden) +
Kurs + Speed, eine EINZIGE IC-Anwendung (`FBMissionBoot.h`s `FBMissionSpawnActor`) für beide Fälle.

## Syntax

Eine Anweisung pro Zeile, `#` leitet einen Kommentar bis Zeilenende ein, Leerzeilen werden ignoriert,
führende Einrückung ist rein kosmetisch. **Zwei Geltungsbereiche:**

- **missionsweit** — `name`, `runway`, `timeout`. Müssen VOR dem ersten `unit`-Block stehen.
- **akteursbezogen** — `module`, `team`, `spawn`, `set`, `wp`, `land`. Nur INNERHALB eines
  `unit`-Blocks; ein Block läuft bis zum nächsten `unit` oder Dateiende.

Beide Richtungen sind harte Parse-Fehler (eine `runway`-Zeile zwischen zwei Units wäre sonst still
„missionsweit, nur spät deklariert"; eine `spawn`-Zeile vor dem ersten `unit` hätte keinen Besitzer).

### Einzel-Jet (Beispiel: `sim/missions/payerne-takeoff-only.fbm`)

```
name payerne-takeoff-only-1
runway 46.84335 6.91523 441.0 228.0 2889     # lat lon elevM trueHdgDeg lengthM (missionsweit)
timeout 600                                  # Sim-Sekunden bis TIMEOUT

unit viper                                   # Blockbeginn: das Callsign dieser Einheit
  module f16                                 # FBModuleRegistry-Key -> FBF16Module
  spawn threshold ground 228.0 0             # 'threshold' = die runway-Zeile oben, 'ground' = auf dem Fahrwerk
  wp 46.74293 6.75267 2500 350               # lat lon altM speedKt (enroute)
  wp 46.66467 6.62666 3000 350               # kein 'land': SUCCESS = beide Wegpunkte erreicht
```

### Paar (Beispiel: `sim/missions/payerne-pair.fbm`)

```
name payerne-pair-1
runway 46.84335 6.91523 441.0 228.0 2889     # missionsweit: gilt für JEDE Einheit (Off-Runway-Prüfung)
timeout 600

unit lead
  module f16
  team friendly
  spawn 46.73800 6.72200 2500 48.0 300       # Luftstart, 2500 m ASL, Kurs 048, 300 kt
  set gear up
  set fuel_pct 60
  wp 46.84335 6.91523 2900 320               # EIGENE Wegpunkte
  wp 46.95000 7.05000 3200 320

unit two
  module f16
  team friendly
  spawn 46.72600 6.73900 2500 48.0 300       # ~1,5 km südöstlich von lead
  set gear up
  set fuel_pct 60
  wp 46.81000 6.95000 2100 300               # eigene, tiefere Route
  wp 46.90000 7.12000 2400 300
```

| Geltung | Keyword | Felder | Bedeutung |
|---|---|---|---|
| Mission | `name`    | Rest der Zeile | Missionsname (Telemetrie/Logs) |
| Mission | `runway`  | lat lon elevM trueHdgDeg lengthM | Optionale Landing-Geometrie (`FBRunway`) — nötig für `spawn threshold`, `land`, und `FBMissionMonitor`s Off-Runway-Touchdown-FAIL-Prüfung; eine reine Luftstart-Mission ohne Landeabsicht braucht keine `runway`-Zeile. Missionsweit: alle Einheiten teilen sie. Breite ungenutzt (0, Default-Fallback im Monitor). |
| Mission | `timeout` | Sekunden (>0) | Sim-Zeit bis TIMEOUT, falls die Mission nicht vorher endet. Gilt für jede Einheit. |
| Akteur  | `unit`    | callsign | Blockbeginn. 1–24 Zeichen aus `[A-Za-z0-9_-]` (das Callsign benennt auch die Telemetriedatei und die `unit=`-Log-Attribution), missionsweit eindeutig. |
| Akteur  | `module`  | Rest der Zeile | Modulname, per `FBModuleRegistry` aufgelöst (heute nur `f16` registriert) — bestimmt sowohl das `FBModule` als auch den JSBSim-Aircraft-Ordnernamen (`vendor/jsbsim/aircraft/<module>`). Pflicht je Block. |
| Akteur  | `team`    | `friendly`\|`hostile`\|`neutral` | Fraktion (`FBUnitTeam`, `core/FBTeam.h`) — landet in der `FBWorld`-Unit-Registry, die Sensoren/Waffen künftig lesen. Optional, Default `friendly`. |
| Akteur  | `spawn`   | `<lat lon \| threshold>` `<altM \| ground>` `hdgDeg` `speedKt` | Pflicht, genau einmal je Block: die deklarative IC dieser Einheit — Position, Höhe-ODER-Boden, Kurs, Speed. `threshold` übernimmt lat/lon der missionsweiten `runway`-Zeile (reine Schreib-Convenience, keine zweite Positions-Syntax). `ground` löst die Höhe aus Gelände + Fahrwerksgeometrie auf; ein numerischer Wert ist eine LITERALE ASL-Höhe (ein Luftstart). Beide Fälle durchlaufen dieselbe eine JSBSim-IC-Anwendung. |
| Akteur  | `set`     | `key value...` | Systemzustand als Missionsdaten — der Runner parst nur die KV-Liste und reicht sie im Spawn-IC-Fenster an `FBModule::ApplySetup(key, value)` DIESER Einheit; das MODUL interpretiert seine eigenen Schlüssel. Ein unbekannter Schlüssel ist ein Laufzeit-FAIL (Exit 1, `SET_REJECTED`-Event), kein Parse-Fehler. F-16 kennt heute: `gear` (`up`/`down`), `fuel_lbs` (absolute Tankmenge, lb), `fuel_pct` (0..100, Anteil der modelleigenen Gesamtkapazität), die vier Schalter des MIDS-Terminals — `datalink` (`on`/`off`, Geräte-Strom), `datalink_xmt` (`on`/`off`, XMT/EMCON), `datalink_filter` (`fr`/`fl`/`off`, HSD-Kontaktfilter), `datalink_range_nm` (Terminal-Reichweite, nm) — sowie FCR/IFF: `fcr_mode` (`off`/`crm`/`acm_hud`/`acm_bore`/`acm_vert`/`acm_slew`), `fcr_range_nm` (überschreibt die Reichweite JEDES Modus), `fcr_slew_az`/`fcr_slew_el` (Cursor der Slewable-Box, Grad), `iff_xpdr` (`on`/`off`, eigener Transponder), `iff_interrogator` (`on`/`off`, eigener Abfrager). |
| Akteur  | `wp`      | lat lon altM speedKt | `FBWaypoint` vom Typ `Enroute`, im Flugplan DIESER Einheit |
| Akteur  | `land`    | — | `FBWaypoint` vom Typ `Land` AN der Runway-Schwelle (braucht die missionsweite `runway`-Zeile) |

`name`, `timeout` und mindestens ein `unit`-Block sind Pflicht; je Block sind `module` und `spawn`
Pflicht. `runway` ist optional (nur nötig für `spawn threshold`/`land`/die Off-Runway-Prüfung). Parse-
Fehler (`FBParseMissionFile` liefert `false` + eine Meldung in `*err`) sind: unbekanntes Keyword, eine
Akteurszeile ohne offenen Block, eine missionsweite Zeile nach dem ersten Block, ein doppeltes oder
nicht dateisicheres Callsign, zwei `spawn`-Zeilen in einem Block, `spawn threshold`/`land` ohne
`runway`, ein `set` ohne Wert, ein `team` außerhalb der drei Werte, ein fehlendes Pflichtfeld. Ein
`module`, das `FBModuleRegistry` nicht kennt, oder ein unbekannter `set`-Schlüssel ist ein Laufzeit-FAIL
des Runners (nicht des Parsers).

**Konsistenz-Validierung beim Aufsetzen:** eine physikalisch widersprüchliche Deklaration ist ein FAIL,
sobald der Runner die Elevation aufgelöst hat (nicht schon im Parser, der keine Geodaten kennt) — heute:
eine explizite `spawn`-Höhe unterhalb des aufgelösten Bodens. `v=0` in der Luft ist dagegen KEIN Fehler
(legal, die Unit fällt dann eben — `FBFlightMonitor` urteilt darüber wie über jeden anderen Flugzustand).

## Datenmodell

`FBMission` = `Name` + optionale `FBRunway` (`HaveRunway`) + `TimeoutS` + `Units` (Liste von
`FBMissionUnit`). `FBMissionUnit` = `Id` (Callsign) + `ModuleName` + `Team` + `FBSpawn` (`HaveSpawn`) +
`SetKV` (Dateireihenfolge) + `FBFlightPlan` (die `wp`/`land`-Zeilen in Dateireihenfolge). Ein
Landeziel ist kein eigenes Flag: der Flugplan endet dann auf einem `FBWaypointType::Land`-Wegpunkt,
und genau daran erkennt der Monitor die Stillstand-Regel (unten).

Der Orchestrator (`FBMissionRunner.cpp`, geteilt von `fb-gym` und `gpu_native --mission`) spawnt je
Block EINEN Akteur (`FBMissionBoot.h::FBMissionSpawnActor`: `ModuleName` über
`FBModuleRegistry::Create` in ein `std::unique_ptr<FBModule>`, eine IC, Plan/Runway/`set` darauf) und
hält von da an ALLES nur über die generischen `FBModule`-Accessoren (`Autopilot()`/`FlightControl()`/
`PilotSystem()`/`Controls()`/`Displays()`/`AirDataSystem()`/`FlightPlan()`/`Telemetry()`/
`ApplySetup()`) — der Runner nennt nie einen konkreten Modultyp und enthält keinen missions-spezifischen
Code (kein Runway-Schwellen-Spawn, kein Wegpunkt-Advance — beides sitzt im Boot bzw. in
`systems/FBNavSystem::AdvanceWaypoint`, dem Modul selbst). Die Bodenhöhe für einen `ground`-Spawn kommt
aus dem injizierten `FBElevationProvider` (`--elev tiles|const|swiss`) — die Datei-Elevation der
`runway`-Zeile ist nur Doku/Fallback.

## Tick-Reihenfolge und Snapshot-Regel

Alle Akteure laufen in EINEM Thread, in **Dateireihenfolge** (Index 0 = primärer Akteur: seine
Telemetrie behält den kanonischen Dateinamen, seine Augen sind die Kamera). Pro Tick:

1. je Akteur Bodenhöhe auflösen,
2. je Akteur `Run()` (Modul → Guidance → FLCS → JSBSim-Substeps),
3. **Barriere**: je Akteur `PublishPose()`,
4. je Akteur beide Monitore füttern,
5. je Akteur Telemetrie sampeln.

Schritt 3 ist die **Snapshot-Disziplin**: `FBUnit::GetPose()` — das, was die `FBWorld`-Unit-Registry
anderen Einheiten (künftig Sensoren/Waffen) zeigt — liefert IMMER die Pose des zuletzt ABGESCHLOSSENEN
Ticks, nie eine halb integrierte. Damit kann die Tick-REIHENFOLGE das Ergebnis nicht beeinflussen; die
geplante Parallelisierung (ein Thread je Unit, Lockstep-Barriere) ist dann eine reine Parallelisierung,
kein Umbau. Der WASM-Frame-Loop fährt dieselbe Reihenfolge.

## Urteil — je Einheit, dann kombiniert

Zwei unbestechliche Instanzen je Akteur, nie vom Modul gesehen (CLAUDE.md „Kein Cheaten"):

- `core/FBFlightMonitor` — das physikalische K.O. (Absturz/LOC). **Das K.O. IRGENDEINER Einheit beendet
  den Lauf** (ein departender Airframe soll nicht im Hintergrund weiterintegrieren); RESULT nennt die
  Einheit (`unit=`), Exit 2.
- `core/FBMissionMonitor` — das MISSIONS-Urteil, **eine Instanz je Einheit MIT Zielen** (nicht-leerer
  Flugplan; eine Einheit ohne Wegpunkte hat nichts zu erreichen und bekommt keinen Monitor, taucht also
  im Urteil nicht auf). Er trägt seine EIGENE Kopie von `FBFlightPlan`/`FBRunway` aus der Missionsdatei
  (nie das Modul-eigene, live mutierte Exemplar) und liest Fortschritt nur aus beobachteter Position —
  ein Modul kann sich nicht per Selbstauskunft zu SUCCESS melden.

Kombinationsregel (die EINZIGE Stelle, an der aus N Urteilen eins wird — `FBMissionRunner.cpp`):

| Gesamturteil | Bedingung | Exit |
|---|---|---|
| CRASH / LOC | eine Einheit hat ein physikalisches K.O. | 2 |
| FAIL    | eine Einheit mit Zielen ist gescheitert (Touchdown abseits der Runway) | 1 |
| TIMEOUT | eine Einheit mit Zielen hat ihre Ziele bis zum Timeout nicht erreicht | 3 |
| SUCCESS | **ALLE** Einheiten mit Zielen haben ihre Ziele erreicht | 0 |

Der Lauf endet beim ERSTEN Scheitern (es gibt nichts mehr zu beweisen) und sonst, sobald jede Einheit
mit Zielen ihr eigenes SUCCESS erreicht hat.

Endet der Flugplan einer Einheit auf einer `land`-Zeile (`FBWaypointType::Land`, immer die
Runway-Schwelle), gilt für DIESEN letzten Wegpunkt eine andere SUCCESS-Regel als für `wp`: kein
einfaches Capture-und-weiter, sondern **Stillstand auf der zugewiesenen Runway** — Fahrwerk mit
Bodenkontakt, Groundspeed unter ~2 kt, Position innerhalb des Runway-Footprints (0 m Längs-, 15 m
Quer-Marge). Ein bloßes Überfliegen der Schwelle in Fluggeschwindigkeit ist noch keine Landung.
`systems/FBPilot`s Phasenmaschine liefert dazu die Flugführung — `Approach` (`FBAutopilot::Course`,
`doc/f16/navigation-ils.md`) → `Flare` → `Rollout` —, aber das URTEIL bleibt beim Monitor.

## Datalink — was eine Einheit von den anderen sieht

Eine Einheit nimmt andere Einheiten AUSSCHLIESSLICH über simulierte Systeme wahr (CLAUDE.md „Kein
Cheaten"). Heute sind das genau ZWEI: das kooperative Netz `systems/FBDatalinkSystem` (MIDS/Link-16,
`doc/f16/datalink-iff.md`, F-16-Ableitung `modules/f16/FBF16Datalink`) und das aktive Radar
`systems/FBRadarSystem` (unten). Beide lesen die Einheiten-Registry (`units/FBUnitRegistry` — die
Snapshots des zuletzt abgeschlossenen Ticks) und schreiben daraus Kontakte in `FBState`; der Pilot liest
nur FBState, nie die Registry.

Regeln, die eine Mission beim Bauen kennen muss:

- **nur die eigene Fraktion** (`team`) — ein kooperatives Netz kennt keine Gegner. Feindaufklärung ist
  Radar, nicht das hier.
- **der ABSENDER muss senden.** `set datalink off` schaltet das Terminal aus: die Einheit ist blind UND
  stumm. `set datalink_xmt off` ist EMCON: sie EMPFÄNGT weiter das ganze Bild, wird aber von niemandem
  mehr geführt (der Regelfall im Übungsluftkampf).
- **Reichweite** = min(Terminal-Reichweite, Funkhorizont beider Höhen ≈ 1,23·(√h₁[ft]+√h₂[ft]) nm).
  F-16-Default 300 nm (MIDS-LVT/Link-16 LOS); `set datalink_range_nm` konfiguriert sie (eine Mission,
  die den Reichweitenverlust zeigen will, setzt sie klein — `missions/payerne-pair-datalink.fbm`).
- **1-Hz-Netzzyklus, kein Live-Bild.** Zwischen zwei Zyklen steht die gemeldete Position still und ihr
  ALTER läuft hoch (0..1 s); ein nicht mehr empfangener Track wird 3 Zyklen gehalten und dann gelöscht.
- **Kontaktfilter** (F-16, HSD): `fr` alle Freundlichen (Default), `fl` nur Flight Leads (mangels
  echter Verbandsstruktur: die ERSTE `unit` dieser Fraktion in der Datei), `off` keine.

Beobachtbar ist der Datalink in beiden Kanälen: `events.log` trägt `datalink TRACK_GAINED` /
`TRACK_LOST` (diskrete Ereignisse), `telemetry.csv` die fünf Spalten `dl_on`, `dl_xmt`, `dl_tracks`,
`dl_near` (nm zum nächsten Track), `dl_age` (s seit dessen Meldung).

## Radar (FCR) & IFF — was eine Einheit selbst FINDET

Das aktive Gegenstück zum Datalink: `systems/FBRadarSystem` (generischer Default), F-16-Ableitung
`modules/f16/FBF16Fcr` (AN/APG-68, `doc/f16/radar-sensors.md`). Der Datalink bekommt Identität geschenkt,
das Radar bekommt ein Echo — die Regeln, die eine Mission kennen muss:

- **Scanvolumen statt Reichweite.** Ein Kontakt entsteht nur, wenn das Ziel im Volumen des aktiven Modus
  liegt: Azimut × Elevation RELATIV ZUR NASE (das Volumen rollt und nickt mit dem Jet) plus ein
  Entfernungstor. Modus-Tabelle (`set fcr_mode`):

  | Modus | Azimut | Elevation | Reichweite | Frame | Auto-Lock |
  |---|---|---|---|---|---|
  | `off` | — | — | — | — | — (strahlt nicht) |
  | `crm` (Default, Power-up) | ±60° | ±10,5° | 40 nm | 4,0 s | nein |
  | `acm_hud` (30×20-Box) | ±15° | ±10° | 10 nm | 1,0 s | ja |
  | `acm_bore` (~10°-Kegel) | ±5° | ±5° | 10 nm | 0,3 s | ja |
  | `acm_vert` (schmal-hoch) | ±5° | −13°…+47° | 10 nm | 1,2 s | ja |
  | `acm_slew` (20×20, `fcr_slew_*`) | ±10° um Cursor | ±10° um Cursor | 10 nm | 0,8 s | ja |
  | (gelockt = STT) | ±60° | ±60° | 40 nm | 0,1 s | Single-Target |

- **Kontakte entstehen und vergehen in ZEIT.** Zwei aufeinanderfolgende Looks (`kHitsToFirm`) machen aus
  einem Echo einen Track — die Aufbauzeit ist also ein Frame lang. Ein Track, der nicht mehr gesehen
  wird, wird `max(1 s, 3 Frames)` lang gehalten (Coast, gefrorene Geometrie, `fcr_lock_age` läuft hoch)
  und dann gelöscht.
- **ACM lockt selbst.** Jeder ACM-Sub-Modus lockt den NÄCHSTEN festen Track ohne Bedienung — das ist der
  Zweck der Modi. Der Lock ist STT: die Antenne verlässt die kleine Box, folgt dem Ziel bis an die
  Gimbal-Grenze (±60°) und sieht dabei NUR noch dieses eine Ziel (alle anderen Trackfiles laufen aus).
- **Ein Kontakt ist ANONYM.** `FBRadarContact` (core/) trägt Range/Bearing/Az/El/Closure/Track-Nummer —
  keine Id, kein Callsign, kein Team. Das ist die Fidelity-Grenze und zugleich der Anti-Cheat-Punkt.
- **IFF ist die EINZIGE Identitätsquelle** (Mode 4, `doc/f16/datalink-iff.md`): gültiger Reply =
  `friendly`; KEIN Reply = **unbekannt**, niemals „hostile". Ein Reply ist nur gültig, wenn der
  Transponder des Ziels an ist UND das Ziel derselben Fraktion angehört (Krypto). Ein Feind mit
  eingeschaltetem Transponder und ein Freund mit ausgeschaltetem liefern dasselbe Ergebnis: unbekannt.
  Schalter: `set iff_xpdr` (was ANDERE von mir zurückbekommen, im Emissions-Snapshot der Unit) und
  `set iff_interrogator` (ob ich selbst frage).
- **Terrain-Maskierung ist NICHT modelliert** (bewusst, im Header dokumentiert): für Luft-Luft zwischen
  zwei fliegenden Einheiten entscheiden Volumen und Reichweite; Maskierung braucht einen DEM-Raymarch je
  Kontakt und je Look.

Beobachtbar in beiden Kanälen: `events.log` trägt `radar RADAR_CONTACT` / `RADAR_DROP` (Track-Aufbau und
-Löschung), `radar RADAR_LOCK` / `RADAR_LOST` (STT) und `radar IFF_REPLY`; `telemetry.csv` die elf
Spalten `fcr_on`, `fcr_mode` (Ordinalzahl der Tabelle oben), `fcr_contacts`, `fcr_lock`
(Track-Nummer, 0 = kein Lock), `fcr_lock_nm`, `fcr_lock_az`, `fcr_lock_el`, `fcr_lock_clos` (kt, + =
annähernd), `fcr_lock_age` (s seit dem letzten Look — > 0 heißt Coast), `fcr_iff` (0 = nicht abgefragt,
1 = unbekannt, 2 = friendly), `iff_xpdr`.

**Keine HUD-Symbologie:** `doc/f16/hud-symbology.md` dokumentiert weder eine Target-Designator-Box noch
ein Locked-Target-Symbol (der radarnahe Eintrag, HMC, ist ein Markpoint-Cursor). Der Lock bleibt deshalb
in FBState/Telemetrie/Events, bis die Symbologie-Referenz ihn abdeckt — erfunden wird nichts.

## Ausgabe je Lauf (`--out DIR`)

- `telemetry.csv` — der primäre Akteur (Index 0). Kanonischer Name, unverändert.
- `telemetry_<callsign>.csv` — jede weitere Einheit, gleiches Schema. Eine Datei je Unit statt einer
  breiten Zeile: die Spalten folgen dem MODUL der Einheit, eine geteilte Zeile müsste entweder alle
  Module in ein Schema zwingen oder den Header von der Besetzung abhängig machen.
- `events.log` — `t=SEK LEVEL tag EVENT key=val …`, greppbar.

**Unit-Attribution im Log:** hat eine Mission MEHR ALS EINE Einheit, trägt jede Zeile, die zu einem
Akteur gehört, als erstes Feld `unit=<callsign>` (`core/FBLog.h`s `FBLogUnitScope`) — auch die
modulinternen (`nav`, `pilot`) und die des Monitors. Bei genau einer Einheit entfällt das Feld: die
Zeilen der Mission SIND die dieser Einheit, es gibt nichts zu unterscheiden (und ältere
Regressions-Baselines bleiben Byte-identisch).

**Teilergebnisse:** vor der kombinierten `RESULT`-Zeile emittiert der Runner bei mehr als einer Einheit
je Akteur eine maschinenlesbare `UNIT_RESULT`-Zeile:

```
t=222.1 INFO mission UNIT_RESULT unit=lead result=SUCCESS reason="all waypoints reached" team=friendly \
    decisive=0 lat=46.9683 lon=7.05104 altM=3211.24 telemetry=out/telemetry.csv
```

`result` ist `SUCCESS|FAIL|TIMEOUT|CRASH|LOC|NONE` (NONE = Einheit ohne Ziele), `decisive=1` markiert
die Einheit, deren Urteil den Lauf BEENDET hat (bei SUCCESS keine). Die abschließenden
`RESULT`/`SUMMARY`-Zeilen tragen dieselbe `unit=`-Attribution wie diese Einheit.

## Beispiele

- `sim/missions/payerne-takeoff-only.fbm`, `sim/missions/payerne-takeoff.fbm` — Payerne (LSMP) Runway
  23, Bodenstart; Schwellen-Koordinaten und -Länge gegen `fb-tiles`' `/elev`-Endpunkt geprüft (DEM
  ~441 m an der Schwelle).
- `sim/missions/payerne-pair-datalink.fbm` — zwei Freundliche, die auseinanderfliegen, Terminals auf
  6 nm konfiguriert: zeigt Latenz (`dl_age`), Netzzyklus und den Reichweitenverlust (`TRACK_LOST`).
- `sim/missions/payerne-flight-datalink.fbm` — fünf Freundliche in Reichweite, jede mit anderer
  Terminal-Konfiguration: die Schalter-Matrix (POWER/XMT/Filter) in einem Lauf.
- `sim/missions/payerne-airstart.fbm` — derselbe Luftraum, aber ein reiner Luftstart (~10 nm SW von
  Payerne, 2500 m ASL, 300 kt, Fahrwerk eingefahren, 60% Tankfüllung) — keine `runway`-Zeile, kein
  Taxi/Rollout.
- `sim/missions/payerne-landing.fbm` — das Landetrainingsgelände: Luftstart ~9 nm auf RWY23-Endanflug
  ausgerichtet, ~500 m AGL, dann `land` — SUCCESS = Stillstand auf der Runway.
- `sim/missions/payerne-full.fbm` — die Ziel-Mission: Bodenstart Payerne → drei Wegpunkte → `land` auf
  derselben Runway wie der Start.
- `sim/missions/payerne-pair.fbm` — **zwei** befreundete F-16 im Luftstart nebeneinander, jede mit
  eigenen Wegpunkten; SUCCESS erst, wenn BEIDE ihre Ziele erreicht haben.
- `sim/missions/payerne-pair-fail.fbm` — dieselbe Paarung mit einem für `two` unerreichbaren Wegpunkt
  bei knappem Timeout: `lead` erreicht seine Ziele, das Gesamturteil ist trotzdem negativ und nennt
  `two` als die Einheit, die es entschieden hat.
- `sim/missions/payerne-four.fbm` — eine Viererrotte im Luftstart, jede Einheit in ihrem eigenen
  Höhenblock: der Skalierungsfall für `fb-gym --threads` (vier annähernd gleich teure Airframes).
- `sim/missions/payerne-mixed.fbm` — bewusst ungleiche Last: `roller` startet am Boden auf der Schwelle,
  `cruiser` ist bereits im Reiseflug — der Stresstest der Lockstep-Barriere.
- `sim/missions/payerne-radar-acm.fbm` — die Radar-Referenzgeometrie: zwei Jets auf senkrechten,
  geraden, gleich hohen Beinen. Das Ziel startet außerhalb jeder ACM-Box, schwenkt durch die Nase und
  verlässt schließlich die Antennenreichweite — kein Kontakt → Aufbau → Lock → Coast → Verlust in EINEM
  Lauf.
- `sim/missions/payerne-radar-bore.fbm` — dieselbe Datei mit EINEM geänderten Wort (`acm_bore` statt
  `acm_hud`): gleiche Geometrie, engeres Volumen, messbar spätere Erfassung.
- `sim/missions/payerne-radar-iff.fbm` — ein Abfrager, drei nacheinander die Nase kreuzende Ziele:
  Freund mit Transponder (friendly), Freund ohne (unbekannt), Feind MIT Transponder (unbekannt — nicht
  hostile).
