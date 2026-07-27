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
- **akteursbezogen** — `module`, `team`, `spawn`, `set`, `wp`, `land`, `objective`. Nur INNERHALB
  eines `unit`-Blocks; ein Block läuft bis zum nächsten `unit` oder Dateiende.

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
| Akteur  | `set`     | `key value...` | Systemzustand als Missionsdaten — der Runner parst nur die KV-Liste und reicht sie im Spawn-IC-Fenster an `FBModule::ApplySetup(key, value)` DIESER Einheit; das MODUL interpretiert seine eigenen Schlüssel. Ein unbekannter Schlüssel ist ein Laufzeit-FAIL (Exit 1, `SET_REJECTED`-Event), kein Parse-Fehler. F-16 kennt heute: `gear` (`up`/`down`), `fuel_lbs` (absolute Tankmenge, lb), `fuel_pct` (0..100, Anteil der modelleigenen Gesamtkapazität), die vier Schalter des MIDS-Terminals — `datalink` (`on`/`off`, Geräte-Strom), `datalink_xmt` (`on`/`off`, XMT/EMCON), `datalink_filter` (`fr`/`fl`/`off`, HSD-Kontaktfilter), `datalink_range_nm` (Terminal-Reichweite, nm) — sowie FCR/IFF: `fcr_mode` (`off`/`crm`/`acm_hud`/`acm_bore`/`acm_vert`/`acm_slew`), `fcr_range_nm` (überschreibt die Reichweite JEDES Modus), `fcr_slew_az`/`fcr_slew_el` (Cursor der Slewable-Box, Grad), `iff_xpdr` (`on`/`off`, eigener Transponder), `iff_interrogator` (`on`/`off`, eigener Abfrager); die Defensivanlage — `rwr` (`on`/`off`, Strom des Warnempfängers), `rwr_display` (`priority`/`open`, TWP-MODE-Anzeigedeckel), `rwr_search` (`on`/`off`, SEARCH-Filter), `cmds_mode` (`off`/`stby`/`man`/`semi`/`auto`/`byp`), `cmds_program` (1..6, PRGM-Knopf), `cmds_chaff`/`cmds_flare` (Vorrat je Typ, zusammen ≤ 120); dazu `task` (`route`/`bfm`/`intercept`) — die FBPilot-Phase, in der diese Einheit startet (Default = das, was der Spawn vorgibt: `route` in der Luft, `preflight` am Boden); die Bordkanone `gun_rounds <n>` (Trommelinhalt beim Start, 0..510 — mehr als die Kapazität ist ein FAIL); sowie die Zuladung `store <station> <typ>` (eine Zeile je Pylon, F-16-Stationen 1..9, Typ aus dem Katalog `core/FBStore.h` — heute `mk82`) und `brief_release_s <t>` (wiederholbar: wann der Pilot pickelt, Sim-Sekunden) sowie `brief_chaff_s <t>` (wiederholbar: wann er Täuschkörper wirft); schließlich die PILOTEN-VARIANTE `pilot_*` (s. „Piloten-Varianten" unten). |
| Akteur  | `wp`      | lat lon altM speedKt | `FBWaypoint` vom Typ `Enroute`, im Flugplan DIESER Einheit |
| Akteur  | `land`    | — | `FBWaypoint` vom Typ `Land` AN der Runway-Schwelle (braucht die missionsweite `runway`-Zeile) |
| Akteur  | `objective` | `survive` \| `waypoints` \| `kill unit <callsign>` \| `kill team <fraktion>` | KAMPFZIEL dieser Einheit (`core/FBObjective.h`) — wiederholbar, s. „Kampfziele" unten. `kill unit` muss eine Einheit DIESER Mission nennen (Vorwärtsreferenz erlaubt, geprüft am Dateiende) und nicht die eigene; `objective waypoints` braucht `wp`/`land`-Zeilen darüber. Ein doppelt deklariertes Ziel ist ein Parse-Fehler. |

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
`SetKV` (Dateireihenfolge) + `FBFlightPlan` (die `wp`/`land`-Zeilen in Dateireihenfolge) +
`Objectives` (die `objective`-Zeilen, `std::vector<FBObjective>`). Ein
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
  Flugplan ODER mindestens eine `objective`-Zeile; eine Einheit ohne beides hat nichts zu erreichen und
  bekommt keinen Monitor, taucht also im Urteil nicht auf). Er trägt seine EIGENE Kopie von
  `FBFlightPlan`/`FBRunway`/den `objective`-Zeilen aus der Missionsdatei
  (nie das Modul-eigene, live mutierte Exemplar) und liest Fortschritt nur aus beobachteter Position
  bzw. aus einem beobachteten Roster (unten) — ein Modul kann sich nicht per Selbstauskunft zu SUCCESS
  melden und keine Auskunft über den Gegner geben.

Kombinationsregel (die EINZIGE Stelle, an der aus N Urteilen eins wird — `FBMissionRunner.cpp`):

| Gesamturteil | Bedingung | Exit |
|---|---|---|
| CRASH / LOC | eine Einheit hat ein physikalisches K.O., **das nicht das erklärte Ziel einer anderen war** | 2 |
| FAIL    | eine Einheit mit Zielen ist **entscheidend** gescheitert (Touchdown abseits der Runway ODER kampfunfähig geschossen, ohne dass das jemandes Kampfziel war) | 1 |
| TIMEOUT | eine Einheit mit Zielen hat ihre Ziele bis zum Timeout nicht erreicht | 3 |
| SUCCESS | jede Einheit mit Zielen, deren Verlust nicht erklärtes Ziel einer anderen war, hat ihre Ziele erreicht | 0 |

Der Lauf endet beim ersten ENTSCHEIDENDEN Scheitern (es gibt nichts mehr zu beweisen), beim ersten
physikalischen K.O. (kein Wrack integriert im Hintergrund weiter) und sonst, sobald jede Einheit mit
Zielen ein Urteil hat.

## Kampfziele (`objective`)

Ohne `objective`-Zeile ist der **Flugplan das ganze Urteil** — die ursprüngliche Regel, unverändert.
Eine `objective`-Zeile macht das Ziel dieser Einheit explizit, und dann ist der Block die **vollständige
Aussage**: der Flugplan wird nur noch bewertet, wenn er als `objective waypoints` mit drinsteht. Das ist
kein Detail, sondern der Grund, warum ein Abfang seine gebriefte Vektor-`wp`-Zeile behalten darf, ohne
dass ein entschiedenes Gefecht am nie erreichten Vektorpunkt in einen TIMEOUT läuft.

| Ziel | Erfüllt, wenn | Verletzt, wenn |
|---|---|---|
| `survive` | am Ende des Laufs noch kampffähig — **nie vorher**, s.u. | die Einheit wird kampfunfähig geschossen → sofort FAIL |
| `waypoints` | der eigene Flugplan ist abgearbeitet (bei `land`: Stillstand auf der Runway) | — |
| `kill unit <callsign>` | die benannte Einheit ist kampfunfähig (`core/FBSystemHealth::CombatEffective`) | — |
| `kill team <fraktion>` | JEDE Einheit dieser Fraktion in der Mission ist es (mindestens eine muss existieren) | — |

SUCCESS heißt: alle Ziele erfüllt. **`survive` kann nicht früh erfüllt werden** — „noch kampffähig" ist
erst wahr, wenn kein Lauf mehr übrig ist, in dem man abgeschossen werden könnte (eine Rakete des
Gegners kann noch in der Luft sein, nachdem er selbst gestorben ist). Eine Einheit mit `survive` bleibt
deshalb bis zum Laufende ohne Urteil und wird dann ausgewertet: Ziele erfüllt und noch kampffähig →
SUCCESS („objectives met, survived"), sonst TIMEOUT. Eine Einheit MIT `kill`, aber OHNE `survive`,
erklärt damit ausdrücklich, dass ihr eigener Verlust kein Scheitern ist — ein gleichzeitiger Abschuss
beider Seiten ist dann ein Tausch und nicht das Scheitern beider.

Die Beobachtung, gegen die ein `kill`-Ziel geprüft wird, ist ein **Roster**: je Nicht-Waffen-Akteur
Callsign, Fraktion und das eine Bit, das sein eigenes Gesundheitsregister veröffentlicht
(`FBUnitObservation`, `core/FBObjective.h`). Der Runner baut ihn einmal pro Tick aus den Registern, die
ER besitzt, und zeigt ihn jedem Monitor — kein Modul wird nach seinem Gegner gefragt und keines nach
sich selbst.

### Zwei Fraktionen mit gegensätzlichen Zielen — ein Duell hat einen Sieger

Die Regel, die aus zwei entgegengesetzten Urteilen eines macht, ist **eine einzige und sie ist
deklarationsbasiert**, nicht team- oder „Spielerseite"-basiert:

> Der Verlust einer Einheit ist **ERWARTET**, wenn er das erklärte Ziel einer anderen war — die Einheit
> ist kampfunfähig UND eine andere Einheit hat ein `kill`-Ziel deklariert, das sie (oder ihre Fraktion)
> nennt. Ein erwarteter Verlust wird weiterhin als das FAIL DIESER EINHEIT gemeldet (`UNIT_RESULT`),
> **entscheidet aber den Lauf nicht** — weder als Missions-FAIL noch, wenn das Wrack später aufschlägt,
> als CRASH. Das Gesamturteil kommt dann von den übrigen Einheiten.

Damit hat ein Duell einen Sieger (SUCCESS) und einen Verlierer (FAIL) statt zweimal FAIL. Schlagen sich
beide Seiten gegenseitig ab (ein Tausch), ist kein Urteil entscheidend und der Lauf meldet das FAIL der
ersten Einheit — niemand ist heimgekommen, und die Zeile sagt das, statt einen Sieger zu erfinden.
Missionen ohne `objective`-Zeilen kennen keinen erwarteten Verlust, kombinieren also exakt wie zuvor
(nachgemessen: 132 von 132 Ausgabedateien der Bestandsmissionen byte-identisch).

Endet ein Lauf durch ein ERWARTETES physikalisches K.O., bekommen alle noch offenen Monitore an dieser
Stelle ihr Urteil (dieselbe Auswertung wie beim Timeout) — sonst hätte der Schütze mit `survive`-Ziel
nie eines bekommen.

**Abschuss als Missions-Urteil.** Verliert eine Einheit durch einen Waffentreffer ihr Triebwerk, ihre
Flugsteuerung oder ihre Struktur (`core/FBSystemHealth::CombatEffective`, s. „Schadensmodell" in
CLAUDE.md), schließt ihr eigener `FBMissionMonitor` mit FAIL — „combat ineffective (weapon damage)"
bzw. „…(survive objective lost)", je nachdem ob sie Ziele deklariert hat.
Das ist ausdrücklich ein MISSIONS-Urteil und kein physikalisches: die Einheit wird nicht eingefroren und
nicht markiert, sie fliegt weiter, solange die Physik es hergibt, und der Physik-Monitor urteilt danach
wie über jedes andere Flugzeug (meist CFIT, wenn das Wrack den Boden erreicht). In der `UNIT_RESULT`-
Zeile hat für eine abgeschossene Einheit das MISSIONS-Urteil Vorrang vor dem späteren CRASH: der
Abschuss erklärt den Aufschlag, der Aufschlag erklärt nichts. Ein unbeschädigtes Wrack (CFIT, Departure)
meldet weiterhin CRASH/LOC.
Konsequenzen für den Missionsentwurf:
- Eine Einheit MIT Zielen beendet den Lauf, sobald sie abgeschossen wird — es sei denn, ihr Abschuss war
  das erklärte Ziel einer anderen; dann läuft die Mission weiter, bis der Schütze sein eigenes Urteil
  hat (`bvr-duel-decided.fbm`).
- Eine Einheit OHNE Ziele trägt gar keinen `FBMissionMonitor` und kann den Lauf nicht beenden; ihr
  Abschuss ist dann beobachtbar bis zum Aufschlag (`damage-amraam.fbm`, Exit 2 = CRASH — diese Mission
  deklariert bewusst KEIN Kampfziel, weil ihr Gegenstand die 340 s Nachspiel nach dem Treffer sind).

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
- **CRM lockt NIE von selbst — der Pilot designiert.** Der Kommandobus trägt dafür `Designate` (TMS
  vorwärts): Wert = die veröffentlichte Track-NUMMER des Kontakts (also genau das anonyme Handle, das
  der Pilot vom Bus liest), Wert 0 = Lock lösen (TMS rückwärts). Eine Nummer ohne festen Track wird als
  `out_of_context` abgelehnt — der Rückstrahler war weg, bis die Hand fertig war. Ein DESIGNIERTER Lock,
  der verlorengeht, fällt zurück in die Suche und greift sich NICHT den nächsten Kontakt; das
  Auto-Reacquire gehört den ACM-Modi, die es verlangt haben. Events: `radar RADAR_DESIGNATE` bzw.
  `radar RADAR_BREAK` (getrennt von `RADAR_LOCK`, weil das eine eine Entscheidung ist und das andere
  ein Automatismus).
- **`fcr_slew_el` ist die Antennenhöhen-Bedienung, nicht nur der ACM-Cursor.** Auch CRMs
  Elevations-MITTE folgt ihr (±10,5° um die eingestellte Höhe): ein mechanisch abtastendes Radar deckt
  bei BVR-Entfernung nur ein paar tausend Fuß ab, und die Antenne auf das falsche Höhenband zu stellen
  ist die klassische Art, an einem Ziel vorbeizufliegen, das das Radar problemlos hätte sehen können.
  Default 0 → unverändertes Verhalten für jede Mission, die den Schlüssel nicht setzt.
- **Ein Kontakt ist ANONYM.** `FBRadarContact` (core/) trägt Range/Bearing/Elevationswinkel/Az/El/
  Closure/Track-Nummer — keine Id, kein Callsign, kein Team. Das ist die Fidelity-Grenze und zugleich der
  Anti-Cheat-Punkt. (Bearing + Elevationswinkel sind WELT-bezogen und stehen neben dem körperfesten
  Az/El-Paar: das Radar kennt seine eigene Lage im Moment des Looks, sein Konsument nicht — ohne das
  müsste jeder Verbraucher einen look-alten Körpervektor durch eine jetzt-aktuelle Fluglage zurückdrehen
  und würde die eigene Rollbewegung in die Zielgeometrie schmieren.)
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

## RWR & Gegenmaßnahmen — wer schaut MICH an, und was werfe ich

Die Gegenseite der Sensorik: `systems/FBRwrSystem` (F-16: `modules/f16/FBF16Rwr`, AN/ALR-56M) hört zu,
`systems/FBCountermeasureSystem` (F-16: `modules/f16/FBF16Cmds`, AN/ALE-47) antwortet. Quelle für beide:
`doc/f16/defence-rwr-cm.md`.

**Was ein Radar ABSTRAHLT, ist beobachtbarer Zustand** (`core/FBEmitter.h`), publiziert an derselben
Tick-Barriere wie Pose und Datalink-Schalter: Modus, Emitter-Art, das körperfeste Keulenfenster und das
Entfernungstor. Drei Signale, und der Unterschied ist der taktische Kern:

| Emission | Keule | Was sie bedeutet |
|---|---|---|
| **Suche** | das ganze Scanvolumen des Modus (der Strahl streicht einmal je Frame darüber) | jemand sucht — Information |
| **Verfolgung** | ±3° BLEISTIFT auf genau einen Kontakt (STT) | er hat MICH — Warnung |
| **Lenkung** | dieselbe Bleistiftkeule, während der Schütze eine Waffe stützt | eine Rakete fliegt — Alarm |
| **Raketensucher** | die eigene Keule des Flugkörpers (`modules/missile/FBMissileSeeker`) | der Endanflug — Alarm |

**Der RWR ist ein geometrisch begrenzter EMPFÄNGER, kein Bedrohungsorakel.** Er meldet, was er HÖRT:

- **Die Keule muss treffen.** Das Fenster ist am SENDER körperfest, wird mit dessen publizierter Lage
  gedreht und dann geprüft — dieselbe Transformation, die der Sender für seine eigene Erfassung benutzt.
  Ein suchendes Radar bestrahlt alles in seinem Volumen; ein verfolgendes GENAU EIN Flugzeug.
- **Die eigene Antenne muss hören können.** 360° Azimut, aber nur **±45° Elevation** (ALR-56M) — darüber
  und darunter liegt eine echte **Blindzone**, die das eigene Manövrieren aufreißt und die eine bereits
  bestehende Lock- oder Startwarnung STILL verschwinden lässt (`rwr THREAT_BLIND`).
- **Hörweite = Sender-Tor · 2** (`kBeamRangeFactor`): der Empfänger sitzt im EINWEG-Pfad, der Sender
  braucht Hin- und Rückweg. Man wird gewarnt, bevor man erfasst wird.
- **Keine Entfernung.** Ein RWR misst Empfangsleistung, nie Range (§2.1). Publiziert werden relative
  Peilung, Elevation, Signalstärke und eine Lethality (Ringposition), niemals Meter.

**CMDS: Programme als Daten, der Modus-Knopf als Zustandsmaschine.** Ein Programm ist das
DED-Parameterschema des ALE-47 (§2.2): je Typ Salvengröße (BQ 0–99), Salvenintervall (BI 0,020–10 s),
Salvenzahl (SQ 0–99), Programmintervall (SI 0,50–150 s); BQ oder SQ = 0 nimmt den Typ aus dem Programm.
Die sechs F-16-Programme (`modules/f16/FBF16Cmds`, Werte [SET], Schema dokumentiert):

| PRGM | Chaff | Fackeln | wofür |
|---|---|---|---|
| 1 BREAK LOCK | 2 × 0,10 s, 2 Salven à 1,00 s | — | die dichte Reflexantwort, und was AUTO gegen eine RAKETE wirft |
| 2 MIXED | 2 × 0,10 s, 2 Salven à 2,00 s | 1, 2 Salven | unbekannte Bedrohung |
| 3 FLARE | — | 2 × 0,10 s, 4 Salven | nur IR (gezählt, wirkungslos: es gibt keinen IR-Sucher) |
| 4 SUSTAINED | 2 × 0,10 s, 4 Salven à 4,00 s | — | gegen einen bloßen TRACK, und was AUTO dann wiederholt |
| 5 SLAP | 1 | 1 | der Wandknopf |
| 6 BYPASS | 1 | 1 | die dokumentierte Notausgabe |

Modi: `off`/`stby` (nichts; nur in STBY darf umprogrammiert werden) · `man` (CMS vorwärts wirft das
PRGM-Programm) · `semi` (das System WÄHLT, aber jeder Abwurf braucht Zustimmung) · `auto` (wählt UND
wiederholt, Zustimmung gilt ab Moduswechsel) · `byp` (genau 1 Chaff + 1 Fackel). Automatische Abwürfe
entfallen bei Chaff-BINGO. **In SEMI/AUTO triggert die Anlage auf den RWR-BLOCK** — also auf die
Warnung, nicht auf die Wahrheit: was in der Blindzone steht, wird nicht beantwortet.

**Die Wirkung — Doppler, nicht Würfel** (`systems/FBRadarSystem`, Modell-Entscheidung mit Herleitung):
eine Chaff-Wolke verliert binnen einer Sekunde die Geschwindigkeit des Flugzeugs und liegt danach im
Clutter-Filter, den ein Puls-Doppler-Sucher verwirft — **es sei denn**, das verfolgte Flugzeug liegt mit
seiner EIGENEN Radialgeschwindigkeit im selben Filter, was genau dann gilt, wenn es quer zur Sichtlinie
fliegt. Dann kann der Prozessor die beiden Echos nicht trennen und nimmt das stärkste (RCS/r⁴, mit der
Alterskurve der Wolke). Gesetzte Parameter: Notch-Halbbreite **40 m/s**, Messdauer **0,2 s**, Blüte
**0,3 s**, Wolkenlebensdauer **8 s**, RWR-Hörfaktor **2,0**. Alles deterministisch — kein Zufall, nirgends.

Beobachtbar: `events.log` trägt `rwr THREAT_NEW` / `THREAT_MODE` / `THREAT_BLIND` / `THREAT_DROP`,
`cmds PROGRAM_START` / `SALVO` / `PROGRAM_END` / `MAGAZINE_EMPTY` und `radar CHAFF_SEDUCED` /
`CHAFF_RESOLVED` (mit den beiden Messgrößen, aus denen die Entscheidung fiel); `telemetry.csv` am
Zeilenende die zehn RWR-Spalten `blk_rwr`, `rwr_on`, `rwr_threats`, `rwr_mode` (−1 = nichts, 0 = Suche,
1 = Verfolgung, 2 = Rakete), `rwr_brg`, `rwr_el`, `rwr_leth`, `rwr_new`, `rwr_launch`, `rwr_act` und die
elf CMDS-Spalten `blk_cmds`, `cm_mode`, `cm_status`, `cm_prog`, `cm_chaff`, `cm_flare`, `cm_lo`,
`cm_disp`, `cm_out_chaff`, `cm_out_flare`, `cm_clouds`.

Referenzläufe: `missions/rwr-spike.fbm` (Suche → Verfolgung → Raketensucher aus EINER Geometrie, und das
Verschwinden aller drei), `missions/rwr-blindzone.fbm` (der Ritt durch die ±45°-Grenze, während der
Sender nachweislich weiter lockt) und die 2×2-Tafel `cm-straight-clean` / `cm-chaff-straight` /
`cm-beam-only` / `cm-chaff-beam` (Chaff allein wirkungslos, Manöver allein fast wirkungslos, beides
zusammen entscheidend).

## Der Avionik-Bus — Gültigkeit, Kommandos, Brief

**Der geteilte Zustand ist ein Satz typisierter BLÖCKE**, nicht mehr eine flache Feldliste
(`core/FBState.h` + `core/FBAvionicsBlocks.h`). Jeder Block hat GENAU EINEN Schreiber (das Quellsystem)
und einen Kopf `{StampS, Status}` (`core/FBBlockStatus.h`) mit **drei** Zuständen — die Semantik eines
Multiplexbus-Jets, nicht seine Adressen:

| Status | Bedeutung | wer erzeugt ihn heute |
|---|---|---|
| `invalid` (0) | Die Zahlen bedeuten nichts: nie geschrieben, oder das Quellsystem ist aus/ausgefallen | `set radalt off` (CARA ohne Strom), Radar/Datalink abgeschaltet, Nav ohne Steerpoint |
| `valid` (1) | Vom eigenen Schreiber zum Zeitpunkt `StampS` aktualisiert | Normalbetrieb |
| `held` (2) | ABSICHTLICH eingefroren: letzte gute Werte, letzter Zeitstempel, keine neue Rechnung | Radarbild zwischen zwei Sweeps, Datalink zwischen zwei Netz-Zyklen, BFM-Schätzung jenseits ihres Extrapolationsfensters, CRUS-Rechenfelder (TTG) bei ausgefahrenem Fahrwerk (`doc/f16/controls-commands.md`) |

`held` ist keine Feinheit, sondern belegtes Verhalten: der echte Jet FRIERT mehrere Reiseflug-Rechenfelder
bei ausgefahrenem Fahrwerk EIN, statt sie ungültig zu machen. Konsumenten unterscheiden das: das HUD
zeichnet einen gehaltenen Wert weiter (er ist gültig, nur alt) und ersetzt einen ungültigen durch
Striche (`R----`, `B---.-`, `---:--`); `systems/FBWarningSystem` meldet eine Warnung, deren Quelle
ungültig ist, als **inhibited** statt als „keine Warnung"; `systems/FBPilot` handelt nicht auf einer
Höhe, die es nicht messen kann (kein Fahrwerk-Einfahren, keine Flare, kein BFM-Boden-Pull ohne gültigen
Radarhöhenmesser).

Jeder Block-Status steht in `telemetry.csv` als eigene Spalte (`blk_platform`, `blk_env`, `blk_airdata`,
`blk_radalt`, `blk_nav`, `blk_cruise`, `blk_firecontrol`, `blk_ufc`, `blk_stores`, `blk_airframe`,
`blk_warn`, `blk_radar`, `blk_datalink`, `blk_bfm` — und, ganz am Zeilenende statt in dieser Gruppe,
`blk_rwr` und `blk_cmds`: ihre Blöcke kamen dazu, als die Gruppe schon in jeder gemessenen
telemetry.csv stand, und ein Einfügen hätte jede Spalte rechts davon verschoben; Werte 0/1/2 wie oben) — plus `warn_active` und
`warn_inhibited` als Bitmasken (`1` = ALOW, `2` = BINGO, `4` = Fahrwerk unsicher).

**Kommandos: der EINZIGE Weg vom Piloten zu einer Box.** Ein Kommando ist `{Ziel, Vorschlagswert}`, die
Quittung `{Ergebnis, Grund}` — das dokumentierte DED-Muster Vorschlagen → Bestätigen/Verwerfen
(`doc/f16/controls-commands.md`). Zwei Latenzklassen: **HOTAS** (Schalter/Taste, 0,5 s, im Manöver
benutzbar) und **DED** (Feldeingabe, 4 s, Kopf nach unten) — eine DED-Eingabe wird oberhalb von 1,5 g
abgelehnt, damit keine KI im Kurvenkampf Steuerpunkte eintippt. Ergebnisse: `accepted`, `clamped`
(übernommen, aber eine dokumentierte System-Obergrenze regiert — z. B. BNGO über 6.070 lb), `inhibited`
(übernommen, Wirkung gesperrt — z. B. ALOW ohne Radarhöhenmesser), `rejected`. Ablehnungsgründe sind der
Katalog aus `doc/f16/controls-commands.md` §6 plus zwei EIGENE Modell-Entscheidungen: `out_of_range`
(die Quellen dokumentieren KEINE Bereichsprüfung — FlightBox lehnt ab und sagt es, statt still zu
klemmen), `channel_busy` (Hand/Kopf sind schon beschäftigt) und `depleted` (das Magazin hinter der Box
ist leer — die einzige Ablehnung, die eine Defensivanlage mitten im Beschuss produziert).

Der Kommandostrom ist beobachtbar: `events.log` trägt `cmd CMD_ISSUE` / `CMD_ACK` / `CMD_REJECT` (mit
Ziel, Wert, Klasse, Ergebnis, Grund, gemessener Latenz), `telemetry.csv` die neun Spalten
`cmd_issued`, `cmd_accepted`, `cmd_rejected`, `cmd_clamped`, `cmd_inhibited`, `cmd_pending`,
`cmd_last`, `cmd_last_outcome`, `cmd_last_reason`.

**Der BRIEF (`brief_*`-Zeilen): was der Pilot IM FLUG selbst eingibt.** Normale `set`-Zeilen richten das
Flugzeug im Spawn-Fenster ein (vor dem ersten Piloten-Tick, siehe oben). Eine `brief_*`-Zeile richtet
NICHTS ein — sie sagt dem Piloten, was er nach dem Abheben über den Kommandopfad eingeben soll, in der
Latenzklasse seiner Bedienung, mit dem Risiko, abgelehnt zu werden. Ohne `brief_*`-Zeile bedient der
Pilot überhaupt nichts (er tippt keine Zahlen ein, die ihm niemand gegeben hat).

| Zeile | Klasse | Wirkung |
|---|---|---|
| `set brief_alow_ft <ft>` | DED | ALOW-Boden eingeben (0…50.000 ft, sonst `out_of_range`) |
| `set brief_bingo_lbs <lb>` | DED | BNGO-Schwelle eingeben (0…20.000 lb; über 6.070 lb → `clamped`) |
| `set brief_master_arm arm\|sim` | HOTAS | Master-Arm setzen |
| `set brief_weapon gun\|aim9\|aim120` | HOTAS | Waffenwahl — heute `rejected/not_implemented` |
| `set brief_chaff_s <t>` | HOTAS | Täuschkörper werfen (CMS vorwärts, gewähltes Programm); wiederholbar |
| `set radalt on\|off` | (Spawn) | CARA-Strom; `off` macht den Radarhöhen-Block für den ganzen Lauf `invalid` |

`sim/missions/cmd-avionics.fbm` fährt genau diese Fälle in einem Lauf durch (Annahme, Klemmung,
Wirkungssperre, „nicht implementiert", Kanal belegt, Manöver-Sperre) und schaltet zusätzlich den
Radarhöhenmesser ab — der Referenzlauf für Kommandostrom UND Gültigkeitszustände.

## Waffen — Zuladung, Abwurf, Aufschlag

**Eine abgefeuerte Waffe ist eine EINHEIT wie jede andere.** Kein Sonderpfad, keine eigene
Ballistikformel: ein Store, der die Station verlässt, ist ein `units/FBSimUnit` mit eigener
JSBSim-Instanz auf seinem eigenen Modell, eigenem `FBModule`, eigener Telemetriedatei und denselben zwei
Monitoren. Seine Flugbahn ist die Aerodynamik des Modells plus Schwerkraft (und, bei einem Lenkflugkörper,
sein eigener Schub und seine eigenen Ruder), nichts sonst.

Zwei Store-Klassen, ein Mechanismus — welche Klasse ein Katalogeintrag ist, sagt sein `Guided`-Flag
(`core/FBStore.h`), und danach registriert sich sein Modul selbst:

| Klasse | Modell | Modul | Verhalten im Flug |
|---|---|---|---|
| ungelenkt (`mk82`) | `vendor/jsbsim/aircraft/mk82` (gepinntes Submodul) | `modules/stores/FBStoreModule` (alle Slots Default/NoOp) | integrieren, sonst nichts |
| gelenkt (`aim120`) | `sim/assets/aircraft/aim120` — FlightBox-EIGEN, weil das gepinnte Submodul keine AMRAAM hat und read-only ist (Prinzip 1) | `modules/missile/FBMissileModule` (Sucher + Lenkung + Uplink-Empfänger) | Sucher erfasst, Lenkgesetz kommandiert die simulierten Ruder |

### Zuladung deklarieren

```
unit lead
  module f16
  spawn 46.60000 6.60000 1524 90.0 450
  set store 3 mk82            # eine Zeile je Pylon
  set store 7 mk82
  set brief_master_arm arm    # ohne ARM verweigert der SMS den Abwurf
  set brief_release_s 30      # wiederholbar: ein Pickle je Zeile
  set brief_release_s 60
```

`store <station> <typ>`: Station = die Pylonnummer DIESES Musters (F-16: 1..9, 1/9 Flügelspitze,
5 Mittellinie — `modules/f16/FBF16Sms`), Typ = ein Katalogschlüssel (`core/FBStore.h`; heute `mk82` und
`aim120`). Unbekannte Station, doppelt belegte Station oder unbekannter Typ = Laufzeit-FAIL beim Spawn,
kein stiller Leerflug.

**Was die Zuladung mit dem Flugzeug macht** (`systems/FBStoresSystem`): jede belegte Station ist eine
JSBSim-**Punktmasse** auf dem Trägerflugzeug (Masse, Schwerpunkt und Trägheitsmomente kommen damit aus
`FGMassBalance`, nicht aus eigener Rechnung), und die Summe der Widerstandsflächen wirkt als
JSBSim-**External-Force** (`CdA·qbar`, körperfeste −x-Richtung) am Schwerpunkt der belegten Stationen.
Beim Abwurf geht die Punktmasse im selben Tick auf 0 — das Flugzeug wird sofort leichter und sauberer,
als Physik. Gemessen (`missions/mk82-carriage-{loaded,clean}.fbm`, identisch bis auf vier
`set store`-Zeilen, beide level bei vollem Schub): 4× Mk-82 = **+2.000 lb** Startmasse, Zeit auf Mach 1,0
**25,8 s statt 22,6 s**, auf Mach 1,2 **51,5 s statt 41,1 s**, Spitzen-Mach **1,364 statt 1,416**.

### Abwurf

Der Abwurf läuft über den **Kommandobus** wie jede andere Bedienhandlung (`WeaponRelease`, HOTAS-Klasse,
0,5 s), nicht über einen Direktaufruf, und kann abgelehnt werden. Der SMS entscheidet:

| Bedingung | Ergebnis | Grund |
|---|---|---|
| Master Arm nicht ARM | `rejected` | `hardware_precedence` (§6.2: ein physischer Schalter sperrt den Softwarepfad) |
| Gewicht auf dem Fahrwerk | `rejected` | `hardware_precedence` (Bodenverriegelung) |
| keine belegte Station gewählt | `rejected` | `out_of_context` (§6.5: gültiges Kommando, falscher Kontext) |
| sonst | `accepted` | — |

Nach jedem Abwurf schaltet der SMS selbst auf die nächste belegte Station (Station-Step), so wie es ein
echter SMS tut. `missions/mk82-safe.fbm` ist der Referenzlauf für die Ablehnung (Master Arm nie
scharfgeschaltet), `missions/mk82-drop.fbm` fährt beide Fälle: vier Abwürfe und ein Pickle auf einen
leeren Jet.

### Die Bordkanone

Die M61A1 ist kein Store: sie hängt an keinem Pylon, verlässt das Flugzeug nie und feuert einen STROM.
Deshalb hat sie einen eigenen Systemslot (`systems/FBGunSystem`, F-16-Installation
`modules/f16/FBF16Gun`), einen eigenen Bus-Block (`FBGunBlock`) und eine eigene Missionszeile:

```
unit viper
  module f16
  set brief_master_arm arm    # ohne ARM verweigert die Kanone den Abzug
  set gun_rounds 40           # optional: weniger als die vollen 510 (z.B. für den Leer-Beweis)
  set task bfm                # geschossen wird in der Kampfphase
  set pilot_gun_burst_s 0.3   # optional: kürzere Feuerstöße
```

**Der Abzug ist ein Kommando** (`GunTrigger`, HOTAS-Klasse), sein WERT die Dauer des Drucks in Sekunden
(gekappt auf `MaxBurstS` = 1,0 s, gemeldet als `clamped`). Die Latenz ist ausnahmsweise NICHT die
0,5-s-Tastendauer der anderen HOTAS-Kommandos, sondern 0,1 s: die Verzögerung zwischen Fingerdruck und
erstem Schuss ist der Hochlauf der Rohre, und der steckt bereits im Waffenmodell (`SpoolUpS` 0,3 s) —
beides zu zählen wäre doppelt gerechnet (`core/FBCommandBus::kTriggerLatencyS`). Abgelehnt wird mit
Grund: `hardware_precedence` (Master Arm SAFE oder Räder am Boden), `depleted` (leere Trommel),
`system_failed` (Kanone zerschossen).

**Was ein Feuerstoß IST** (`core/FBGun.h`): pro Sim-Tick ein ballistisches BÜNDEL — bei 6.000 Schuss/min
und 0,1 s Tick zehn Schuss mit einer gemeinsamen Startgeschwindigkeit (Mündungsgeschwindigkeit auf die
Bore-Richtung PLUS die Eigengeschwindigkeit des Jets) und einem Streukegel. Geflogen wird das Bündel vom
Klienten (`core/FBGunProjectiles`, Schwerkraft + quadratischer Widerstand gegen die ISA-Dichte), getroffen
wird auf den VERÖFFENTLICHTEN Posen (`app/FBMissionRunner`) — dieselbe Grenze wie beim Näherungszünder:
die Waffe wertet ihren eigenen Treffer nie aus.

Ereignisse in `events.log`: `gun TRIGGER` (jeder Druck), `gun BURST` (jedes Bündel), `gun HIT`
(erwartete Treffer, Streuung, Aufschlaggeschwindigkeit, Energiedichte, Zone), `gun MISS` (die dichteste
Annäherung eines Bündels, das nichts getroffen hat), `gun DRY` (Trommel leer), `pilot GUN_TRACK` (der
Pilot fliegt jetzt den Trichter statt der Verfolgungskurve). Telemetrie: die `gun_*`-Spalten am rechten
Rand (Trommelinhalt, Verbrauch, Abzüge, Ablehnungen, Lösung + Trichter-Urteil).

**Was die Treffer machen** (`core/FBDamageModel::ApplyKinetic`): dieselbe Register-, Zonen- und
Schwellenlogik wie ein Gefechtskopf, nur wird die ankommende Flächenenergie aus Trefferzahl,
Aufschlaggeschwindigkeit und Streuung berechnet statt aus einer Splittermasse — und sie SUMMIERT sich je
Zone, weil ein Feuerstoß ein durchgehender Strom ist, den der Tick nur in Bündel zerschneidet (sonst
hinge der Schaden an der Tickrate). Ein Gefechtskopf summiert nicht: eine Detonation ist EIN Ereignis.

**Beweismissionen**: `missions/gun-bfm.fbm` (Zielverfolgungspass gegen einen GERADEAUS fliegenden
Gegner) und `missions/gun-turning.fbm` (derselbe Schütze gegen bfm-basics DAUERKURVENDEN Verteidiger —
die harte Prüfung, weil dort die Trichterlösung durch den Trichter WANDERT). Beide enden mit einem Kill
(Exit 1 = FAIL für den Getroffenen); das Urteil steht in den Ereignissen (`gun HIT`, `damage SYSTEM`,
`damage KILL`) und in `gun_sol_err`/`gun_in_funnel`. Die Zahlen der Waffe selbst (Streuungs-Fit gegen
MIL-DTL-45500/1A, Flugzeit, Trichter-Geometrie, Vorhaltelösung gegen die geflogene Bahn,
Munitionsverbrauch, Ablehnung bei leerer Trommel) prüft `make -C sim test-gun`.

**Die Nachführung ist eine Regelung, kein Zielen** (`systems/FBPilot`, Abschnitt 3c): das Gesetz
kommandiert eine Drehrate ∝ Fehler, und gegen einen kurvenden Gegner ist die geforderte Rohrrichtung
eine RAMPE — ein reiner P-Anteil bleibt dann konstant um (Rampenrate × Zeitkonstante) zurück (gemessen:
Fehler nie unter 4,6° bei ~1°-Trichtertoleranz, zwei Feuerstöße, 70 Schuss, kein Treffer). Deshalb sind
FEHLERRATE und Integral eigene Regelanteile. Vorher/nachher über je acht Anflüge pro Verteidiger:
Trichterzeit 3,2 s → 20,7 s (geradeaus) bzw. 0,0 s → 21,6 s (kurvend), Schuss auf dem Ziel 11,9 → 111,2
bzw. 0,0 → 120,4 Patronen, Abschüsse 0 → 5 bzw. 0 → 7 von je acht Läufen, mittlerer Nachführfehler
10,5° → 6,9° bzw. 11,9° → 4,1°.

### Startbedingung des Stores

Position, Lage und Geschwindigkeitsvektor kommen aus dem TRÄGER, über die eine deklarative
IC-Anwendung, die auch jeden Jet spawnt (`fdm/FBFdmBoot` mit `FBFdmSpawn::Ballistic`): Position =
Trägerposition + Stationsversatz (körperfest, mit der Trägerlage gedreht), Lage = Trägerlage, Velocity =
Trägergeschwindigkeit **an dieser Station** (CG-Geschwindigkeit + ω × r, damit ein Abwurf im Roll stimmt).
Es gibt bewusst KEINEN Ejektor-Impuls — für dessen Größe existiert keine belegbare Quelle
(`doc/f16/weapons.md` §4.5), also erbt der Store die Bewegung des Flugzeugs und nichts Erfundenes.
Getrimmt wird nicht: eine Bombe hat kein Ruder.

### Lebenszyklus, Tick-Semantik, Determinismus

- **Besitzer** ist der Runner, wie bei jedem Akteur (`FBActorList`). Das Modul kann keine Einheit
  erzeugen (die IC liegt hinter `fdm/FBFdmBoot.h`, das kein `systems/`- oder `modules/`-File inkludiert);
  der SMS legt nur einen `FBStoreRelease`-Datensatz in eine Warteschlange, die der Runner leert.
- **Entstehung**: am ENDE des Ticks, in dem der Abwurf kommandiert wurde — der Store wird also erst im
  NÄCHSTEN Tick gerechnet. Das ist der Determinismus-Grund: die Step-Phase verteilt Akteursindizes über
  Threads, ein mitten in der Phase auftauchender Akteur würde das Ergebnis von der Reihenfolge abhängig
  machen. Die Kapazität der Akteursliste ist vorreserviert (eine Zeile je belegte Station), es wird im
  Tick-Pfad nichts allokiert.
- **Ende**: der Aufschlag, entschieden von `core/FBFlightMonitor` — derselbe Richter wie für jeden Jet,
  gegen dieselbe Elevationsquelle. Für eine Waffe ist das die Detonation statt eines Absturzes: der
  Runner beendet den LAUF deswegen nicht (`UNIT_RESULT` nennt sie `IMPACT`), sondern stellt die Einheit
  still. Ein Store, der weder aufschlägt noch divergiert, wird nach der Lebensdauer seines
  Katalogeintrags (Mk-82: 300 s) verworfen.
- Der Store-FDM bekommt bewusst KEINEN Boden (`units/FBSimUnit`): JSBSims Ground-Reactions beschreiben
  ein RUHENDES Objekt — die Feder/Dämpfer-Werte des mk82-Modells (10.000 lbf/ft, 200.000 lbf/ft/s)
  divergieren bei 150 m/s Einschlag innerhalb eines Schritts, es bliebe kein Aufschlagzustand zu melden.
  Eine Bombe federt nicht, sie detoniert; wo der Aufschlag ist, entscheidet weiterhin der Richter.

### Der Lenkflugkörper (AIM-120)

Ein gelenkter Store hat drei Dinge mehr als eine Bombe, und alle drei sind simulierte Systeme, keine
Formeln:

- **Sucher** (`modules/missile/FBMissileSeeker`, eine `systems/FBRadarSystem`): eigenes aktives Radar,
  ±10° Sichtfeld, auf die aktuelle Zielschätzung geschwenkt, Reichweite und Aktivierungsentfernung aus
  dem Katalog. Er ist AUS, bis die Lenkung ihn einschaltet, und erfasst wie jedes andere Radar (mehrere
  Blicke bis „firm", danach Gimbal-Verfolgung ±45°). Kein IFF: eine Rakete kann nicht fragen, wer das ist.
- **Lenkgesetz** (`modules/missile/FBMissileGuidance`, eine `systems/FBPilot`-Ableitung):
  Proportionalnavigation `a = N · Vc · (Ω × r̂)` mit N = 4, Schwerkraftkompensation, darunter zwei
  Querbeschleunigungs-Regelkreise (Beschleunigungsmesser + Kreisel, Verstärkung nach Staudruck
  geplant) → **Ruderkommandos** über `FBAutopilot`(Manual) → `FBFlightControl` → `FBFdm::SetControls`.
  Nichts setzt Position, Kurs oder Lage.
- **Uplink-Empfänger** (`modules/missile/FBMissileUplink`, eine `systems/FBDatalinkSystem`): hört die
  Lenkfunk-Aussendung SEINES Schützen ab (dessen `units/FBUnit`-Signatur, wie XMT und IFF eine
  beobachtbare Emission) und veröffentlicht sie als den einen Datalink-Track auf seinem eigenen Bus.

**Drei Lenkphasen** (`msl_phase`): `INERTIAL` (0) — die Startprogrammierung, konstant extrapoliert;
`MIDCOURSE` (1) — der Schütze korrigiert über den Uplink, solange er seinen Lock hält; `TERMINAL` (2) —
der eigene Sucher hat erfasst. Der Übergang ist ein EREIGNIS, kein Timer: der Sucher geht bei der
Aktivierungsentfernung an, die Phase wechselt erst, wenn er wirklich erfasst — was nur gelingt, wenn die
Midcourse-Lenkung ihn nah genug ausgerichtet hat. **Verliert der Schütze seinen Lock, stoppt der Uplink**,
die Phase fällt auf `INERTIAL` zurück und die Rakete fliegt auf ihrer letzten Information weiter
(`missions/intercept-lostlock.fbm` trifft damit noch, `missions/intercept-defeated.fbm` nicht mehr).

**Startbereich (DLZ)**, gerechnet im Feuerleitsystem (`modules/f16/FBF16FireControl`) aus einer
Vorwärtsintegration der Waffenleistungstabelle gegen die aktuelle Radargeometrie: `Raero` (maximale
kinematische Reichweite), `Rtr` (Treffer auch wenn das Ziel beim Start abdreht), `Rmin`
(`Verschlusszeit · Schärfzeit + Wendezuschlag`), plus die Zeitmarken bis Suchereinschaltung und bis
Einschlag. Der SMS verweigert einen Start **ohne Lock**, **ohne Lösung** oder **außerhalb** des
Bereichs — zusätzlich zu den Hardware-Sperren (Master Arm, Bodenkontakt).
`missions/intercept-dlz.fbm` fährt alle drei Antworten in einem Lauf.

**Treffer**: ein Store mit Annäherungszünder (`FuzeRadiusM`, AIM-120: 10 m) detoniert, wenn er eine
Einheit näher passiert als dieser Radius. Gemessen wird die **dichteste Annäherung innerhalb des Ticks**
(Segment-CPA zwischen zwei Posen — bei 1.500 m/s Annäherung liegen zwei 10-Hz-Proben 150 m auseinander,
ein reiner Abstandstest würde jeden echten Treffer verpassen), auf der WAHRHEIT (veröffentlichte Posen),
nicht auf der Schätzung der Rakete. Vor Ablauf der Schärfzeit zündet nichts — deshalb detoniert eine
Rakete nicht am eigenen Träger. Was ein Treffer BEWIRKT, ist noch nicht modelliert: es gibt ein Ereignis
mit Abstand, Annäherungsgeschwindigkeit und Geometrie, kein Schadensmodell.

### Beobachtbar

- `events.log`: `sms RELEASE` (Station, Typ, Massenbilanz), `sms RELEASE_REJECTED` (Grund + Detail),
  `stores SEPARATION` (die vollständige Startbedingung), `stores IMPACT` (`mode=ground|lost`, Position,
  Bodenhöhe, Flugzeit, Geschwindigkeit, **Aufschlagwinkel**, Lage), `stores EXPIRED`.
- Für gelenkte Runden zusätzlich: `sms LAUNCH_SOLUTION` (der komplette Startbereich im Moment des
  Starts), `sms LAUNCH_OUT_OF_ZONE`, `missile PROGRAMMED` (die Startprogrammierung), `missile PHASE`
  (jeder Phasenwechsel mit Grund, Flugzeit, Entfernung), `missile SEEKER_ACTIVE`, `stores DETONATION`
  (Ziel, **Fehlabstand**, Annäherungsgeschwindigkeit, Flugzeit, Aspekt) und `stores MISS`/`EXPIRED`
  (`closestM` = dichteste Annäherung an eine ANDERE Einheit als den Schützen).
- Die Telemetriedatei einer gelenkten Runde hat EIGENE Spalten (der Bus wird pro Einheit aufgebaut, ein
  Jet-Trace ändert sich dadurch um keine Spalte): `msl_phase`, `msl_range`, `msl_closure`,
  `msl_losrate` (was die Proportionalnavigation gegen null treibt), `msl_los_az`/`msl_los_el`,
  `msl_nz_cmd`/`msl_ny_cmd`, `msl_fin_pitch`/`msl_fin_yaw`, `msl_seeker` (0 aus / 1 aktiv / 2 erfasst),
  `msl_tgt_age` (Alter der letzten echten Messung — die Zahl, die den Lock-Verlust sichtbar macht).
- `telemetry.csv`, sechs am Ende angehängte Spalten (bestehende verschieben sich nie): `sms_arm`,
  `sms_station` (gewählte Station, −1 = keine), `sms_loaded`, `sms_lbs` (getragene Storemasse),
  `sms_released`, `sms_gw_lbs` (Startmasse des Flugzeugs — der Massensprung beim Abwurf steht damit in
  derselben Zeile wie die Buchführung).
- eine eigene `telemetry_<callsign>_<typ>_<n>.csv` je Store: dieselbe Schemabreite wie ein Jet, mit der
  vollen Flugbahn (10 Hz) bis zur Aufschlagzeile.

### Gegenprobe zur unabhängigen Rechnung

`missions/mk82-drop.fbm` mit `--elev const` (flache 0-m-Basis, also Abwurfhöhe = Abwurf-AGL). Gemessen
gegen die widerstandsfreie Herleitung aus `doc/f16/weapons.md` §4.2 (`t=√(2h/g)`, Reichweite `v·t`):

| Größe | JSBSim-mk82 | widerstandsfrei | Differenz |
|---|---|---|---|
| Fallzeit aus 2.499 m | 24,10 s | 22,58 s | **+6,8 %** |
| Fortbewegung bei 231 m/s | 4.600 m | 5.221 m | **−11,9 %** |
| Vertikalgeschwindigkeit beim Aufschlag | 190,8 m/s | 221,4 m/s | **−13,8 %** |

(alle vier Stores desselben Laufs innerhalb von 0,01 s / 8 m identisch — die Streuung ist der
Unterschied ihrer Abwurfzustände, nicht Rauschen)

Beide Vorzeichen sind die erwarteten: Widerstand verlängert den Fall und bremst zugleich die
Horizontalkomponente stärker, als er den Fall verlängert. Die Herleitung ist genau das, was ihre eigene
Quelle behauptet, eine **untere Schranke** — sie ist kein Zielwert und wird nicht weggerechnet.

## Bodenziele (`module target_soft` | `target_hard`)

Ein **statisches Bodenziel ist eine ganz gewoehnliche `unit`** — kein neues Schluesselwort, keine zweite
Deklarationssyntax. Es unterscheidet sich in genau einer Zeile:

```
unit bunker
  module target_soft            # FBModuleRegistry-Key wie 'f16' — die ART des Ziels
  team hostile                  # die Fraktion, gegen die ein 'kill unit/team' geprueft wird
  spawn 46.90000 7.05000 ground 90.0 0    # Position; 'ground' = Elevation aus dem Provider, hdg = Laengsachse
```

**Es hat keine Flugdynamik und deshalb kein JSBSim-Modell.** Das ist die Konstruktionsentscheidung, und
sie sitzt an genau einer Stelle: `FBModule::FdmModelName()` liefert einen LEEREN Namen, `FBModule::
UnitKind()` liefert `FBUnitKind::Ground`, und die Spawn-Bahn (`app/FBMissionBoot.h`) baut daraufhin eine
`units/FBSimUnit` OHNE `FBFdm`. Alles andere an der Einheit ist unveraendert dieselbe Mechanik wie bei
einem Jet: Identitaet, Fraktion, veroeffentlichte Pose, **Gesundheitsregister**, **Schadensmodell**,
Roster, Telemetriedatei, Unit-Registry. Die Alternative — einem Bunker ein triviales JSBSim-Modell zu
geben — haette ein erfundenes aerodynamisches Objekt bei 100 Hz integriert, nur um die Position zu
reproduzieren, an der es gespawnt wurde.

Folgen, die aus `UnitKind::Ground` direkt fallen: der Physik-Monitor sieht ein Bodenziel nie (es fliegt
nicht, es kann keinen Absturz haben), Luft-Luft-Sensoren (Radar/Datalink) finden es nicht, Kanonen-Bundles
werden nicht gegen es aufgeloest (**kein Strafing**, s.u.), und es steht im Roster, gegen den
`objective kill unit` geprueft wird. Ein `set`-Schluessel ist bei einem Ziel IMMER unbekannt und damit
ein Missions-FAIL — was ein Ziel ist, sagt sein Modulname, wo es steht, seine `spawn`-Zeile.

| Modul | Struktur faellt ab | ...degradiert ab | gedacht als |
|---|---|---|---|
| `target_soft` | 2,8e3 J/m² (Mk-82: ~45 m) | 1,2e3 J/m² (~69 m) | ungeschuetzte Anlage, Fahrzeugpark, Stellung |
| `target_hard` | 9,0e4 J/m² (Mk-82: ~8 m) | 2,5e4 J/m² (~15 m) | Bunker, gehaertetes Bauwerk |

Nur `Structure` ist deklariert: `FBSystemHealth::CombatEffective` fragt nach Triebwerk, Steuerung und
Struktur, und ein Bauwerk hat genau eines davon. Zerstoert = `Structure` failed = kampfunfaehig =
`objective kill unit <name>` erfuellt. Die Schwellen sind [SET] (weapons.md §4.7 nennt Gefechtskopf-
Innenleben als echte Luecke), verankert an der offen zitierten Groessenordnung von 50–60 m
Wirkradius einer 500-lb-Bombe gegen ungeschuetzte Ziele.

**Der Bodenaufschlag als Detonation:** trifft ein Store den Boden, loest der Runner — nicht das Modul —
seinen Gefechtskopf gegen jedes Bodenziel in der Naehe auf, durch dasselbe `core/FBDamageModel` wie eine
Rakete neben einem Jet. Der Aufschlagpunkt wird dabei **sub-Tick** rekonstruiert (der Store fliegt ohne
Bodenkontaktkraefte weiter, also `Tiefe / Sinkrate` zurueckprojiziert): beim 0,1-s-Tick des Laufes sind
das ~20 m Horizontalweg, ein Fuenftel des gesamten Lieferfehlers. Gegen FLUGZEUGE wird ein Bodenburst
bewusst NICHT aufgeloest — ein Jet ueber der eigenen Detonation ist real gefaehrdet, aber die dafuer
noetige Splitter-Geometrie gegen eine Zelle gibt es hier nicht, und ein erfundener Radius waere eine Zahl,
die sich als Physik ausgibt.

## Luft-Boden-Angriff (`set task attack`, `set attack_mode ccip|ccrp`)

Eine Einheit mit `set task attack` fliegt einen **Bombenangriff auf den aktiven Steerpoint**
(`systems/FBPilot`s Attack-Phase). Drei Teile, eine Entscheidung:

1. **Anflug** — `FBAutopilot::Direct` auf den aktiven Wegpunkt, auf DESSEN Hoehe und Geschwindigkeit,
   also ein **waagerechter Laydown-Anflug**. Bewusst waagerecht: `Direct` haelt eine Hoehe und steuert
   einen Punkt an, fliegt also genau diese Bahn exakt und wiederholbar, waehrend ein 20–30°-Sturz den
   Piloten die ganze Bahn gegen die eigene Hoehenhaltung kaempfen liesse — und dann waere jeder Meter
   Fehlabstand ein Streit ueber das Fliegen statt eine Messung der Abwurfrechnung.
2. **Abwurf** — EIN Pickle ueber den Kommandobus, auf den Cue der Feuerleitung und auf nichts sonst.
   Der Pilot rechnet keine Ballistik; er liest den `FBFireControlBlock` wie jedes andere Instrument.
3. **Abdrehen** — 135°-Ausweichkurve mit Steigflug (F-16-Zahlen: `AttackEgressTurnDeg` 135,
   `AttackEgressClimbM` 600, `AttackEgressS` 30), danach zurueck in die Route-Phase.

**CCIP und CCRP sind EINE Rechnung, zwei Fragen** (`core/FBBallistics`, gemeinsames Primitiv): eine
Vorwaerts-Integration der Ballistiktabelle des Stores (`core/FBStore.h`s `FBWeaponPerf` — Masse, EIN
Cd, Referenzflaeche, Schaerfzeit) gegen eine ebene Aufschlagflaeche. Die Flaeche ist die
**Steerpoint-Elevation**, also der `FBElevationProvider`-Wert, den auch der Radarhoehenmesser liest und
gegen den der Monitor den Aufschlag beurteilt. Aus derselben Integration:

| Modus | Cue, auf den der Pilot ausloest |
|---|---|
| `ccrp` | `AgTimeToReleaseS <= 0` — die Solution-Cue laeuft den Steering-Line herunter und passiert den FPM |
| `ccip` | derselbe Moment UND `|AgCrossErrM|` innerhalb der Pipper-Toleranz (F-16: 45 m) — „das Pipper liegt SEITLICH auf dem Ziel", das Urteil, das ein Countdown nicht faellen kann |

Auf einem sauber geflogenen Anflug loesen beide im selben Tick aus; auf einem schlecht gespurten loest
CCRP aus und trifft daneben, CCIP loest gar nicht aus. Beides ist das dokumentierte Verhalten
(doc/f16/weapons.md §2.5).

**Der Pickle wird um die eigene Betaetigungslatenz vorgehalten.** Ein Kommando erreicht die Box eine
Klassenlatenz spaeter (`core/FBCommandBus`, HOTAS 0,5 s); genau auf dem Cue zu druecken wuerde den Store
0,5 s zu spaet loesen — bei 231 m/s sind das 115 m, mehr als die ganze Rechnung wert ist. Der echte Jet
loest dasselbe Problem andersherum: in CCRP HAELT der Pilot den Knopf und das FLUGZEUG loest aus, wenn
die Cue den FPM passiert. Gemessen: ohne Vorhalt 123 m lang, mit Vorhalt 8 m.

Missionszeilen:

```
  set task attack                 # startet in der Attack-Phase (wie 'bfm'/'intercept')
  set attack_mode ccip|ccrp       # welchen Cue der Pilot nimmt; setzt auch den Modus im Abwurf-Protokoll
  set pilot_attack_bias_s <s>     # Variante: Abwurf um s Sekunden NACH dem Cue (+ = spaet, − = frueh)
  set pilot_attack_ccip_m <m>     # Variante: die CCIP-Pipper-Toleranz
```

### Beobachtbar (Luft-Boden)

- `pilot ATTACK_RELEASE` — der Cue im Moment des Drueckens: `ttrS`, `leadS`, `biasS`, `alongErrM`,
  `crossErrM`, `missM`, `bombRangeM`, `tofS`, `armMarginS`.
- `sms RELEASE_SOLUTION` — was der Rechner VORHERSAGTE, wie es mit der Runde den Jet verlaesst
  (`predLat`/`predLon`/`predTofS`/`aimLat`/`aimLon`/`aimMissM`/`armMarginS`/`solAgeS`). Das
  Gegenstueck zu `LAUNCH_SOLUTION` einer gelenkten Runde.
- `stores IMPACT` — zusaetzlich `crossLat`/`crossLon`/`crossBackS`/`crossTofS`: der
  sub-Tick-rekonstruierte Durchstosspunkt, gegen den alles Weitere gemessen wird.
- `stores DELIVERY` — **Vorhersage gegen Wirklichkeit**, vom Besitzer der Simulation gemessen:
  `predErrM` (was der RECHNER falsch hatte), `aimErrM` (was die LIEFERUNG falsch hatte),
  `aimLongM`/`aimAcrossM` (lang/kurz und rechts/links in der Anflugrichtung), `tofErrS`, `planeM` gegen
  `groundAslM` (die Elevationsdifferenz zwischen Rechenebene und echtem Boden am Aufschlagpunkt).
- `damage DAMAGE`/`SYSTEM`/`KILL` am Bodenziel — dieselben Zeilen wie bei einem getroffenen Jet.
- `mission UNIT_RESULT` eines Bodenziels: `INTACT` oder `DESTROYED` (statt eines Flug-Urteils).

### Gemessen (`--elev const`, flache 0-m-Basis)

`missions/attack-ccrp.fbm` / `attack-ccip.fbm`: 19 km Anflug, 900 m, 450 KCAS, Wurfweite 2.880 m.

| Groesse | CCRP | CCIP | 2 s zu spaet (`attack-late.fbm`) |
|---|---|---|---|
| `predErrM` (Rechner gegen Modell) | 57,1 m | 57,1 m | 57,0 m |
| `aimErrM` (Bombe gegen Ziel) | **37,2 m** | **37,2 m** | **482,2 m** |
| davon lang / seitlich | 19,6 / 31,6 m | 19,6 / 31,6 m | 481,6 / 23,4 m |
| `tofErrS` | −0,097 s | −0,097 s | −0,097 s |
| Urteil | SUCCESS (exit 0) | SUCCESS (exit 0) | TIMEOUT (exit 3), Ziel steht |

Der Rechnerfehler ist ein **systematischer Vorhalt-Fehler nach kurz**: die echte Bombe fliegt weiter,
als die Tabelle sagt, weil die Tabelle EIN Cd fuer alle Machzahlen fuehrt und den Auftrieb der
weathercockenden Runde gar nicht kennt. Beides sind erklaerte Auslassungen des RECHNERS
(`core/FBBallistics.h`), keine Fehler der Simulation — genau wie die DLZ-Abweichung einer gelenkten
Runde. Der Lieferfehler ist kleiner als der Rechnerfehler, weil dessen Laengsanteil dem Abwurfmoment
entgegenlaeuft; der verbleibende dominante Anteil ist **seitlich** und stammt aus dem Spurfehler der
Fuehrung (~0,4° Kurs bei 2.880 m Wurfweite = ~30 m), nicht aus der Ballistik.

`missions/attack-hardened.fbm` fliegt denselben Abwurf gegen `target_hard`: derselbe 37-m-Fehlabstand,
KEINE Schadenszeile (die ankommende Energie erreicht nicht einmal die Degrade-Schwelle) — TIMEOUT,
`result=INTACT`. Die Fragilitaetsklassen sind damit ein Modell und keine Dekoration.

## Kampf-Missionen (`set task bfm`)

Eine Einheit mit `set task bfm` fliegt keine Wegpunkte, sondern **BFM** (`systems/FBPilot`s Bfm-Phase):
sie regelt gegen den **gelockten Radarkontakt** und gegen nichts sonst. Alles, was sie über den Gegner
weiß, baut `systems/FBBfmTrack` aus aufeinanderfolgenden Kontakten (Position + geschätzter
Geschwindigkeitsvektor); Registry, Weltwahrheit und Datalink-Tracks sind für den Piloten unerreichbar.
Deshalb gehört in eine BFM-Mission zwingend `set datalink off` (sonst wäre die Sensor-Beschränkung nur
behauptet) und ein Auto-Lock-Modus (`set fcr_mode acm_hud` o.ä. — CRM lockt nicht von selbst).

Beispiele: `sim/missions/bfm-basic.fbm` (Verfolger 2 nm hinten), `bfm-offset.fbm` (Verfolger seitlich
versetzt, Aspekt ~90° = Winkelnachteil), `bfm-merge.fbm` (Head-on-Merge, Aspekt 180°), `bfm-blind.fbm`
(versetzter Merge — der Vorbeiflug REISST DEN LOCK ab und zeigt Extrapolation + Suche + Wiedererfassung).

**Die Suche ist zielbewegungs-bewusst.** Sie fliegt nicht die zuletzt GEMESSENE Position an (dort ist er
längst nicht mehr), sondern das DATUM aus `systems/FBBfmTrack::Datum`: letzter Vektor fortgeschrieben,
solange diese Vorhersage mehr wert ist als der letzte Look (bis 2/ω), Unsicherheitsradius
`min(0,5·V·ω·t², V·t)`, und die Webbreite ist genau dessen Winkelbreite von hier aus. Die Webphase
beginnt beim SUCHBEGINN statt an der Missionsuhr — sonst entscheidet der Zufallszeitpunkt des
Kontaktverlusts über den Erfolg. Gemessen über 16 Merges derselben Geometrie (Sweep über die
Verlustzeit): vorher wurden 6 von 11 Kontaktverlusten wieder erfasst (34/80/81/86/170/240 s, fünfmal
nie), nachher 11 von 11 (10…141 s, Median 39 s).

**Ein Kampf hat kein Wegpunkt-Ziel — solche Missionen enden per TIMEOUT (Exit 3), und zwar
absichtlich.** Der Verfolger deklariert keine Objectives (kein `wp`), der Gegner fliegt sein definiertes
Manöver (in den vier Missionen: ein einziger Wegpunkt im ZENTRUM seiner Kurve, den die Direct-Guidance
nicht erreichen kann → stabiler Dauerkurvenflug am Bank-Limit). `core/FBMissionMonitor` hat damit nichts
zu urteilen und sagt korrekt TIMEOUT; das Urteil über den Kampf liest die Auswertung aus der Telemetrie
(`bfm_*`-Spalten unten).

Zusätzliche Telemetrie-Spalten (Quelle `systems/FBBfmTrack`, ganz hinten angehängt — bestehende Spalten
verschieben sich nie): `bfm_pursuit` (`none`/`search`/`lead`/`pure`/`lag`), `bfm_valid` (Schätzung jung
genug zum Verfolgen), `bfm_locked` (Radar hält ihn JETZT), `bfm_age` (s seit dem letzten echten Look),
`bfm_rng` (nm), `bfm_ata` (Grad off nose, + = rechts), `bfm_aspect` (Grad AM ZIEL: 0 = genau in seinem
Rücken, 180 = frontal), `bfm_hca` (Kursdifferenz), `bfm_clos` (kt, + = annähernd), `bfm_es` (eigene
Energiehöhe = Höhe + v²/2g, ft — die einzige Energiezahl, die aus EIGENEN Instrumenten kommt),
`bfm_gcmd` (kommandiertes g), `bfm_ctrl` (Kontrollposition JETZT), sowie die drei Integrale
`bfm_engaged` / `bfm_lock_s` / `bfm_ctrl_s` (s) — Lock-Haltequote und Zeit in der Kontrollposition sind
damit aus der LETZTEN Zeile ablesbar. Jede dieser Größen ist aus der Perspektive der Einheit selbst
berechenbar; alles, was die Weltwahrheit braucht (z.B. der WAHRE Aspektwinkel), gehört in die
Auswertung, nicht in den Piloten.

## Abfang-Missionen (`set task intercept`)

Eine Einheit mit `set task intercept` fliegt einen **Abfang jenseits der Sichtweite** (`systems/FBPilot`s
Intercept-Phase). Der Gegenpol zu `bfm`: BFM wird mit der NASE geflogen und der Lock geht nie weg — ein
Abfang wird mit dem SENSOR geflogen, und die ganze Kunst ist, wann man ihn worauf richtet. Die Phase ist
eine eigene kleine Zustandsmaschine (`systems/FBEngagement`, Spalte `eng_state`):

| Zustand | Was der Pilot tut |
|---|---|
| `search` | Den gebrieften Vektor fliegen (der aktive Wegpunkt IST der Vektor: Peilung, Entfernung, Flughöhe), im SUCHMODUS, Antennenhöhe auf das eigene Höhenband gestellt — und **NICHT locken**: ein Lock ist eine Warnung an den Gegner. |
| `closing` | Ein Kontakt steht auf dem Schirm: Verfolgungskurs, co-altitude mit dem Kontakt, Antenne auf dem Rückstrahler zentriert — immer noch ohne Lock. |
| `attack` | Innerhalb der gebrieften Lock-Entfernung: designieren (TMS vorwärts über den Kommandobus), den Startbereich aus dem FireControl-Block lesen, und schießen, sobald `range <= Rtr` UND das Ziel innerhalb des Suchkopf-Erfassungskegels liegt. |
| `support` | Eine Rakete fliegt: Lock HALTEN (der Uplink führt sie) und dabei **abdrehen bis an den Rand des Antennenkegels** (Crank). Gehalten bis zur vom FCC vorhergesagten Flugzeit — der Suchkopf übernimmt zwar früher, aber bis zum Einschlag gibt es keinen Grund, dem Ziel entgegenzufliegen. |
| `defend` | Jemand hat eine Lösung auf diesem Flugzeug: **quer zur Bedrohungspeilung drehen** (90°, dort ist die eigene Radialgeschwindigkeit null und ein Puls-Doppler-Sucher kann Flugzeug und Düppelwolke nicht trennen) und Täuschkörper werfen. Beides über den Kommandobus, nach einer menschlichen Reaktionszeit. |
| `abort` | Nichts mehr zum Schießen, oder der Kampf ist unter die Abfang-Entfernung gefallen: kalt abdrehen. |

**Wann eine Bedrohungswarnung eine Antwort verlangt** ist die Kernregel: ein Suchkopf auf dem eigenen
Flugzeug (RWR-Modus `missile`) immer; ein bloß VERFOLGENDES Radar (`track`) erst, wenn der eigene Angriff
nichts mehr zu gewinnen hat — der Schuss ist weg und braucht keine Führung mehr, oder es gab nie einen
zu schießen. Sonst verliert man das Gefecht, indem man vor dem eigenen Schuss abdreht.

**Und wann sie wieder aufgenommen wird.** Ist die Bedrohung vorbei (kein Sucher mehr, Haltezeit
abgelaufen), fragt der Pilot genau drei Instrumente: Waffen auf den Trägern (Stores-Block), kein BINGO
im Warnblock, ein strahlendes Radar. Fehlt eines, löst er sich (`abort`) — das ist die ehrliche
Abbruchbedingung. Sonst geht er zurück in `search`, und die sucht dann das DATUM des zuletzt gesehenen
Gegners statt des gebrieften Vektors: Kurs, Höhenband, Antennenelevation und Webbreite kommen alle aus
`systems/FBBfmTrack::Datum`. Hat er nie etwas gesehen, ist das Datum ungültig und alles bleibt exakt der
gebriefte Vektor. Gemessen an `bvr-duel.fbm`: vorher flogen beide nach dem Abwehrmanöver ihren Vektor
weiter und trennten sich 474 s lang bis 70,7 km, jeder mit einer Rakete an Bord; jetzt kehren beide um
(größte Trennung 55,6 km bei t≈280 s), gehen bei t≈355 s wieder in `closing` und bei t≈365 s in
`attack`, und der zweite Schuss fällt bei t=527 s (43,6 m Fehlabstand, im Notch abgewehrt wie der
erste).

Die Zahlen sind Modul-Sache (`modules/f16/FBF16Pilot`): Lock ab 16 nm, Schuss bei Rtr, Crank auf 45°
(APG-68-Kardanwinkel 60° minus Reserve), Abbruch unter 5 nm, Suchmodus = CRM. Generisch (Piloten-, nicht
Flugzeug-Eigenschaft) bleiben die 1,0 s Reaktionszeit und die 0,5 s zwischen zwei Bedienhandlungen —
beide ZUSÄTZLICH zur Bus-Latenz der jeweiligen Bedienklasse.

Wie BFM-Missionen enden Abfang-Missionen per **TIMEOUT (Exit 3)**, und aus demselben Grund: ein Gefecht
hat kein Wegpunkt-Ziel. Das Urteil steht in der LETZTEN ZEILE der `eng_*`-Spalten (Quelle
`systems/FBEngagement`, ganz hinten angehängt — bestehende Spalten verschieben sich nie):

| Spalte | Bedeutung |
|---|---|
| `eng_state` | s.o. |
| `eng_tgt_nm` / `eng_ata` / `eng_aspect` / `eng_clos` / `eng_locked` | die aktuelle Geometrie des bearbeiteten Kontakts (−1 = keiner) |
| `eng_detect_s` / `eng_lock_s` | **Zeit bis zur Erfassung**: erster fester Kontakt, erster Lock |
| `eng_shot_s` / `eng_shot_nm` / `eng_shot_ata` / `eng_shot_aspect` | **Schussentfernung und -geometrie** |
| `eng_shot_rtr_nm` / `eng_shot_raero_nm` / `eng_shot_rmin_nm` | der Startbereich IM MOMENT des Schusses — ein Schuss ist nur so gut wie die Geometrie, in der er fiel |
| `eng_tta_s` / `eng_tti_s` | die beiden Vorhersagen des Feuerleitrechners (bis Eigenlenkung, bis Einschlag) — gegen die die geflogene Flugzeit gemessen wird |
| `eng_support_s` / `eng_support_f` / `eng_pitbull` | **wurde die Führung bis zur Eigenlenkung gehalten** — Sekunden mit Lock im Führungsfenster, als Anteil davon, und das Urteil am Fensterende |
| `eng_threat_s` / `eng_react_s` | **Reaktionszeit auf die Bedrohungswarnung** (gemessen ab dem Moment, in dem die Warnung eine Antwort VERLANGTE, nicht ab dem ersten Symbol) |
| `eng_defend_s` / `eng_chaff` / `eng_shots` | Sekunden in der Verteidigung, tatsächlich ausgestoßene Düppel-PATRONEN (die Zählung der CMDS-Anlage, nicht die der Schalterwürfe), abgefeuerte Schüsse |
| `eng_es` / `eng_es_min` | **Energiezustand über den Ablauf**: Energiehöhe jetzt und ihr Minimum seit Beginn des Gefechts |

Beispiele: `sim/missions/bvr-intercept.fbm` (einseitig: nicht schießendes Ziel, ganze Kette Suche →
Erfassung → Schuss → Führung → Treffer), `bvr-duel.fbm` (**beidseitig**: zwei KI-Jets, beide bewaffnet,
beide mit RWR und Gegenmaßnahmen — nahezu spiegelbildlich und deshalb ein Patt; seit der Pilot nach der
Verteidigung zum Datum zurückkehrt, ist es ein Patt MIT zweitem und drittem Anlauf statt zweier
auseinanderfliegender Jets, weshalb der Timeout dort 700 s ist), `bvr-duel-decided.fbm`
(dasselbe Paar, ENTSCHIEDEN: 6.000 m und 150 kt Energieunterschied, sonst identisch — der Höhere/
Schnellere hat das größere Rtr, schießt zuerst, der andere kommt nie zum Schuss; SUCCESS gegen FAIL),
`bvr-defend.fbm` + `bvr-defend-blind.fbm` (das Verteidigungs-Paar:
identischer Schuss, EINE Zeile Unterschied — `set rwr on|off` — also reagierende gegen nicht reagierende
KI).

## Piloten-Varianten (`set pilot_*`) und das Turnier

Die Entscheidungszahlen eines Abfangs sind Eigenschaften des PILOTEN, nicht der Zelle. Sie stehen als
Defaults im Modul (`modules/f16/FBF16Pilot`) und sind je Einheit als Missionsdaten überschreibbar:
`set pilot_<param> <wert>` → `FBPilot::ApplyTuning` → `systems/FBPilotTuning`. Eine **Variante ist damit
eine Zeile in einer Missionsdatei**, keine neue Klasse und kein neuer Build. Ein nicht gesetzter
Parameter bleibt die eigene Zahl des Piloten — eine Mission ohne `pilot_*`-Zeile fliegt unverändert.

| Schlüssel | Band | Was er entscheidet |
|---|---|---|
| `pilot_speed_kt` | 150…900 | Geschwindigkeit, mit der der Abfang geflogen wird (kt TAS) |
| `pilot_lock_nm` | 1…40 | Entfernung, bei der designiert wird — der Lock ist die Warnung an den Gegner |
| `pilot_shot_rtr` | 0,1…3,0 | Auslösen bei diesem Vielfachen von Rtr (>1 = jenseits von Rtr) |
| `pilot_shot_ata_deg` | 1…60 | wie weit off-nose noch geschossen wird |
| `pilot_shot_spacing_s` | 0…120 | Abstand zweier Schüsse auf dasselbe Ziel |
| `pilot_crank_deg` | 0…60 | wie weit der gestützte Schuss weggedreht wird (Gimbal-Grenze 60°) |
| `pilot_abort_nm` | 0…40 | darunter ist der Abfang vorbei |
| `pilot_beam_deg` | 0…180 | Verteidigungsdrehung gegen die Bedrohungspeilung (90° = reiner Beam) |
| `pilot_chaff_s` | 0,2…60 | Wurfintervall während der Verteidigung |
| `pilot_defend_hold_s` | 0…120 | wie lange die Verteidigung nach der letzten Warnung gehalten wird |
| `pilot_react_s` | 0…30 | menschliche Reaktionszeit auf eine Bedrohungswarnung (Default 1,0 s) |
| `pilot_action_s` | 0,1…30 | eine Bedienhandlung pro dieser Zeit (Default 0,5 s) |
| `pilot_gun_burst_s` | 0,1…1,0 | Länge EINES Abzugsdrucks (Default 0,5 s ≈ 50 Schuss) |
| `pilot_gun_tol_frac` | 0,05…1,0 | wie eng der Pipper vor dem Schuss gehalten wird, als Anteil der Trichter-Toleranz (Default 0,35) |
| `pilot_bfm_ctrl_min_nm` | 0,05…5,0 | Nahkante der Kontrollposition im Kurvenkampf |
| `pilot_bfm_ctrl_max_nm` | 0,05…10,0 | ...und ihre Fernkante — eine Kanonenposition liegt IM Trichter (600…3.000 ft), eine Raketenposition außerhalb |

Beides — Reaktions- und Handlungszeit — liegt weiterhin ZUSÄTZLICH zur Bus-Latenz der jeweiligen
Bedienklasse (`core/FBCommandBus`); keine Variante kann schneller antworten, als der Jet erlaubt. Ein
unbekannter Schlüssel oder ein Wert außerhalb des Bandes ist ein Laufzeit-FAIL wie jede andere
schlechte `set`-Zeile — eine vertippte Turnier-Zahl fliegt nicht still den Default.

**Der Turnierläufer** ist ein Skript, kein Build-Target: `sim/tools/fb_tournament.py` (stdlib-Python).
Er schreibt aus einer Variantenliste (`sim/tools/variants-bvr.txt`) für jedes Paar und **beide
Seitenzuordnungen** eine `.fbm`-Datei, fährt sie über `fb-gym --threads N` und wertet Telemetrie +
`UNIT_RESULT` aus. Die Fitness dominiert im Ergebnis (Abschuss +1000, eigener Verlust −1200, gelandeter
Treffer +150, nie geschossen −250) und ordnet erst darunter nach Handwerk (Schussgeometrie im
Startbereich, Stützanteil, Schuss-Vorsprung, Verteidigung, Energie) — die Handwerkssumme kann ein
Ergebnis innerhalb eines Laufs nie umdrehen. Die Ausgabe nennt je Paarung BEIDE Seiten mit ihren
Einzelposten, die Rangliste trennt `outcome` von `craft`.

```
sim/tools/fb_tournament.py --variants sim/tools/variants-bvr.txt --out /tmp/t --geometry split \
    --threads 2 --check-determinism
```

## Ausgabe je Lauf (`--out DIR`)

- `telemetry.csv` — der primäre Akteur (Index 0). Kanonischer Name, unverändert.
- `telemetry_<callsign>.csv` — jede weitere Einheit, gleiches Schema. Eine Datei je Unit statt einer
  breiten Zeile: die Spalten folgen dem MODUL der Einheit, eine geteilte Zeile müsste entweder alle
  Module in ein Schema zwingen oder den Header von der Besetzung abhängig machen.
- `events.log` — `t=SEK LEVEL tag EVENT key=val …`, greppbar.

**Schadens-Ereignisse und -Spalten** (`core/FBDamageModel`, s. CLAUDE.md „Schadensmodell"): je Treffer
eine `damage DAMAGE`-Zeile (Zone, Abstand zur Zellenstruktur, Fragment-Energie in J/m², Sprengmasse,
Annäherungsgeschwindigkeit, Zündpunkt im Körperrahmen, die zwei Bitmasken), je betroffenem System eine
`damage SYSTEM`-Zeile (`system=… state=degraded|failed`) und, wenn der Treffer die Einheit
kampfunfähig macht, genau eine `damage KILL`-Zeile. In `telemetry.csv` kommen ganz hinten vier Spalten
dazu (bestehende verschieben sich nie): `dmg_hits`, `dmg_failed`, `dmg_degraded` (Bitmasken über
`FBSystemId`) und `dmg_effective`. Die Blockgültigkeits-Spalten (`blk_*`) zeigen denselben Vorgang aus
der Sicht des Avionik-Busses: ein ausgefallenes System schaltet seinen Block auf `0` (invalid).

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
- `sim/missions/cmd-avionics.fbm` — der Avionik-Kommando-/Gültigkeits-Demonstrator (kein Flugtest):
  briefte Eingaben in beiden Latenzklassen, alle vier Quittungsergebnisse, beide Eigen-Policy-Gründe,
  dazu ein abgeschalteter Radarhöhenmesser (`invalid`) neben dem ohnehin gehaltenen Radarbild (`held`).
- `sim/missions/attack-ccrp.fbm` — der Luft-Boden-Referenzlauf: 19 km waagerechter Anflug, CCRP-Abwurf
  einer Mk-82 auf ein deklariertes `target_soft`, Aufschlag 37,2 m neben dem Ziel, Ziel zerstoert,
  `objective kill unit` erfuellt (SUCCESS, exit 0). Mit `--elev const` messen (flache 0-m-Basis:
  Rechenebene, Zielhoehe und Aufschlagboden sind dann dieselbe Zahl).
- `sim/missions/attack-ccip.fbm` — derselbe Anflug auf dem CCIP-Cue, mit `objective survive` so gebaut,
  dass der Lauf ueber den Treffer hinaus bis zum Ende der Ausweichkurve und zurueck in die Route laeuft:
  der VOLLSTAENDIGE Angriffsablauf in einem Lauf.
- `sim/missions/attack-late.fbm` — die Gegenprobe: dieselbe Datei mit EINER Zeile mehr
  (`set pilot_attack_bias_s 2.0`, Abwurf zwei Sekunden nach dem Cue). 482 m Fehlabstand statt 37 m,
  Ziel steht, TIMEOUT (exit 3) — das Mass dafuer, was die Rechnung leistet.
- `sim/missions/attack-hardened.fbm` — derselbe gute Abwurf gegen `target_hard`: gleicher Fehlabstand,
  keine Wirkung, `result=INTACT`. Die Fragilitaetsklassen sind ein Modell, keine Dekoration.
