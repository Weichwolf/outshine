# FlightBox Core (`sim/src/core/`)

> Body still in German — translation pass pending (see [roadmap](../roadmap.md)).

**Quelle**: die Quelldateien selbst — `sim/src/core/` (53 Dateien) + `sim/src/math/FBMat4.h`, Stand
Commit `9673e00` (2026-07-27) — plus `CLAUDE.md`s `core/`-Absatz im `sim/src/`-Verzeichnisbaum. Die
langen Kommentar-Banner der Quelldateien tragen die HERLEITUNGEN; sie sind hier vollständig
übernommen, jede Zahl mit ihrer Begründung oder ihrer Kennzeichnung als Setzung. Wo Code und
CLAUDE.md auseinanderlaufen, steht das unter [Offene Punkte](#gaps) — nicht stillschweigend
aufgelöst.

**Wofür diese Schicht zuständig ist**: die WERTETYPEN der Simulation (was ein Store, ein Kontakt, ein
Wegpunkt, ein Kommando IST), die geteilten PRIMITIVE (Geodäsie, Atmosphäre, Ballistik, Einheiten), die
zwei BEOBACHTUNGSKANÄLE (Log, Telemetrie), der AVIONIK-BUS (Zustand + Kommando) und die drei
Zustände, die kein Modul schreiben darf: die beiden unbestechlichen Richter und das
Gesundheitsregister.

**Wofür sie NICHT zuständig ist**: Verhalten. Kein Regelgesetz, kein Sensor, kein Pilot, kein
Renderer, keine JSBSim-Naht. `core/` weiß nicht, dass es eine F-16 gibt, es kennt kein `FGFDMExec`,
und es besitzt keine Einheit. Es wird von `systems/`, `modules/`, `units/`, `fdm/`, `app/` benutzt —
und benutzt keines davon.

---

## Spec

`core/` is the value layer of the simulation: what a store, a contact, a waypoint, a command **is**,
the shared primitives (geodesy, atmosphere, ballistics, units), the two observation channels, the
avionics bus (state + command) and the three states no module may write. It carries **no behaviour** —
no control law, no sensor, no pilot, no renderer, no JSBSim seam.

| Contract | Acceptance / measurement anchor |
|---|---|
| `core/` never points at `systems/` or `modules/` | include graph; the `core-lib` target builds without either |
| I/O-free, not format-free | no `FILE*`/`fstream`; `snprintf` into a local buffer is allowed (`../conventions.md`) |
| The avionics bus is a set of typed output blocks, each with **exactly one** writer and a three-state validity head (`Invalid` / `Valid` / `Held`) | a block whose writer is unpowered or destroyed reads `Invalid`, never a stale number; the `blk_*` telemetry columns make validity observable per tick |
| Avionics is operated through a command path with acknowledgement, two latency classes and a closed rejection catalogue | a command into a destroyed box acks `rejected/system_failed`; every issue/ack/reject shows up in `events.log` |
| The two judges belong to the client, never to the module | `grep -rn 'FBFlightMonitor\|FBMissionMonitor' sim/src/systems sim/src/modules` stays empty |
| The health register is monotone and writable by exactly one class | every mutator private, single `friend FBDamageModel` — enforced by the compiler, not by convention |
| Damage resolution is deterministic | same geometry → same damage picture, thread-independent (measured) |
| Every number carries its provenance | derived / measured / `[SET]` — see `../conventions.md` |

## State

Built and in service; 53 files plus `math/FBMat4.h`.

| Piece | Status | Anchor |
|---|---|---|
| Avionics block bus (17 blocks) + command bus | built | `071ea2b` |
| `FBLog` / `FBTelemetry`, thread-local context for the gym parallel path | built | `e4d7c26`, `6d7ed5a` |
| `FBFlightMonitor` (physics KO) | built | `28e74e5` |
| `FBMissionMonitor` (mission verdict) | built | `92fe8a4` |
| Objectives, roster, team-capable verdict | built | `82df2e2` |
| `FBSystemHealth` + `FBDamageModel` | built | `6d84647` |
| Gun catalogue, gun ballistics, projectile pool | built | `a1a8fbf` |
| Free-fall ballistics (the shared CCIP/CCRP primitive) | built | `1eeff72` |
| Elevation providers: constant, runway plateau, baked Swiss DEM | built (the tiles provider lives in `world/` and is **not** part of the core lib) | `705c90a` |

Everything below under Knowledge is the distilled per-file detail, every constant with its provenance.

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `core/FBFlightMonitor.h` | banner locates the off-runway verdict in `FBMissionRunner.cpp`; it has lived in `core/FBMissionMonitor::Tick` since the mission monitor exists |
| `core/FBStateBusTelemetry.cpp` | banner counts "the two blocks added afterwards (Rwr, Cmds)" — there are three, `blk_gun` follows the same rule |
| `core/FBDamageModel` | `kMaxZones = 5` is coupled to `FBDamageZone` but **not** compiler-checked; a new zone disappears silently in `AddKinetic`'s range check |
| `core/FBAvionicsBlocks.h` | `FBStoresBlock::Arm` defaults to `Arm`, `FBGunBlock::Arm` to `Sim` — asymmetry without a source, and the armed side is the less conservative one |
| `core/FBFlightPlan` | `FBWaypointType` declares four types, the parser produces two |
| `math/FBMat4.h` | breaks the tree's coding convention (pre-pivot inheritance) |

### Deliberately not modelled (from the retired `TODO.md` §3)

| Thing | Consequence |
|---|---|
| `GroundElevPatch` declared, unimplemented | no terrain following, no CFIT prediction. Parked here because the provider hook is a `core/` type; it moves to the world-side file when `world/` is split. |

### Inventory (German, from the previous `Offene Punkte` section)

Gefundene Lücken, Inkonsistenzen und Fragen — nichts davon ist weggeschrieben oder beschönigt.

1. **CLAUDE.md nennt die Blockliste unvollständig.** Der `core/`-Absatz listet „Platform, Env,
   AirData, RadarAlt, Nav, Cruise, FireControl, Ufc, Stores, Airframe, Warnings, Radar, Datalink,
   Bfm" (14). `FBState.h` trägt heute **17**: zusätzlich `Gun`, `Rwr`, `Cmds`. Der Code ist die
   Wahrheit; CLAUDE.md ist an dieser Stelle veraltet.

2. **Der Banner von `FBStateBusTelemetry.cpp` nennt den Gun-Block nicht.** Er begründet die
   eingefrorene Spaltenliste mit „die zwei Blöcke, die danach hinzukamen (Rwr, Cmds)"; tatsächlich
   sind es **drei** — `FBGunSystem::DeclareTelemetry` deklariert `blk_gun` mit derselben Begründung
   als erste eigene Spalte. Sachlich stimmt alles, nur die Aufzählung im Kommentar ist unvollständig.

3. **Der Banner von `FBFlightMonitor.h` verortet das Off-Runway-Urteil falsch.** Er schreibt, dieses
   Urteil gehöre „dem Aufrufer, der die Mission kennt (`FBMissionRunner.cpp` bewertet es als FAIL,
   eine separate, weiterhin unbestechliche, Runner-eigene Prüfung; siehe deren eigenen Banner)". Die
   Prüfung lebt inzwischen in `core/FBMissionMonitor::Tick` (`OnRunway`, 50 m/30 m Marge). Der
   Kommentar stammt aus der Zeit vor dem Missionsmonitor.

4. **`FBStoresBlock::Arm` und `FBGunBlock::Arm` haben verschiedene Struct-Defaults** — `FBArmState::Arm`
   bzw. `FBArmState::Sim`. Beide werden zur Laufzeit von ihrem jeweiligen System geschrieben, der
   Unterschied ist also vermutlich folgenlos; ob die Asymmetrie beabsichtigt ist, sagt keine Quelle.
   Ein genullter/uninitialisiert publizierter Stores-Block liest als SCHARF, was die weniger
   konservative der beiden Voreinstellungen ist.

5. **`FBFlightPlan` deklariert vier Wegpunkttypen, der Parser erzeugt zwei.** `Takeoff` und `Approach`
   sind in `FBWaypointType` deklariert, aber `FBParseMissionFile` erzeugt ausschließlich `Enroute`
   (`wp`) und `Land` (`land`). Kein Fehler — nur ein noch unbenutzter Teil des Typs; wer die Enum
   liest, sollte das wissen.

6. **`FBNavBlock::MagVarDeg` ist ein Platzhalter (immer 0).** Im Blockkommentar so gekennzeichnet.
   Jede magnetische Peilung, die irgendwo daraus abgeleitet würde, ist heute eine wahre Peilung.

7. **`FBEnvironmentBlock` ist kein Avionikblock**, wird aber wie einer behandelt (eigener
   Gültigkeitskopf, eigener Schreiber = der Client). Im Kommentar ausdrücklich so begründet („nicht
   Avionik im Zellensinn, aber geteilter Pro-Frame-Zustand mit exakt derselben Erzeuger-/
   Konsumentenfrage"). Für einen Leser, der den Bus als Flugzeugsystem-Bus liest, ist das eine
   Überraschung, die hier notiert sei.

8. **`kMaxStoreStations = 12` gegen neun F-16-Pylone.** Der Block reserviert zwölf Stationsslots; die
   F-16 deklariert neun (`modules/f16/FBF16Sms`). Kein Widerspruch (Kapazität ≥ Bedarf), aber die
   überzähligen drei Slots sind unbelegt und tragen dauerhaft 0.

9. **`FBDamageZone` hat fünf Werte inkl. `None`, `FBSystemHealth::kMaxZones` ist 5.** Die Kopplung ist
   im Kommentar benannt (`kMaxZones = 5 /* core/FBDamageModel's FBDamageZone, including None */`),
   aber sie ist NICHT compilergeprüft: ein `static_assert` gegen die Enum-Größe fehlt, und
   `core/FBSystemHealth.h` inkludiert `FBDamageModel.h` bewusst nicht (die Abhängigkeit läuft
   andersherum). Eine neue Zone würde still über den Rand des `Kinetic_`-Arrays zeigen — abgefangen
   nur von der Bereichsprüfung in `AddKinetic`, die den Beitrag dann VERWIRFT.

10. **`FBGunProjectiles` löst nie gegen Gelände auf.** Im Banner erklärt und begründet (Luft-Luft ist,
    wofür der Pool da ist), hier nur als bekannte Grenze festgehalten: es gibt keinen
    Beschuss-Fußabdruck am Boden.

11. **`FBDamageModel::ApplyKinetic` summiert, `Apply` nicht** — bewusst und begründet (Strom vs.
    Ereignis). Konsequenz, die nirgends ausgesprochen wird: **ein Gefechtskopf-Burst profitiert nie
    von vorher eingesteckter kinetischer Energie und umgekehrt.** Die beiden Wirkungen teilen sich das
    Register (die Systemzustände), aber nicht die Energie-Buchhaltung — `Kinetic_` ist rein kinetisch.
    Ob eine Zelle, die schon 50 Kanonentreffer hat, auf einen Splitterburst empfindlicher reagieren
    sollte, ist eine offene Modellfrage.

12. **`FBLog::Unit_` ist auf 32 Zeichen begrenzt, Callsigns auf 24** — passt, aber die Kopplung ist
    nicht als `static_assert` festgehalten; `snprintf` würde still kürzen.

13. **`math/FBMat4.h` folgt der Coding-Konvention des Baums nicht** (freie `static`-C-Funktionen, kein
    `namespace FlightBox`, kein `FB`-Präfix an den Funktionen). Es ist die älteste Datei dieser
    Sammlung und offenkundig Vor-Pivot-Erbe. Kein Defekt, aber ein Stilbruch, den ein künftiger
    Leser sonst für eine Absicht halten könnte.

14. **Die Aufzählung „53 Dateien, ~4.800 Zeilen" der Aufgabenstellung stimmt** (`ls | wc -l` = 53;
    `wc -l` über `core/` + `math/` = 4.927 inkl. `FBMat4.h`).

15. **Nicht in `core/` und deshalb hier nur genannt**: `FBTilesElevation` (der vierte
    Elevation-Provider) liegt in `world/` und ist NICHT Teil der Core-Lib — `fb-gym` linkt ihn nicht.
    Wer die Provider-Liste vollständig lesen will, muss dorthin.


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 0. Die Regel, die `core/` definiert

| Regel | Belegt durch |
|---|---|
| `core/` inkludiert NIE `systems/`, `modules/`, `units/`, `fdm/`, `render/`, `world/`, `app/` | `grep -rn '#include' sim/src/core` findet ausschließlich `"FB*.h"` aus `core/` selbst und Standard-Header (verifiziert) |
| `core/` ist I/O-frei — kein `FILE*`, kein `fstream`, kein `printf` | zwei benannte Ausnahmen, s.u. |
| Formatierung ist erlaubt | `snprintf` in einen lokalen Puffer, überall (`FBLog.cpp`, `FBTelemetry.cpp`) |
| Ereignisse laufen über `FBLog`, periodischer Zustand über `FBTelemetryBus` | kein verstreutes `printf` in `core/systems/modules/render/world/fdm/units` |

**Die I/O-Ausnahmen, beide begründet:**

| Datei | Was sie tut | Warum es keine Verletzung ist |
|---|---|---|
| `core/FBLog.cpp`, `core/FBTelemetry.cpp` | `<cstdio>` für `snprintf` — reine Formatierung in lokale Puffer | kein Dateihandle, kein Stream; die SINKS liegen in `app/` (`FBLogSinks.*`, `FBTelemetrySinks.*`) |
| `core/FBBakedDemElevation.cpp` | `fopen`/`fread` EINES statischen Daten-Assets beim Konstruieren | dieselbe Kategorie, in der JSBSim sein eigenes Modell-XML lädt — Asset-Load, kein Streaming, kein Netz. Im Header explizit so begründet: „core/, not world/". |

#### Datei-Inventar nach Sache

| Thema | Dateien | Abschnitt |
|---|---|---|
| Avionik-Bus (Zustand) | `FBState.h`, `FBAvionicsBlocks.h`, `FBBlockStatus.h`, `FBStateBusTelemetry.h/.cpp` | [1](#1-der-avionik-bus) |
| Avionik-Bus (Kommando) | `FBAvionicsCommand.h`, `FBCommandBus.h/.cpp` | [2](#2-die-kommandoseite) |
| Beobachtungskanäle | `FBLog.h/.cpp`, `FBTelemetry.h/.cpp` | [3](#3-die-zwei-kanäle) |
| Richter | `FBFlightMonitor.h/.cpp`, `FBMissionMonitor.h/.cpp` | [4](#4-die-zwei-richter) |
| Missionsdaten | `FBMissionFile.h/.cpp`, `FBFlightPlan.h`, `FBRunway.h`, `FBSpawn.h`, `FBObjective.h`, `FBTeam.h`, `FBMode.h`, `FBMasterMode.h`, `FBArmState.h` | [5](#5-missionsdaten-als-typen) |
| Schaden | `FBSystemHealth.h/.cpp`, `FBDamageModel.h/.cpp` | [6](#6-schaden) |
| Waffen + Ballistik | `FBStore.h`, `FBBallistics.h/.cpp`, `FBGun.h`, `FBGunBallistics.h/.cpp`, `FBGunProjectiles.h/.cpp`, `FBWeaponUplink.h` | [7](#7-waffen-wertetypen-und-ballistik) |
| Sensor-/EW-Wertetypen | `FBRadarContact.h`, `FBDatalinkTrack.h`, `FBEmitter.h`, `FBRwrThreat.h`, `FBCountermeasure.h` | [8](#8-sensor--und-ew-wertetypen) |
| Elevation-Hook | `FBElevationProvider.h`, `FBConstantElevation.h`, `FBRunwayPlateauElevation.h/.cpp`, `FBBakedDemElevation.h/.cpp` | [9](#9-der-elevation-hook) |
| Basistypen/Mathematik | `FBGeodesy.h`, `FBAtmosphere.h`, `FBUnits.h`, `math/FBMat4.h` | [10](#10-geodäsie-atmosphäre-einheiten-mathematik) |

---

### 1. Der Avionik-Bus

`core/FBState.h` ist der EINE geteilte Pro-Frame-Zustand. Er ist **kein flaches Feldbündel**, sondern
ein Satz typisierter AUSGABEBLÖCKE (`core/FBAvionicsBlocks.h`), jeder mit einem Gültigkeitskopf
(`core/FBBlockStatus.h`).

**Warum Blöcke** (`FBState.h`-Banner): die flache Version konnte „dieser Wert gilt gerade nicht" nicht
ausdrücken — und genau das ist bei Schaden/Ausfall das Erste, was ein Display und ein KI-Pilot wissen
müssen. Sie hatte außerdem keine compilerprüfbare Antwort auf „wer hat dieses Feld geschrieben": eine
Wartungs-Prüfung fand **zehn tote Felder und vier, die gelesen, aber nie geschrieben wurden**.

**Die EINE Regel**: jeder Block hat GENAU EINEN Schreiber (im Blockkommentar namentlich benannt) und
beliebig viele Leser. Kein Leser schreibt, kein Block hat zwei Schreiber.

**`NowS`** ist die Zeitreferenz des Busses: das Modul stempelt sie einmal pro `Run()` aus seiner
Sim-Uhr, BEVOR es irgendeinen Slot taktet; jeder Blockkopf-Zeitstempel kommt daraus. Eine Uhr für den
ganzen Bus ist das, was „wie alt ist dieser Block" beantwortbar macht, ohne dass jedes System ein
eigenes Jetzt führt.

**Was übernommen ist und was nicht** (`FBAvionicsBlocks.h`-Banner, Modell MIL-STD-1553): übernommen
ist die SEMANTIK — definierte Datengruppen, ein Erzeuger, ein Gültigkeitsflag. NICHT übernommen:
Bus-Adressierung, Wortpackung, Message-Scheduling. Der Transport ist eine typisierte Struktur per
Referenz in EINEM Adressraum; Remote-Terminal-Adressen dafür zu erfinden wäre Cargo-Kult.

#### 1.1 `FBBlockStatus` — die Dreizustands-Gültigkeit

`core/FBBlockStatus.h`. Drei Zustände, nicht zwei, und der dritte ist BELEGT statt erfunden.

| Zustand | Bedeutung | Konsument tut |
|---|---|---|
| `Invalid` (0) | Die Zahlen bedeuten nichts. Nie geschrieben, oder das Quellsystem ist aus/ausgefallen. | Anzeige entrümpeln/stricheln |
| `Valid` (1) | Von seinem einen Schreiber zum Zeitpunkt `StampS` geschrieben, dort aktuell. | normal lesen |
| `Held` (2) | Der Schreiber hat das Aktualisieren ABSICHTLICH eingestellt. Die Felder tragen weiter die letzten guten Werte, `StampS` weiter den Zeitpunkt der letzten ECHTEN Aktualisierung. | letzten guten Wert weiter zeigen, Alter beachten |

**Die Herleitung von `Held`**: DCS/ED modellieren mehrere berechnete CRUS-Seiten-Felder als
FREEZE-AT-LAST-VALUE, sobald das Fahrwerk unten ist — sie hören auf zu aktualisieren, sie werden nicht
leer (`doc/f16/controls-commands.md`, „The DED's propose -> commit/reject protocol"). Ein
ausgefallenes und ein absichtlich eingefrorenes System sind verschiedene Tatsachen, und ein Konsument
reagiert verschieden darauf (Cue löschen vs. letzte gute Zahl stehenlassen).

**`StampS` bewegt sich bei `Hold()` NICHT.** Genau das macht „wie alt ist diese gehaltene Zahl" zu
einer beantwortbaren Frage.

| Methode | Vertrag |
|---|---|
| `Publish(nowS)` | `StampS = nowS`, Status → `Valid` |
| `Hold()` | `Valid` → `Held`; ein nie publizierter Block hat nichts zu halten und bleibt `Invalid` |
| `Invalidate()` | Status → `Invalid` |
| `Readable()` | `Status != Invalid` — die übliche Frage: darf ich die Zahlen überhaupt lesen? `Valid` UND `Held` sagen ja |
| `AgeS(nowS)` | `nowS - StampS` |

#### 1.2 Die Blöcke und ihre Schreiber

`FBState` trägt heute **17** Blöcke. Reihenfolge = Deklarationsreihenfolge in `FBState.h`.

| Block | Schreiber (der EINE) | Inhalt |
|---|---|---|
| `FBPlatformBlock Platform` | der Besitzer des FDM-Zustands: das Modul publiziert aus dem `st`, das es je `Run()` bekommt; der Client publiziert dieselbe Struktur mit der Live-Pose des Frames neu (`units/FBSimUnit::HudState`) | Roll/Pitch/Yaw, `AltM` (ASL geodätisch), ENU-Versatz vom Sim-Ursprung, GS/TAS/VS, Home-Distanz/-Peilung (relativ zur Nase, −180…180), `FBMode Mode` (der ECHTE, bestätigte Guidance-Modus) |
| `FBEnvironmentBlock Env` | der Client (Ephemeriden + Live-Wetter), nie ein Modulsystem | Wolkendeckung total + 3 Schichten (0..1), Wolkenbasis AGL (0 = unbekannt), Sonnen-/Mond-El/Az, Mondphase (beleuchteter Anteil) |
| `FBAirDataBlock AirData` | `systems/FBAirDataSystem` | CAS, Mach, `GLoad` + laufendes `GLoadPeak` seit Boot, `TrackDeg` (Ground-Track true 0..360), `FpaDeg` (Flugbahnwinkel, + = steigend) |
| `FBRadarAltBlock RadarAlt` | `systems/FBRadarAltimeter` | `AglFt` |
| `FBNavBlock Nav` | `systems/FBNavSystem` | Steerpoint-Peilung/-Elevationswinkel/-Distanz(nm)/-Elevation(ft ASL), Bullseye-Peilung (VOM Bullseye ZUM Flugzeug)/-Distanz, `MagVarDeg` (Platzhalter 0) |
| `FBCruiseBlock Cruise` | `systems/FBNavSystem` (ein Quellsystem darf mehr als eine Nachricht publizieren) | `SteerTtgS` |
| `FBFireControlBlock FireControl` | `modules/f16/FBF16FireControl` | vier Produkte, s. [1.3](#13-der-firecontrol-block--vier-produkte-unter-einem-kopf) |
| `FBUfcBlock Ufc` | `modules/f16/FBF16Ufc` | `AlowFt`, `BingoLbs` + `BingoEffectiveLbs`, `SteerNum` |
| `FBStoresBlock Stores` | `systems/FBStoresSystem` (F-16 füllt den Slot mit `modules/f16/FBF16Sms`, das NUR Pylon-Geometrie beisteuert) | `FBArmState Arm`, Stationszahl, gewählte Station (1-basiert, −1 = keine), `Station[12]` = `FBStoreKind`-Ordinal je Station (0 = leer), geladene Anzahl/Gewicht, `ReleasedCount` |
| `FBGunBlock Gun` | `systems/FBGunSystem` (F-16: `modules/f16/FBF16Gun`) | `FBArmState Arm`, `Kind` (FBGunKind-Ordinal, 0 = keine Kanone), Restschuss, verschossen, `Firing`, `Ready` |
| `FBAirframeBlock Airframe` | `systems/FBAirframeControls` (über das Modul, dem das FDM-Handle gehört) | `GearPosition` 0..1 kinematisch verzögert, `WeightOnWheels`, `SpeedbrakeNorm`, `FuelLbs`/`FuelPct`, `EngineRunning` |
| `FBWarningBlock Warnings` | `systems/FBWarningSystem` | `Active`- und `Inhibited`-Bitmaske |
| `FBRadarBlock Radar` | `systems/FBRadarSystem` | `Radiating`, `ModeOrdinal` (Modul-eigenes Label, keine Logik), Kontaktzahl, `LockIndex` (−1 = kein Lock), `IffTransponder`, `FBRadarContact Contacts[8]` |
| `FBRwrBlock Rwr` | `systems/FBRwrSystem` | `Powered`, Bedrohungszahl, `PriorityIndex` (−1 = keine), `MissileLaunch`, `Activity`, `HiddenSearch`, `FBRwrThreat Threats[8]` |
| `FBCmdsBlock Cmds` | `systems/FBCountermeasureSystem` | `FBCmdsMode`, `FBCmdsStatus`, gewähltes Programm, Chaff-/Flare-Rest, `ChaffLow`/`FlareLow` („LO"-Lampe), `Dispensing`, verbrauchte Zahlen, `ActiveClouds` |
| `FBDatalinkBlock Datalink` | `systems/FBDatalinkSystem` | `Powered`, `Transmitting`, Trackzahl, `FBDatalinkTrack Tracks[8]` |
| `FBBfmBlock Bfm` | `systems/FBBfmTrack` (vom Modul nach dem Piloten-Entscheidungstakt auf den Bus publiziert) | `Locked`, Range, Az/El (körperbezogen = ATA), Closure, Aspekt (am ZIEL: 0 = wir sitzen ihm im Heck, 180 = head-on), HCA, geschätzter ENU-Versatz + geschätzter Zielgeschwindigkeitsvektor |

**Wo `Held` der NORMALFALL ist, nicht die Ausnahme:**

| Block | Warum |
|---|---|
| `Radar` | Zwischen zwei Antennen-Frames steht die Geometrie still, nur das Kontaktalter läuft — genau Freeze-at-last-value. Der Kopf sagt `Held`, bis der nächste abgeschlossene Sweep neu publiziert. |
| `Datalink` | Dasselbe von der anderen Seite: das Netz frischt einmal pro Zyklus auf, dazwischen steht das Bild. |
| `Rwr` | Eine Bedrohung, deren Emission nicht mehr gehört wird, wird kurz weitergetragen, bevor sie fällt; solange nichts Neues empfangen wird, steht das Bild und nur das Alter läuft. |
| `Cruise` | Fahrwerk unten → die berechneten CRUS-Felder frieren ein, während Peilung/Distanz weiterlaufen. Der Ursprung des dritten Zustands. |
| `Bfm` | Der Kopf trägt die drei Zustände der Fusion wörtlich: `Invalid` = nie gesehen, `Valid` = frisch oder im glaubwürdigen Extrapolationsfenster, `Held` = jenseits davon, wo die Schätzung auf die letzte GEMESSENE Position zurückfällt und nichts mehr ist, worauf man vorhält. **`StampS` ist der LOOK**, auf dem die Schätzung steht, nicht die Publikationszeit. |

**Der Referenzfall für `Invalid`** ist `RadarAlt`: die CARA ist eine bestromte Box, und
`doc/f16/controls-commands.md` §6.4 nennt die Folge wörtlich — die ALOW-Warnung feuert nur mit
bestromtem und sendendem Radarhöhenmesser, so bereitwillig das DED die Schwelle auch angenommen hat.
Stromlos publiziert die Box keine 0 ft; sie macht ihren Block ungültig, und jeder Konsument muss
sagen, was er ohne sie tut.

**Dokumentierte Fusion (kein zufälliges Koppeln)** — die drei Stellen, an denen ein System einen
fremden Block liest, um seine eigene Ausgabe abzuleiten, sind im jeweiligen Blockkommentar benannt:
Fire Control liest Nav + Platform; Warnings liest RadarAlt, UFC und Airframe; der Cruise-Freeze wird
vom Gear-Signal des Airframe-Blocks getrieben.

#### 1.3 Der FireControl-Block — vier Produkte unter einem Kopf

Alle vier teilen sich den Kopf, weil alle vier Ausgabe DERSELBEN Box sind und alle gemeinsam ungültig
werden, wenn die Quellen es tun, die sie fusionieren. Jedes trägt zusätzlich ein eigenes
„gibt es eine LÖSUNG"-Bit — eine Feuerleitung ohne Ziel publiziert ihren Block trotzdem, und das ist
eine andere Tatsache als ein nicht publizierter Block. **Der Bus ist SI** (Meter/Sekunden); nur
Displays rechnen nach nm um.

**(1) Entfernungsmessung**: `SteerSlantNm`, `RangeProvider` (Buchstabe neben der Zahl, F-16: `'B'` =
baro/Steerpoint-Elevation).

**(2) Der Luft-Luft-Startbereich (DLZ)** — `doc/f16/weapons.md` §2.5:

| Feld | Bedeutung |
|---|---|
| `DlzValid` | gelocktes Ziel UND eine gewählte Waffe, die einen Startbereich hat |
| `TargetRangeM` | Schrägentfernung des gelockten Ziels — das, was die Grenzen klammern |
| `ClosureMs` | + = annähernd |
| `RaeroM` | kinematische Maximalreichweite gegen ein NICHT manövrierendes Ziel |
| `RtrM` | turn-and-run: ein Treffer, selbst wenn das Ziel im Startmoment abdreht |
| `RminM` | kleinste Entfernung, die Scharfwerden + Endphasen-Homing noch erlaubt |
| `TimeToActiveS` | vorhergesagte Sekunden vom Start bis zum Aktivwerden des Suchkopfs |
| `TimeToImpactS` | vorhergesagte Gesamtflugzeit; < 0 = von hier kein Abfangen |
| `InZone` | `Rmin <= range <= Raero` — was die Start-Verriegelung des SMS liest |

**(3) Die Kanonenlösung (EEGS)**: Der EEGS-Trichter ist ein VISIER — seine Wände sind die bekannte
Spannweite des Ziels, gezeichnet auf der Entfernung, für die die Kanone korrekt vorgehalten ist. Ein
Ziel, das den Trichter füllt, steht auf der Entfernung, für die der Vorhalt gerechnet wurde. Was diese
Geometrie über einen SCHUSS sagt, sind drei Zahlen:

| Feld | Bedeutung |
|---|---|
| `GunValid` | es gibt eine Lösung |
| `GunRangeM` | zum vorhergesagten SCHNITTPUNKT, nicht zum Ziel jetzt |
| `GunTofS` | Geschossflugzeit dorthin |
| `GunAimErrorDeg` | Winkel zwischen der geforderten Rohrrichtung und der Nase |
| `GunLeadAzDeg`/`GunLeadElDeg` | die geforderte Rohrrichtung, körperbezogen (+ = rechts/oben) |
| `GunSpreadM` | Sigma des Streumusters am Schnittpunkt |
| `GunSpanMr` | Winkelspanne des Ziels dort, Milliradiant |
| `GunFunnelTopMr` | seine Spanne an der MINIMAL-Entfernung des Trichters (das enge Ende) |
| `GunFunnelBottomMr` | …und an der Maximal-Entfernung |
| `GunTolDeg` | die eigene Zieltoleranz des Trichters auf dieser Entfernung |
| `GunInRange` | innerhalb des Entfernungsfensters des Trichters |
| `GunInFunnel` | in Reichweite UND die Nase innerhalb der Toleranz der Vorhaltelösung |

Der Außer-Reichweite-Test des Guides („Ziel kleiner als der Trichterboden") ist damit wörtlich
`GunSpanMr < GunFunnelBottomMr`.

**(4) Die Luft-Boden-Abwurflösung (CCIP/CCRP)**: EINE Integration (`core/FBBallistics.h`), zwei
Fragen. Der AUFSCHLAGPUNKT ist das CCIP-Pipper; die drei FEHLER sind dieser Punkt gemessen gegen den
designierten Zielpunkt entlang des aktuellen Bodenkurses, was der CCRP-Solution-Cue herunterzählt. Ein
Konsument wählt den Modus dadurch, WELCHE Zahlen er liest — er verlangt nicht, dass die Box in einem
Modus ist.

| Feld | Bedeutung |
|---|---|
| `AgValid` | es gibt eine Lösung |
| `AgImpactLatDeg`/`AgImpactLonDeg` | **die einzigen geodätischen `double` auf diesem Bus**: bei 1e-5° ist ein `float` ein Meter, und der Meter ist die Größe, in der die ganze Lösung gemessen wird |
| `AgImpactElevM` | die Ebene, gegen die gelöst wurde (die der Entfernungsquelle), m ASL |
| `AgTofS` | Fallzeit bei Auslösung jetzt |
| `AgRangeM` | horizontal Flugzeug → Aufschlagpunkt: die Eigenreichweite der Bombe |
| `AgAlongErrM` | + = die Waffe fällt ZU KURZ; 0 = jetzt auslösen |
| `AgCrossErrM` | + = sie fällt RECHTS daneben — der Steuerlinienfehler |
| `AgMissM` | beide zusammen: Abstand des Pippers vom Zielpunkt |
| `AgTimeToReleaseS` | `AgAlongErrM` bei aktueller Bodengeschwindigkeit; <= 0 = der Cue ist durch |
| `AgArmMarginS` | Fallzeit, die übrig bleibt, NACHDEM die Schärfverzögerung des Zünders abgelaufen ist; < 0 = ein Blindgänger |
| `AgInRange` | beide Hälften der Freigabe-Bedingung des Guides als das eine Bit, auf dem entschieden wird: der Zielpunkt ist noch VOR dem aktuellen Aufschlagpunkt (der Auslösemoment ist nicht vorbei) UND eine Auslösung jetzt würde vor Ankunft scharf |

#### 1.4 Der UFC-Block trägt ZWEI Bingo-Zahlen

`doc/f16/controls-commands.md` §6.8: das DED-Feld zeigt, was der Pilot GETIPPT hat; die Warnung feuert
an der System-Obergrenze. Displays lesen `BingoLbs`, das Warnsystem `BingoEffectiveLbs` — die beiden
zusammenzulegen hätte den dokumentierten Clamp unsichtbar gemacht.

#### 1.5 Der Warnungs-Block ist eine Bitmaske

`FBWarningBit`: `FBWarnAlow` (1<<0, unter der CARA-ALOW-Schwelle), `FBWarnBingo` (1<<1, Sprit auf oder
unter der bestätigten BNGO-Schwelle), `FBWarnGearUnsafe` (1<<2, auf den Rädern ohne down-and-locked).
Eine Bitmaske, damit EIN Block das ganze Annunciator-Panel trägt, ohne pro Lampe ein Feld zu wachsen.

**`Inhibited` ist keine Verzierung**: Bedingungen, deren QUELLBLOCK `Invalid` ist, können nicht
ausgewertet werden — und das ist eine andere Tatsache als „warnt nicht".

#### 1.6 `FBStateBusTelemetry` — Gültigkeit als messbare Zeitreihe

`core/FBStateBusTelemetry.h/.cpp`. Eine Telemetrie-Quelle namens `"blk"`, die je Block dessen
`FBBlockStatus`-Ordinal ausgibt (0/1/2).

**Warum**: der Dreizustandskopf ist nur etwas wert, wenn er PRÜFBAR ist — und die Werte selbst können
ihn nicht zeigen, denn ein `Held`-Block trägt dieselben Zahlen wie ein `Valid`-Block. Ohne diese
Quelle sähen „das Datalink-Bild ist zwischen zwei Netzzyklen eingefroren" und „der Radarhöhenmesser
ist tot" beide wie gewöhnliche Daten in einer CSV aus. Mit ihr ist beides EINE Spalte.

**Die Spaltenliste ist eingefroren — als REGEL, nicht als Versäumnis.** Sie deklariert 14 Namen
(`blk_platform`, `blk_env`, `blk_airdata`, `blk_radalt`, `blk_nav`, `blk_cruise`, `blk_firecontrol`,
`blk_ufc`, `blk_stores`, `blk_airframe`, `blk_warn`, `blk_radar`, `blk_datalink`, `blk_bfm`). Diese
Quelle sitzt in der MITTE jeder je gemessenen `telemetry.csv`; ein Name mehr würde jede Spalte rechts
davon verschieben. Später hinzugekommene Blöcke melden ihre eigene Gültigkeit deshalb als ERSTE Spalte
ihrer eigenen Telemetrie-Quelle, die der Bus am ENDE registriert:

| Block | Wo seine Gültigkeit steht |
|---|---|
| `Rwr` | `blk_rwr`, erste Spalte von `systems/FBRwrSystem`s Quelle `"rwr"` |
| `Cmds` | `blk_cmds`, erste Spalte von `systems/FBCountermeasureSystem`s Quelle `"cm"` |
| `Gun` | `blk_gun`, erste Spalte von `systems/FBGunSystem`s Quelle `"gun"` |

Ein Block, dessen Kopf nicht in dieser Liste steht, ist nicht unbeobachtbar; er wird eine Spalte
weiter rechts beobachtet.

---

### 2. Die Kommandoseite

`core/FBAvionicsCommand.h` ist das VOKABULAR, `core/FBCommandBus.h/.cpp` der WEG. Die Form ist dem
dokumentierten propose→commit/reject-Protokoll des DED entnommen (`doc/f16/controls-commands.md`,
„The DED's propose -> commit/reject protocol"): ein Kommando ist `{Ziel, Vorschlagswert}`, die
Quittung ist `{committed, Grund}`. Nichts in `FBAvionicsCommand.h` führt etwas aus — das BESITZENDE
System tut das in seinem eigenen Takt und antwortet.

**Warum überhaupt ein Kommandopfad, wenn die KI den Setter direkt rufen könnte** (Banner): weil ein
Setter-Aufruf keine Pilotenhandlung ist. Er kann nicht abgelehnt werden, er kann keine Zeit kosten, er
kann nicht stattfinden, während beide Hände fliegen, und er hinterlässt keine Spur. Alle vier sind
echte Eigenschaften des Cockpits, und alle vier führt dieses Modell wieder ein — eine KI, die den Jet
über dasselbe Vokabular steuert wie ein Mensch, ist eine KI, deren Vorteil Entscheidungsqualität ist,
nicht Zugriff.

#### 2.1 Die Ziele

`FBCommandTarget` — Ordinale sind telemetriesichtbar: **anhängen, nie umsortieren**. Klasse und Gruppe
werden aus `FBCommandClassOf`/`FBCommandGroupOf` (`FBCommandBus.cpp`) abgeleitet, nicht am Ziel
gespeichert.

| Ziel | Log-/Telemetrie-Name | Klasse | Gruppe | Wert-Semantik |
|---|---|---|---|---|
| `RadarMode` | `radar_mode` | HOTAS | Sensors | Modus-Ordinal |
| `RadarRangeNm` | `radar_range_nm` | **DED** | Sensors | nm (getippt) |
| `RadarSlewAz` | `radar_slew_az` | HOTAS | Sensors | deg |
| `RadarSlewEl` | `radar_slew_el` | HOTAS | Sensors | deg |
| `IffTransponder` | `iff_xpdr` | HOTAS | Sensors | 0/1 |
| `IffInterrogator` | `iff_interrogator` | HOTAS | Sensors | 0/1 |
| `DatalinkPower` | `datalink_power` | HOTAS | Comms | 0/1 |
| `DatalinkTransmit` | `datalink_xmt` | HOTAS | Comms | 0/1 (EMCON) |
| `DatalinkFilter` | `datalink_filter` | HOTAS | Comms | Filter-Ordinal |
| `DatalinkRangeNm` | `datalink_range_nm` | **DED** | Comms | nm |
| `MasterMode` | `master_mode` | HOTAS | Avionics | `FBMasterMode`-Ordinal |
| `MasterArm` | `master_arm` | HOTAS | Avionics | `FBArmState`-Ordinal |
| `AlowFt` | `alow_ft` | **DED** | Avionics | ft |
| `BingoLbs` | `bingo_lbs` | **DED** | Avionics | lbs |
| `SteerpointNum` | `steerpoint` | **DED** | Avionics | Nummer |
| `WeaponSelect` | `weapon_select` | HOTAS | Avionics | Auswahl-Ordinal |
| `Designate` | `designate` | HOTAS | Avionics | veröffentlichte Tracknummer; **0 = Lock lösen** |
| `StationSelect` | `station_select` | HOTAS | Stores | Stationsnummer |
| `WeaponRelease` | `weapon_release` | HOTAS | Stores | Pickle |
| `CmDispense` | `cm_dispense` | HOTAS | Defensive | **0 = das per PRGM-Knopf gewählte Programm, 1..6 = ein Programm direkt** |
| `CmConsent` | `cm_consent` | HOTAS | Defensive | CMS Aft/Right: SEMI/AUTO-Zustimmung |
| `CmdsMode` | `cmds_mode` | **DED** | Defensive | `FBCmdsMode`-Ordinal |
| `GunTrigger` | `gun_trigger` | HOTAS | Stores | **Dauer des Abzugsdrucks in Sekunden** |

**Warum `CmdsMode` DED ist und `CmDispense`/`CmConsent` nicht**: der CMDS-Modusknopf sitzt auf der
linken Auxiliary-Konsole, nicht am Stick — eine Hand geht vom Gashebel und der Kopf nach unten
(`doc/f16/defence-rwr-cm.md` §2.2). `CmDispense`/`CmConsent` sind der CMS-Schalter und bleiben HOTAS:
der ganze Sinn einer Gegenmaßnahme ist, dass sie MITTEN im Manöver geworfen werden kann.

**Warum die Triggerdauer der WERT ist** (nicht ein Strom aus Press/Release-Kommandos): ein Kommando
modelliert EINE Handlung, und eine Handlung am Abzug ist ein Feuerstoß erklärter Länge. Ein
Press/Release-Paar könnte der 0,5-s-Boden des Busses zwischen zwei Aktionen am selben Schalter nie
tragen.

**Warum die Stores-Gruppe die Kanone enthält**: eine Kanone ist kein SMS, aber sie ist Bewaffnung und
wird im selben Slot-Takt beantwortet — eine Gruppe für „die Dinge, die dieses Flugzeug tödlich
machen", damit ein Abzug im Takt der Box wirkt, die feuert.

#### 2.2 Die zwei Latenzklassen

`FBCommandClass` — `doc/f16/controls-commands.md` §5 ist das einzige quantitative Timing-Material der
Quellen.

| Klasse | Was es ist | Eigenschaften |
|---|---|---|
| `Hotas` | ein Druck oder ein Schalterwurf | Sub-Sekunde, MITTEN im Manöver nutzbar; die Avionik selbst nutzt 0,5-s- und 1,0-s-Haltezeiten, um zwei Kommandos am selben Schalter zu unterscheiden |
| `Ded` | Feld auswählen → tippen → ENTR | mehrere Sekunden, Kopf UNTEN, Hände weg — die Klasse Dinge, die ein Pilot ZWISCHEN Manöverabschnitten tut, nie während eines |

Beides als eine Klasse zu modellieren würde einer KI erlauben, bei 7 g einen Steerpoint zu tippen —
und genau das ist es, was dieser Split verbietet.

#### 2.3 Ergebnisse

`FBCommandOutcome` — vier Ausgänge, weil die Quellen vier unterscheidbare Enden dokumentieren:

| Ergebnis | Bedeutung |
|---|---|
| `Pending` | der Schwebezustand zwischen `Post()` und der Antwort des besitzenden Systems |
| `Accepted` | bestätigt, Wirkung live |
| `Clamped` | bestätigt, das Feld zeigt das Getippte, aber eine Systemobergrenze regiert die WIRKUNG (§6.8, die BNGO-Grenze). **Keine Ablehnung: ENTR war erfolgreich.** |
| `Inhibited` | bestätigt, die Wirkung ist durch etwas anderes gesperrt (§6.4, ALOW ohne bestromten Radarhöhenmesser) |
| `Rejected` | nicht bestätigt; `Reason` sagt warum |

#### 2.4 Der vollständige Ablehnungs-/Grundkatalog

`FBCommandReason`. Die **ersten acht** sind die acht dokumentierten Ablehnungs-/Vorbedingungsmuster
aus `doc/f16/controls-commands.md` §6, eins zu eins und in dessen Reihenfolge. Die **letzten vier**
sind FlightBox' EIGENE und sind als solche gekennzeichnet, weil die Quellen keine davon dokumentieren.

| Grund | Log-Name | Herkunft | Bedeutung |
|---|---|---|---|
| `None` | `none` | — | — |
| `PilotReject` | `pilot_reject` | §6.1 | RCL/RTN — der Pilot hat es sich anders überlegt |
| `HardwarePrecedence` | `hardware_precedence` | §6.2 | eine physische Schalterstellung sperrt den Softwarepfad aus |
| `SequencePrecondition` | `sequence_precondition` | §6.3 | eine Zustandsmaschinen-Reihenfolge (ein Roll-AP-Modus braucht zuerst einen Pitch-Modus) |
| `EffectPrecondition` | `effect_precondition` | §6.4 | angenommen, aber die WIRKUNG braucht etwas anderes (ALOW/Radalt) |
| `OutOfContext` | `out_of_context` | §6.5 | der stille No-Op: gültiges Kommando, falsches SOI/falscher Modus |
| `NotImplemented` | `not_implemented` | §6.6 | die Box existiert im Jet, aber (noch) nicht in diesem Simulator |
| `SoftFailure` | `soft_failure` | §6.7 | erfolgreich, aber mit korruptem Zustand (DTE-MPD-Upload mit CMDS nicht STBY) |
| `ValueClamped` | `value_clamped` | §6.8 | angenommen, Systemobergrenze regiert |
| `OutOfRange` | `out_of_range` | **FlightBox** | §6 schließt mit der Feststellung, dass die Quellen KEINE Bereichsprüfungs-Politik dokumentieren („a FlightBox command-block model will need to invent its own range-validation policy"). FlightBox LEHNT AB und sagt es, statt still zu klemmen; der eine dokumentierte Clamp (BNGO) ist `Clamped`, also ist Schweigen nie ein Ausgang. |
| `ChannelBusy` | `channel_busy` | **FlightBox** | die Antwort des Latenzmodells: Hände/Kopf des Piloten sind schon mit einem unabgeschlossenen Kommando derselben Klasse belegt. Aus §5s Druckdauer-Obergrenze ABGELEITET, von keinem Guide behauptet. |
| `Depleted` | `depleted` | **FlightBox** | die Box ist willig und das Kommando gültig, aber das Magazin dahinter ist leer. Getrennt von `OutOfContext`, weil ein leerer Werfer eine Tatsache über das FLUGZEUG ist, die der Pilot anders hören muss — und es ist die eine Ablehnung, die ein Defensivsystem MITTEN im Beschossenwerden erzeugt. |
| `SystemFailed` | `system_failed` | **FlightBox** | die adressierte Box ist WEG — abgeschossen, nicht abgeschaltet (`core/FBSystemHealth`). Eigener Grund, weil es weder ein Kontextfehler noch eine erfüllbare Vorbedingung ist: nichts an der Konfiguration des Flugzeugs bringt dieses Kommando zurück, und ein Cockpit, das einem zerstörten Radar „falscher Modus" antwortete, schickte seinen Piloten einen Schalter suchen, der nichts mehr tut. |

#### 2.5 Die Wertetypen

```
FBAvionicsCommand { Seq, Target, Value(double), IssuedS, DueS }
FBCommandAck      { Seq, Target, Value, Outcome, Reason, CompletedS }  // Committed() = nicht Rejected/Pending
```

`Value` trägt JEDE Nutzlast als `double` — Enum-Auswahlen reisen als ihr eigenes Ordinal, Booleans als
0/1. Eine Warteschlange mit Varianten-Nutzlast bräuchte an jedem Sprung ein Tag und ein `switch`, ohne
Gewinn: das ZIEL sagt bereits, wie die Zahl zu lesen ist. Die Quittung ist vom Kommando GETRENNT,
damit ein Konsument das Paar loggen/telemetrieren kann, ohne dass der Warteschlangeneintrag
überleben muss.

#### 2.6 `FBCommandBus` — was der Bus selbst erzwingt

Besessen vom MODUL (wie der Zustandsbus, den er spiegelt). Der Pilot POSTET, das Modul reicht jedes
fällige Kommando an das System weiter, dem es gehört — in DESSEN Takt —, und dieses System
COMPLETED es. Feste Kapazität, keine Allokation, kein Ownership.

**Die drei Regeln, bevor irgendein System ein Kommando sieht:**

| Regel | Mechanik | Ablehnungsgrund |
|---|---|---|
| **LATENZ** | ein Kommando ist vor `IssuedS + LatencyS(Target)` nicht konsumierbar. Nichts, was eine KI tut, trifft schneller ein, als eine Hand sich bewegen kann. | — (es wartet) |
| **BELEGUNG** | EINE DED-Eingabe zur Zeit (ein Pilot hat einen Kopf); derselbe HOTAS-Schalter kann innerhalb eines Druckdauer-Fensters nicht zweimal bedient werden | `ChannelBusy` |
| **MANÖVER-SPERRE** | eine DED-Eingabe ist Kopf-unten-, Hände-weg-Arbeit; oberhalb `kDedMaxG` fliegt der Pilot den Jet, statt zu tippen | `SequencePrecondition` |

Alles andere — ergibt dieser Wert Sinn, ist diese Box überhaupt verbaut — ist die Antwort des
BESITZENDEN Systems, weil dort das Wissen sitzt.

**Die Konstanten, mit ihrer Herleitung:**

| Konstante | Wert | Herleitung |
|---|---|---|
| `kHotasLatencyS` | **0,5 s** | der dokumentierte Kurz-/Lang-Druck-Diskriminator (§5): die Avionik selbst benutzt 0,5 s, um zwei Kommandos an einem Schalter zu unterscheiden — also ist das der Boden für die Wirkung einer HOTAS-Aktion und für die Wiederbenutzung desselben Schalters. |
| `kDedLatencyS` | **4,0 s** | **FlightBox' eigene Zahl, abgeleitet statt zitiert.** §5 sagt, eine DED-Feldeingabe sei „realistically several seconds per field (select field, type digits, press ENTR)" und nennt keine Zahl. Vier Sekunden ist die Mitte von „several" und — der Punkt — eine Größenordnung über der HOTAS-Klasse, was die Eigenschaft ist, die das Modell braucht. |
| `kDedMaxG` | **1,5 g** | **FlightBox' eigene Zahl**: kein Guide nennt ein g-Limit für Dateneingabe. 1,5 g ist eine Spur über Horizontalflug — genug, dass eine sanfte Reisekurve das DED nicht sperrt, niedrig genug, dass alles, was als Manövrieren erkennbar ist, es tut. |
| `kTriggerLatencyS` | **0,1 s** | **Die eine HOTAS-Aktion, deren Latenz KEINE Druckdauer ist.** `kHotasLatencyS` ist die Zahl, mit der die Avionik einen kurzen von einem langen Druck an einem MODUS-Schalter unterscheidet — sie ist, wie lange das SYSTEM wartet, bevor es entscheidet, was der Pilot meinte. Auf den Abzug angewandt hieße sie, dass Geschosse eine halbe Sekunde nach Fingerschluss das Rohr verlassen. Tun sie nicht: die Verzögerung zwischen Drücken und erstem Schuss ist der Hochlauf der Kanone, und der ist modelliert, wo er hingehört (`core/FBGun.h`s `SpoolUpS`, 0,3 s). Übrig bleibt der FINGER, und 0,1 s ist die menschliche Betätigungszeit. **Es zählt**: bei den Nachführraten eines Jägers bewegt sich die Ziellösung in einer halben Sekunde etwa ein Grad, was auf Kanonenentfernung fünf Meter Fehlschuss sind — der Unterschied zwischen einem treffenden und einem nicht treffenden Feuerstoß (in beide Richtungen gemessen). |
| `kMaxPending` | **8** | feste Queue-Kapazität; ein Überlauf wird mit `ChannelBusy` abgelehnt |
| `kTargetSlots` | **32** | flaches Array „letzter Abschluss je Ziel-Ordinal", O(1) und allokationsfrei; `static_assert` erzwingt Erweiterung bei neuen Zielen |

Der ABSTAND zwischen zwei Abzugsbetätigungen bleibt `kHotasLatencyS` wie bei jedem anderen Schalter:
ein Finger kann schnell ziehen, aber nicht zweimal im selben Augenblick.

**Die API:**

| Methode | Vertrag |
|---|---|
| `Post(target, value, nowS)` | das eine Verb des Piloten. Liefert die Quittung, wie sie JETZT steht: `Pending`, wenn das Kommando in die Queue kam, oder ein finales `Rejected`, wenn der Bus selbst es abgelehnt hat |
| `TakeDue(group, nowS, out)` | die Modulseite: das nächste fällige Kommando dieser Gruppe herausgeben. Ordnungserhaltende Entnahme; `false`, wenn nichts fällig ist |
| `Complete(cmd, outcome, reason, nowS)` | die Antwort des besitzenden Systems. Setzt zugleich das Fenster „dieser Schalter wurde gerade bedient" |
| `SetLoadFactor(g)` | der Manöverzustand, den die DED-Sperre liest — vom Modul aus dem AirData-Block publiziert |

**Er ist zugleich der REKORDER des Kommandostroms** — `FBTelemetrySource "cmd"` und
`FBLog`-Quelle `cmd`:

- Log-Ereignisse: `CMD_ISSUE` (seq/target/value/class/dueS), `CMD_ACK`
  (seq/target/value/outcome/reason/latencyS), `CMD_REJECT` (seq/target/value/reason).
- Telemetriespalten: `cmd_issued`, `cmd_accepted`, `cmd_rejected`, `cmd_clamped`, `cmd_inhibited`,
  `cmd_pending`, `cmd_last`, `cmd_last_outcome`, `cmd_last_reason`.

Damit zeigt ein Gym-Lauf, WAS die KI bedient hat, lange bevor es ein Display gibt, dem man zusehen
könnte. `Clamped` und `Inhibited` zählen zusätzlich als `Accepted` (sie SIND bestätigt).

---

### 3. Die zwei Kanäle

Die Trennung ist scharf und in beiden Bannern gegenseitig referenziert:
**Log = diskrete Ereignisse, Telemetrie = periodisch gesampelter Zustand.**

#### 3.1 `FBLog` — diskrete, greppbare Ereignisse

`core/FBLog.h/.cpp`. Eine STATISCHE FASSADE, kein besessenes Objekt: Logging ist Cross-Cutting-
Infrastruktur, die jede Schicht braucht (systems/render/world/fdm), und ein `FBLog&` durch jede
`Run()`-Signatur zu fädeln würde den ganzen Aufrufgraph anfassen, ohne Verhalten zu gewinnen. Eine
Aufrufstelle bleibt ein Einzeiler:

```cpp
FBLog::Warn("pilot", "sink_rate_high", {{"vs", -12.3}});
```

**I/O-frei**: emittiert wird nur, wenn ein `FBLogSink` injiziert ist (`FBLog::SetSink`); ohne Sink
kostet es einen Zeigervergleich und keine Formatierung. Die konkreten Sinks (stdout/Datei/Fan-out,
plus `FBBufferedLogSink`) leben in `app/FBLogSinks.h` — die eine Stelle, an der rohes stdio erlaubt
ist.

| Level | `FBLogLevel::Debug / Info / Warn / Error` |
|---|---|
| Default-Level | `Debug` — jede migrierte Aufrufstelle druckte vorher bedingungslos, und die WASM-Browserkonsole soll unverändert aussehen; wer einen ruhigeren Kanal will (die `events.log` des Missions-Runners), hebt das Level explizit an. |

**`FBLogField`** ist ein `key=val`-Feld: numerische Overloads formatieren kompakt (`%g`), `int` als
Dezimalzahl, `bool` als `0`/`1`, Strings unverändert — der Sink quotiert einen Wert mit Leerzeichen
(spiegelt die alte `events.log`-Konvention `reason="..."`).

**Threading** — die entscheidende Aufteilung für `fb-gym --threads N`:

| Was | Speicherklasse | Warum |
|---|---|---|
| `Sink_`, `Level_` | prozessweit statisch | KONFIGURATION, einmal beim Boot gesetzt |
| `TimeS_`, `Unit_[32]`, `ThreadSink_` | `thread_local` | KONTEXT: ein Thread, der Einheit `two` rechnet, IST in einem anderen Kontext als einer, der `lead` rechnet. Die Alternative (ein Kontextobjekt durch jede `Run()`-Signatur) ist genau das, was diese Fassade vermeidet. |

Single-Thread-Clients (native, wasm) sehen identisches Verhalten: ein Thread, ein Kontext.

**Unit-Attribution**: `FBLog::SetUnit(label)` — ist sie gesetzt, trägt jede Zeile `unit=<callsign>`
als ERSTES Feld (ein Skript splittet auf das erste Feld, ein Mensch sieht ohne Scannen, wessen Zeile
es ist). Ist sie leer, wird NICHTS hinzugefügt: die Zeilen einer Einzel-Einheit sind die Zeilen der
Mission und brauchen keine Attribution — das hält sie außerdem byte-identisch zu jeder
Regressions-Baseline von vor der Multi-Unit-Ära. `Unit_` ist ein fester 32-Byte-Puffer: er wechselt
pro Akteur pro Tick und darf nie allokieren.

**`FBLog::SetThreadSink(sink)`** leitet die Ausgabe DIESES Threads um. Der Missions-Runner zeigt jeden
Worker auf den Puffer DER EINHEIT, die er rechnet, nie auf die gemeinsame `events.log` — ein Worker,
der direkt durchschriebe, machte die Zeilenreihenfolge zu einer Funktion des Schedulers. Die Puffer
werden an der Tick-Barriere in Einheiten-Reihenfolge abgelassen. **Level und „hört überhaupt jemand
zu" bleiben die Frage des PROZESS-Sinks** — ein Capture-Puffer ist eine Umleitung einer bereits
akzeptierten Zeile, kein zweiter Schalter.

**Zwei RAII-Scopes**, damit kein Zustand über eine Einheit hinausleckt:

| Klasse | Wirkung |
|---|---|
| `FBLogUnitScope(label)` | setzt die Attribution und löscht sie im Destruktor — kein Label kann auf die Zeilen der nächsten Einheit oder auf die missionsweiten Zeilen zwischen den Schleifen lecken |
| `FBLogThreadSinkScope(sink)` | dieselbe Disziplin für den Capture-Puffer — ein Worker, der ohne Löschen zurückkehrte, schriebe im nächsten Tick weiter hinein, unter Umständen in den Puffer einer anderen Einheit |

#### 3.2 `FBTelemetry` — Zeitreihe mit Schema

`core/FBTelemetry.h/.cpp`. Klassen DEKLARIEREN sich als Quelle; die Emission ist ZENTRAL.

| Typ | Rolle |
|---|---|
| `FBTelemetryChannel` | `{Name, Unit}` |
| `FBTelemetrySchema` | geordnete Kanalliste, `Add(name, unit="")` |
| `FBTelemetryRow` | Feldpuffer; `Push(double)` formatiert mit `%.6f`, `Push(int)`/`Push(bool)`/`Push(string)` |
| `FBTelemetrySource` | Interface: `TelemetryName()`, `DeclareTelemetry(schema)` (EINMAL, bei `Bus::Start()`), `SampleTelemetry(row)` (EINMAL je `Bus::Tick()`) |
| `FBTelemetrySink` | Interface: `Header(columns)`, `Row(fields)` — konkrete Implementierung (`FBCsvTelemetrySink`) in `app/` |
| `FBTelemetryBus` | der EINE Emitter: `Register(src)` (GEBORGTER Zeiger, der Bus besitzt nie ein System), `SetSink`, `Start()`, `Tick(simTimeS)` |

**Die Regel, die alles zusammenhält**: eine Zeile entsteht durch KONKATENATION — jede Quelle pusht
genau so viele Felder, wie sie Kanäle deklariert hat, in derselben Reihenfolge. **Deklarations- =
Registrierungs- = Spaltenreihenfolge**, kein stringindiziertes Nachschlagen zur Sample-Zeit.

`Start()` legt zuerst den Kanal `t` (Einheit `s`) an, dann jede Quelle in Registrierungsreihenfolge,
und schiebt den Header raus. `Tick()` startet notfalls selbst, pusht die Simzeit und sampelt jede
Quelle in eine Zeile. **Ein Null-Sink macht `Tick()` zu einem billigen No-Op** — der WASM-Boot lässt
ihn ungesetzt.

**Die Anhänge-Regel** (CLAUDE.md + `FBSystemHealth.h`-Banner): neue Quellen werden IMMER hinten
angehängt (`units/FBSimUnit::StartTelemetry`), damit keine je gemessene Spalte ihre Position verliert.
Deshalb ist `FBSystemHealthTelemetry` eine eigene Quelle statt weiterer Spalten auf einer
bestehenden, und deshalb tragen Rwr/Cmds/Gun ihre Blockgültigkeit selbst (s. [1.6](#16-fbstatebustelemetry--gültigkeit-als-messbare-zeitreihe)).

**Die `core/`-eigenen Telemetriequellen:**

| Quelle | Name | Spalten |
|---|---|---|
| `FBCommandBus` | `cmd` | `cmd_issued`, `cmd_accepted`, `cmd_rejected`, `cmd_clamped`, `cmd_inhibited`, `cmd_pending`, `cmd_last`, `cmd_last_outcome`, `cmd_last_reason` |
| `FBStateBusTelemetry` | `blk` | 14 × `blk_*` (s. 1.6) |
| `FBSystemHealthTelemetry` | `dmg` | `dmg_hits`, `dmg_failed` (Bitmaske über `FBSystemId`), `dmg_degraded`, `dmg_effective` |

---

### 4. Die zwei Richter

Zwei Instanzen, zwei FRAGEN, nie zu einer vermischt. Beide gehören dem CLIENT/Runner, beide werden mit
einem schreibgeschützten Pro-Tick-Sample gefüttert, beide sind für `systems/` und `modules/`
unsichtbar. `grep -rn 'FBSimUnit\|FBFlightMonitor\|FBMissionMonitor' src/systems src/modules` bleibt
ohne Treffer — das Modul WIRD BEOBACHTET, es sieht die Richter nicht.

| | `core/FBFlightMonitor` | `core/FBMissionMonitor` |
|---|---|---|
| Frage | „hat die ZELLE überlebt" | „ist die MISSION gelungen" |
| Wahrheitsquelle | das gepinnte JSBSim-Modell selbst (Lage, Raten, Fahrwerksposition, Strebenkraft, Ground-Reaction-Kontaktflags, statisches Gewicht) | die MISSIONSDATEI selbst (eigene, unveränderliche Plan-/Ziel-/Runway-Kopie) + beobachtete Position + beobachtetes Roster |
| Weiß NICHTS über | Runways, Missionen, „wo eine Landung hätte stattfinden sollen" | Physik/Absturz |
| Verdikte | `FBKoReason` | `FBMissionVerdict` (`Success`/`Fail`/`Timeout`) |
| Latching | ja: `Tick()` liefert `true` an genau dem EINEN Tick, an dem es trippt; jeder spätere Aufruf ist ein No-Op mit `false` | ebenso |
| Selbst-Log | `FBLog::Error("monitor", "KO", …)` mit allen gemessenen Werten | `FBLog::Info("mission", "RESULT", {result, reason})`, dazu `WP_REACHED` |

Beide werden von JEDEM Client gefüttert, der eine Sim-Schleife fährt — `app/FBMissionRunner.cpp`
(fb-gym / `gpu_native --mission`) genauso wie der WASM-Frame-Loop (`app/FBAppWasm.cpp`). Je EINE
Definition, kein zweiter Paralleltest.

#### 4.1 `FBFlightMonitor` — das physikalische K.O.

`core/FBFlightMonitor.h/.cpp`. **Vollständig modell-abgeleitet und airframe-agnostisch by
construction**: die Klasse kennt keinen Modul-/Flugzeugtyp und keine modul-deklarierten Zahlen, nur
generische FDM-/Ground-Reaction-Größen, die jedes JSBSim-Modell hergibt. **Es gibt gar keinen
modulseitigen Deklarationskanal für K.O.-Schwellen** — ein Modul kann sich kein milderes Urteil
erklären.

**`FBFlightMonitorSample`** — bewusst NICHT `fb_fdm_state` (core/ hängt nicht von fdm/ ab). Der
Aufrufer füllt diese schmale Sicht (`FBBuildFlightMonitorSample`, `app/FBMissionBoot.h`):

| Feld | Bedeutung |
|---|---|
| `LatDeg`, `LonDeg`, `ElevM` | Position, Höhe m ASL |
| `GroundAslM` | DIESELBE Pro-Tick-Probe, die der Aufrufer schon aus seinem injizierten `FBElevationProvider` gezogen und in JSBSims Bodenhöhe gefüttert hat — als `double` übergeben, damit der Monitor ein flacher Wertkonsument bleibt |
| `RollDeg`, `PitchDeg` | Lage |
| `PDegS`, `QDegS`, `RDegS` | Körperraten, °/s |
| `VsMs` | Vertikalgeschwindigkeit, + = Steigen |
| `TasMs` | wahre Fahrt, m/s — **ausschließlich** zur Ableitung des Flugbahnwinkels, nie als Aero-/AoA-Surrogat |
| `GearPosNorm` | 0=oben..1=unten, die modell-eigene verzögerte Fahrwerksposition |
| `GearForceLbs` | Spitzen-Strebenkompressionskraft der Räder in diesem Tick (`FBFdm::GetMaxGearForceLbs`) |
| `WeightLbs` | das modell-eigene aktuelle statische Gewicht (`FBFdm::GetWeightLbs`) |
| `AnyWow` | irgendein BOGEY-(Rad-)Kontakt komprimiert |
| `StructureContact` | irgendein NICHT-rädriger Ground-Reaction-Kontaktpunkt komprimiert (ein deklarierter STRUCTURE-Punkt: Flügelspitze/Leitwerk/Einlauf/Radom der heutigen F-16 — was auch immer eine `aircraft.xml` jenseits ihres Fahrwerks deklariert) |
| `FdmFault` | der Integrator selbst hat aufgegeben (JSBSim hat z.B. eine `FloatingPointException` aus einem Tabellen-Lookup geworfen) — ein schlichter `bool`, damit der Monitor fdm-entkoppelt bleibt |

**Die Prüfreihenfolge in `Tick(s, simTimeS)`** — die Reihenfolge ist Teil der Herleitung:

| # | Prüfung | Bedingung | Trigger | Warum an dieser Stelle |
|---|---|---|---|---|
| 0 | `NumericalDivergence` | `FdmFault` ODER erstes nicht-endliches Feld der ROH-Eingaben | 1 Tick | **VOR allem anderen**: jede andere Prüfung ist ein VERGLEICH, und IEEE-754 macht jeden Vergleich gegen NaN falsch. Ohne diese Prüfung segelte ein divergiertes FDM an allen vorbei und der Lauf endete als unerklärter TIMEOUT. Geprüft auf den ROHEN Eingaben (nicht auf `aglM` oder dem abgeleiteten FPA), damit die Divergenz an ihrem Eintrittspunkt gefangen wird. |
| 1 | `CfitPenetration` | `aglM < kPenetrationMarginM` (−3,0 m) | 1 Tick | vor der Bestätigungsgruppe, weil die eigene 3-m-Marge einen Einzeltick-Geländesprung schon absorbiert; ein echtes Loch-durchs-Mesh soll kein Fenster abwarten |
| 2 | `StructureContact` | `StructureContact` für `kContactConfirmS` (0,2 s) | bestätigt | binäres Signal, s.u. |
| 3 | `GearUpContact` | `AnyWow && GearPosNorm < kGearDownThreshold` (0,5) für 0,2 s | bestätigt | binäres Signal |
| 4 | `HardLanding` | `AnyWow && WeightLbs > 0 && GearForceLbs > kHardLandingForceFactor * WeightLbs` (3,0×) | 1 Tick | wird bei JEDEM Tick mit komprimiertem Bogey geprüft, nicht nur an der Aufsetzflanke, damit die tatsächliche Kraftspitze erwischt wird, wo immer im Kompressionszyklus sie liegt |
| 5 | `AttitudeContact` | (`AnyWow` ODER `StructureContact`) UND (`|Roll| > 15°` ODER `Pitch > 15°`) | 1 Tick | Geometrie-Risiko (Tailstrike/Strukturschlag) auch bei gutmütiger Fahrwerkslast |
| 6 | `Loc` (Departure) | in der Luft UND `sqrt(p²+q²+r²) > kLocRateDegS` (60 °/s), gehalten `kLocSustainS` (3 s) | Dauer | rein verhaltensbasiert |
| 6b | `Loc` (Stall/Mush) | in der Luft, `TasMs > kMinTasMs` (15 m/s), `|Pitch − FPA| > kNoseFlightpathMismatchDeg` (30°), gehalten `kStallSustainS` (4 s) | Dauer | rein kinematisch |

**Die Schwellen und ihre Herleitung** (alle in `FBFlightMonitor.cpp`; „regression" heißt: vorbestehende
`FBMissionRunner`-Konstante, wörtlich hierher verschoben):

| Konstante | Wert | Herleitung |
|---|---|---|
| `kPenetrationMarginM` | **−3,0 m** | Fahrwerk/Bauch steckt IM Gelände-Mesh, keine sanfte Landung. Regression: `FBMissionRunner`s vorbestehendes `aglM < -3.0`, unverändert. |
| `kGearDownThreshold` | **0,5** | JSBSims eigenes `gear/gear-pos-norm` (0=oben..1=unten, der verzögerte kinematische Transit jedes Einziehfahrwerk-Modells). Jeder WOW-Kontakt bei substantiell eingefahrenem Fahrwerk ist eine Bauch-/Gear-up-Landung, egal wo — eine rein physikalische Tatsache, und **0,5 ist schlicht der generische Mittelpunkt dieses normierten [0,1]-Bereichs**. |
| `kHardLandingForceFactor` | **3,0** | Aus der EIGENEN Fahrwerksphysik des Modells, nicht aus einer deklarierten Sinkrate: JSBSims Feder-/Dämpfer-Reaktion (`GetMaxGearForceLbs`) IST die real simulierte Aufsetzlast; sie gegen das modell-eigene statische Gewicht (`GetWeightLbs`) zu vergleichen braucht genau EINE core-eigene, airframe-agnostische Schranke — einen Lastfaktor, jenseits dessen kein Fahrwerk dieser KLASSE ausgelegt ist. Generische Fahrwerks-Auslegungspraxis bemisst eine Grenz-/Hartlandung um ein niedriges einstelliges „g" am CG; **konzentriert auf EINE Strebe** (die Prüfung nimmt die SPITZE, keine Summe) überschreitet eine sauber geflogene Landung routinemäßig 1× Gesamtgewicht auf einem Hauptfahrwerk, ohne im Entferntesten hart zu sein. 3,0× liegt bequem über diesem Normal-Transienten und weit unter einem strukturversagens-tauglichen Aufschlag — konservativ in Richtung NICHT-Auslösen bei einer guten Landung. **Empirisch verifiziert**: der Referenz-Startlauf überschreitet nie einen kleinen Bruchteil von 1× Gewicht je Strebe; ein absichtlich exzessives Aufsetzprofil überschreitet die Schranke mit weitem Abstand. |
| `kMaxContactPitchDeg` / `kMaxContactRollDeg` | **15° / 15°** | Extremlage bei Bodenkontakt — ein Geometrierisiko, generisch für jede Zelle mit endlicher Bodenfreiheit; ERGÄNZT (ersetzt nicht) die modellgetriebene Strukturkontakt-Prüfung für eine Zelle, deren `aircraft.xml` keine STRUCTURE-Punkte deklariert. Konservative, klassengenerische Schranke, nicht auf eine Zelle getunt. |
| `kLocRateDegS` / `kLocSustainS` | **60 °/s / 3,0 s** | Rein VERHALTENSBASIERT, keine aero-modellspezifische Größe (**keine AoA-Zahl** — die bedeutet je Flugzeugklasse etwas anderes und ist für manche gar nicht sinnvoll): eine anhaltende, mehrachsige Körperraten-Magnitude dieser Größe, mehrere Sekunden in der Luft gehalten, ist für jede Starrflügelzelle ein Trudeln/Taumeln, kein koordiniertes Manöver. Generisches Ingenieursurteil, keine Pro-Zelle-Zahl. |
| `kMinTasMs` | **15 m/s** | schließt den Nahe-null-Fahrt-Setzvorgang aus. Regression: `FBMissionRunner`s altes `cas > 15.0`-Tor (m/s). |
| `kNoseFlightpathMismatchDeg` / `kStallSustainS` | **30° / 4,0 s** | Stall-/Mush-/Deep-Stall-Signatur, rein KINEMATISCH und bewusst NICHT die modell-eigene `alpha-deg`-Ausgabe (keine aerodynamische Größe, keine Pro-Zelle-Stall-AoA; das Vanilla-Modell exponiert ohnehin kein deklariertes Alpha-Limit, aus dem man eine ableiten könnte): aus Lage + Geschwindigkeitsvektor allein — wo die Nase ZEIGT (`PitchDeg`) gegen wo das Flugzeug tatsächlich HINGEHT (FPA aus `VsMs`/`TasMs`, `atan2(VsMs, sqrt(TasMs²−VsMs²))`). Ein großer, anhaltender Unterschied ist Flug, der für KEINE Starrflügelzelle anliegend/konventionell ist. Ein schneller, flacher, absichtlicher Sturzflug (nahezu waagerechte Lage, hohe Sinkrate aus purer Geschwindigkeit) hält diesen Unterschied klein — **empirisch gegen ein Sturzflug-Testprofil verifiziert** —, ein gestalltes/mushendes Flugzeug nicht. |
| `kContactConfirmS` | **0,2 s** | **Nur auf die zwei BINÄREN, schwellenüberschreitenden Signale angewandt** (`StructureContact` und die `AnyWow`-Komponente von `GearUpContact`: JSBSims WOW-Flag, ein hartes wahr/falsch darüber, auf welcher Seite der Oberfläche ein Kontaktpunkt sitzt), die ein Einzeltick-Geländesprung für genau eine Probe umklappen kann: der Aufrufer schiebt einmal pro Tick eine FRISCHE Geländehöhe ins FDM (ein diskretes Update, keine kontinuierlich verfolgte Fläche), und auf einem Live-DEM mit echtem Längsgefälle kann dieser Sprung kurz als Kontakt gelesen werden. **Empirisch bestätigt**: ein Live-Daten-Lauf ohne dieses Fenster löste bei einem gewöhnlichen Startlauf fälschlich `STRUCTURE_CONTACT` aus. Bewusst NICHT auf `HardLanding`/`AttitudeContact` angewandt — das sind glatt veränderliche PHYSIKALISCHE Größen aus der eigenen kontinuierlichen Dynamik des Flugzeugs, und (gemessen) klingt eine echte Hartlandungs-Kraftspitze selbst innerhalb eines ähnlich kurzen Fensters ab; sie zu „sustainen" ließe die Prüfung genau die Aufschläge verfehlen, für die sie existiert. 0,2 s sind ein paar Ticks bei jeder Taktrate dieses Codebaums (10 Hz Missionsentscheidung, 60+ Hz WASM-Frame-Loop) — lang genug, ein Einzelsample-Artefakt zu verwerfen, viel zu kurz, um für einen echten anhaltenden Kontakt (Sekunden, kein Bruchteil davon) eine Rolle zu spielen. |

**`FBKoReason`-Strings** (so stehen sie in der `events.log`): `NONE`, `NUMERICAL_DIVERGENCE`,
`STRUCTURE_CONTACT`, `CFIT`, `GEAR_UP_CONTACT`, `HARD_LANDING`, `ATTITUDE_CONTACT`, `LOC`.

**Der `KO`-Log-Eintrag** trägt alles Gemessene mit, damit der Aufrufer nichts nachrechnen muss:
`reason`, `detail`, `lat`, `lon`, `aglM`, `vsMs`, `roll`, `pitch`, `p`, `q`, `r`, `gearPos`,
`gearForceLbs`, `weightLbs`.

#### 4.2 `FBMissionMonitor` — das Missions-Urteil

`core/FBMissionMonitor.h/.cpp`. Konstruiert aus der MISSIONSDATEI:
`FBMissionMonitor(plan, objectives, runway, haveRunway, timeoutS, wpCaptureM = 500.0)` — alles
KOPIERT, nie das lebende, modul-mutierte Exemplar. `wpCaptureM` entspricht dem Guidance-seitigen
Erfassungsradius (`FBNavSystem::AdvanceWaypoint`, Default ebenfalls 500 m), damit Missions-Urteil und
geflogene Bahn sich über „erreicht" einig sind.

**`FBMissionMonitorSample`** — bewusst schmal:

| Feld | Bedeutung |
|---|---|
| `LatDeg`, `LonDeg` | beobachtete Position |
| `AnyWow` | trägt gerade irgendein Fahrwerk Gewicht |
| `GroundSpeedKt` | das Stillstands-auf-der-Runway-SUCCESS-Tor |
| `CombatIneffective` | **der Abschuss als MISSIONS-Tatsache** (`FBSystemHealth::CombatEffective` negiert). Gehört hierher und nicht in den Physik-Richter, aus genau dem Grund, der die beiden trennt: der Physikrichter fragt, ob die Zelle überlebt hat, und ein Jet, dem gerade das Triebwerk herausgeschossen wurde, fliegt noch und hat nichts überlebt. Ob seine SORTIE vorbei ist, ist eine Missionsfrage. **Die Einheit wird dadurch nicht gestoppt, eingefroren oder für tot erklärt** — sie wird weiter integriert, bis der Physikrichter sein eigenes Wort hat. |
| `Roster` | `FBMissionRoster` — die anderen Einheiten, wie beobachtet: Id, Fraktion, das eine Bit ihres eigenen Gesundheitsregisters. Leer für eine Mission ohne Kampfziele. |

**Die Prüfreihenfolge in `Tick()`:**

1. **Abschuss** (`CombatIneffective`) — ZUERST, weil alles darunter ein Flugzeug voraussetzt, das noch
   irgendwohin kommen könnte. WESSEN Fehlschlag das ist, hängt an der Deklaration:

   | Deklaration | Folge |
   |---|---|
   | keine `objective`-Zeile | `FAIL "combat ineffective (weapon damage)"` — die Alt-Lesart, unverändert |
   | `objective survive` | `FAIL "combat ineffective (survive objective lost)"` |
   | nur `objective kill …` | **nichts** — diese Einheit wurde nicht beauftragt heimzukommen, ihr eigener Verlust beendet nichts. Ein gleichzeitiger Schlagabtausch ist damit ein Tausch statt eines beidseitigen Fehlschlags: eine Missionsdesign-Entscheidung der DATEI, nicht dieser Klasse. |

2. **Bodenkontakt abseits der zugewiesenen Runway** → `FAIL "touchdown off the assigned runway"`. Eine
   Landung, die der reine Physikrichter akzeptiert hat (überlebbar), die aber am falschen Ort
   stattfand, hat das Missionsziel verfehlt.
3. **Wegpunkt-Fortschritt** gegen die eigene Plankopie, rein aus beobachteter Position.
4. **SUCCESS braucht BEIDE Hälften**: Plan erledigt UND alle `kill`-Ziele erfüllt (und kein
   `survive`-Ziel offen, s.u.).
5. **Timeout** (`simTimeS >= TimeoutS_`) → ruft `Finalize()`.

**`OnRunway`-Geometrie** (unveränderte Geometrie des Vorgänger-Crashtors): (lat,lon) auf die
Längs-/Querachse der Runway projizieren (Mittellinie ab der Schwelle auf `TrueHeadingDeg`,
`FBTrackProjectM`) — auf der Runway genau dann, wenn innerhalb ihrer Länge (± `marginAlongM` davor/
dahinter) und halben Breite (± `marginAcrossM`). **`WidthM <= 1`** (das Missionsformat lässt sie
ungesetzt) fällt auf eine großzügige **60-m**-Generic-Runway-Breite zurück.

| Aufruf | Margen |
|---|---|
| Off-Runway-FAIL-Tor | 50 m längs, 30 m quer |
| Land-Waypoint-SUCCESS-Tor | 0 m längs, 15 m quer |

**`kStillstandKt` = 2,0 kt** — ein Taxi-Geschwindigkeits-Schwellwert, kein voller Stillstand: ein
ausrollendes Flugzeug hat noch einige Ticks lang ein paar Knoten, und das Missionsziel („gelandet und
gestoppt") ist deutlich vor dem letzten Knoten erfüllt.

**Wegpunkt-Erfassung — zwei Regeln:**

| Typ | Regel |
|---|---|
| `Enroute` u.a. | (a) **capture**: `FBPlanarDistM(pos, wp) <= WpCaptureM_`; Referenz ist das FLUGZEUG (seine Breite skaliert die Länge) — die historische Konvention hier. (b) **passed**: ODER DAS FLUGZEUG IST DARÜBER HINAUS. Ein Erfassungskreis fragt „ist es angekommen"; für einen Fix, an dem das Flugzeug nicht ankommen KANN (einer innerhalb seines eigenen Kurvenradius), ist die ehrliche Frage „ist es dort hingekommen", und die Antwort ist die Achse des Legs: jenseits der Senkrechten durch den Fix liegt der Fix hinten. **Erst ab dem zweiten Wegpunkt**, weil es erst dann einen deklarierten Anflugkurs gibt, gegen den „hinten" gemessen werden kann. |
| `Land` (immer der LETZTE) | erfasst und avanciert NICHT. Verlangt, dass das Flugzeug tatsächlich AUF der Runway ZUM STEHEN kommt (`AnyWow` UND `GroundSpeedKt < 2,0` UND `OnRunway(0, 15)`) — ein Anflug, der die Schwelle nur mit Fluggeschwindigkeit streift, ist keine Landung. SUCCESS ist hier eine eigenständige Bedingung, nie über ein `ActiveIdx_`, das hinten aus dem Plan fällt. |

Die „passed"-Regel ist DIESELBE geometrische Regel, mit der `systems/FBNavSystem` die GUIDANCE
sequenziert — und bewusst eine ZWEITE, unabhängige Formulierung davon statt eines Aufrufs dorthin:
diese Klasse urteilt aus ihrer eigenen privaten Plankopie und beobachteter Position allein, und das
darf nicht gegen fünf geteilte Zeilen eingetauscht werden.

**`PlanJudged_` — wann der Flugplan überhaupt Teil des Urteils ist** (im Konstruktor entschieden):

```
PlanJudged_ = Objectives_.empty() || HasObjective(Waypoints);
```

Ohne `objective`-Zeilen ist der Plan das GANZE Urteil (das ursprüngliche Urteil des Formats,
unverändert). MIT ihnen ist der Block die vollständige Aussage darüber, was diese Einheit erreichen
muss — der Plan wird also nur beurteilt, wenn er es sagt (`objective waypoints`): die `wp`-Zeile einer
BVR-Mission ist ein gebriefter Vektor für die Guidance, kein Ort, an dem der Jäger ankommen muss, und
sie als Ziel zu lesen ist genau das, was eine entschiedene Auseinandersetzung in einen Timeout laufen
ließe.

**`Finalize(s, simTimeS)` — das Ende des Laufs, wenn es nicht die eigene Uhr dieser Einheit war.**
`survive` ist das eine Ziel, das nicht früh erfüllt werden KANN — „noch kampffähig" ist erst wahr,
wenn kein Lauf mehr übrig ist, in dem man abgeschossen werden könnte (eine Rakete, die noch in der
Luft ist, als ihr Schütze starb, ist genau der Grund, warum es hier keine Abkürzung gibt). Eine
Einheit mit `survive` bleibt daher absichtlich unentschieden, solange die Auseinandersetzung läuft,
und wird hier gefragt. Idempotent und latchend wie `Tick`.

**`KillObjectivesMet` überspringt zwei Arten bewusst**: `Survive` (nur in `Finalize` beantwortet) und
`Waypoints` (von `PlanJudged_`/`PlanDone_` beantwortet). Ließe man eine von beiden in `FBObjectiveMet`
laufen, wäre sie dauerhaft unerfüllt — eine Einheit, die `kill` UND `waypoints` deklariert (der
dokumentierte Weg, den Flugplan beurteilt zu lassen und trotzdem ein Kampfziel zu haben), könnte dann
nie SUCCESS erreichen.

**Die SUCCESS-Formulierung** ist wortgetreu abwärtskompatibel: ohne Ziele exakt der Satz des Plans,
Byte für Byte (diese Strings stehen in jeder gemessenen `events.log`) — `"all waypoints reached"` bzw.
`"stopped on the runway"`. Mit Zielen wird angehängt: `", objectives met"`, und in `Finalize`
zusätzlich `", survived"`. Ein Akteur mit Zielen und ohne Wegpunkte hat nur diese zu melden
(`"objectives met"`).

#### 4.3 Warum es zwei sind

Ein sanftes Aufsetzen mit ausgefahrenem Fahrwerk auf einem Acker ist für das FLUGZEUG kein Absturz —
nur für die Bewertung einer Mission. Umgekehrt ist „kampfunfähig" ein Urteil über die SORTIE, nie ein
Freeze und nie ein Fall für den Physikrichter. Die Trennung ist das, was beide Urteile ehrlich hält:
der Physikrichter kann nicht durch Missionsgestaltung beeinflusst werden, und der Missionsrichter
kann nicht durch Physik überstimmt werden.

---

### 5. Missionsdaten als Typen

#### 5.1 `FBMissionFile` — der `.fbm`-Parser

`core/FBMissionFile.h/.cpp`. **Reine String-rein/Struct-raus-Funktion**, kein File-I/O — die App liest
die Datei und reicht den Text herein; so bleibt `core/` plattformneutral. Format-Referenz:
`doc/mission-format.md`.

```cpp
bool FBParseMissionFile(const std::string &text, FBMission &out, std::string *err = nullptr);
```

**Struktur:**

```
FBMission { Name, FBRunway Runway, HaveRunway, TimeoutS, vector<FBMissionUnit> Units }
FBMissionUnit { Id, ModuleName, FBUnitTeam Team, FBSpawn Spawn, HaveSpawn,
                FBFlightPlan Plan, vector<FBObjective> Objectives,
                vector<pair<string,string>> SetKV }
```

Ein Block = eine `units/FBSimUnit`; N Blöcke = ein Verband. Deshalb ist hier nichts mehr ein Skalar
„das Modul"/„der Spawn". Ein Einzelflug ist der Sonderfall „ein Block", kein zweiter Dialekt.

**Zwei Geltungsbereiche, eine Datei** — beide Richtungen sind harte Fehler:

| Bereich | Schlüsselwörter | Regel |
|---|---|---|
| missionsweit | `name`, `runway`, `timeout` | nur VOR dem ersten `unit`-Block. Eine `runway`-Zeile zwischen zwei Units läse sich sonst still als „missionsweit, aber spät deklariert". |
| akteursbezogen | `module`, `team`, `spawn`, `wp`, `land`, `objective`, `set` | nur INNERHALB eines Blocks; ohne vorangehende `unit`-Zeile gibt es keinen Besitzer |

**Die Zeilengrammatik:**

| Zeile | Syntax | Anmerkung |
|---|---|---|
| `unit` | `unit <callsign>` | genau ein Callsign; Duplikat = Fehler |
| `name` | `name <freier Text bis Zeilenende>` | Pflicht |
| `runway` | `runway <lat> <lon> <elevM> <hdgDeg> <lengthM>` | `WidthM` bleibt 0 → die 60-m-Fallback-Regel greift |
| `timeout` | `timeout <positive Sekunden>` | Pflicht; `0` oder negativ = Parse-Fehler |
| `module` | `module <name>` | Pflicht je Block; über `FBModuleRegistry` aufgelöst |
| `team` | `team friendly\|hostile\|neutral` | fehlt = `friendly` |
| `spawn` | `spawn <lat\|threshold> <lon> <altM\|ground> <hdgDeg> <speedKt>` | Pflicht je Block, nur EINMAL. `threshold` benutzt die Runway-Position wieder (Bodenstart-Bequemlichkeit, keine zweite Positionssyntax) und braucht eine `runway`-Zeile; `ground` löst die Höhe aus Terrain + Fahrwerksfreiheit beim Spawn auf (`app/FBMissionBoot.h`), nie ein separater Codepfad |
| `wp` | `wp <lat> <lon> <altM> <speedKt>` | `FBWaypointType::Enroute` |
| `land` | `land` | braucht eine `runway`-Zeile; erzeugt einen `FBWaypointType::Land` auf Schwellenposition/-elevation, Speed 0 |
| `objective` | `objective survive` \| `objective waypoints` \| `objective kill unit <callsign>` \| `objective kill team <faction>` | s.u. |
| `set` | `set <key> <value…>` | ROHE KV-Daten; **der Parser interpretiert NIE einen Schlüssel**, nur das Modul (`FBModule::ApplySetup`). Ein unbekannter Schlüssel ist ein Laufzeit-FAIL des Runners, kein Parse-Fehler. |

Kommentare: `#` bis Zeilenende. Leerzeilen werden ignoriert.

**Die Callsign-Regel** (`CallsignOk`): 1–24 Zeichen aus `[A-Za-z0-9_-]`. Begründung: ein Callsign wird
auch ein DATEINAME (`outDir/telemetry_<id>.csv`) und ein Log-Feldwert — das akzeptierte Alphabet ist
die Schnittmenge aus „in beidem sicher": keine Trennzeichen, kein Quoting, keine Leerzeichen.

**Die Objective-Sonderregeln:**

- `kill` verlangt einen expliziten `unit`/`team`-Diskriminator statt zu raten, was auf das Wort folgt:
  ein Callsign DARF „hostile" heißen (das Parser-Alphabet verbietet es nicht), und eine Mission, deren
  Kill-Ziel still die Bedeutung wechselt, weil jemand einen Jet nach einer Fraktion benannt hat, ist
  kein Format, das jemand debuggen sollte.
- `objective waypoints` braucht `wp`/`land`-Zeilen ÜBER sich.
- Eine Einheit kann sich nicht selbst als `kill unit`-Ziel haben.
- Ein exaktes Duplikat (gleiche Art + gleiches Ziel) ist ein Fehler.

**Die Ganzdatei-Prüfungen am Schluss** (Fehler ohne Zeilennummer, weil es keine Zeile gibt, auf die
man zeigen könnte): `name` fehlt, `timeout` fehlt, kein `unit`-Block, ein Block ohne `module`, ein
Block ohne `spawn` — und: **ein `kill unit`-Ziel wird gegen die GANZE Besetzung aufgelöst**, nicht nur
gegen die bis dahin gesehenen Blöcke (ein Ziel darf eine weiter unten deklarierte Einheit nennen; die
beiden Seiten eines Duells wären sonst ein Reihenfolge-Rätsel). Ein Ziel, das nicht existiert, ist
eine Mission, die nie gewonnen werden kann — also ein Parse-Fehler statt eines stillen, nie erfüllten
Ziels.

Fehlermeldungen tragen `"line N: …"`; `out` ist nur bei `true` vollständig gültig.

#### 5.2 `FBFlightPlan` / `FBWaypoint`

`core/FBFlightPlan.h`. Eine schlichte geordnete Wegpunktkette. **Nur Struktur, keine
Prozedurlogik** — SIDs/Holdings/Anflugsequenzierung sind die Phasenmaschine von `FBPilot`, nicht
dieser Container. Liegt in `core/`, damit `systems/FBPilot` und eine künftige Missions-Setup-UI ihn
teilen, ohne dass eines das andere besitzt.

```
FBWaypointType { Takeoff, Enroute, Approach, Land }
FBWaypoint { LatDeg, LonDeg, AltM (m ASL), SpeedKt (CAS), Type }
FBFlightPlan: AddWaypoint / Clear / Size / Empty / At(i) / ActiveIndex / SetActiveIndex / ActiveWaypoint
```

`ActiveWaypoint()` liefert `nullptr`, wenn der Index außerhalb liegt. Der Parser erzeugt heute nur
`Enroute` und `Land`.

#### 5.3 `FBRunway`

`core/FBRunway.h`. Die landungsrelevante Geometrie EINER Runway:
`ThresholdLatDeg`, `ThresholdLonDeg`, `ThresholdElevM`, `TrueHeadingDeg` (Kurs der verlängerten
Mittellinie), `LengthM`, `WidthM`. Wertetyp in `core/` (wie `FBFlightPlan`), damit die Anflug-/
Landephasen von `FBPilot` und eine künftige Flugplatzdatenbank dieselbe Form teilen.

#### 5.4 `FBSpawn`

`core/FBSpawn.h`. Die deklarative Anfangsbedingung einer Einheit — **reine Daten, keine Modi/Phasen**:

| Feld | Bedeutung |
|---|---|
| `LatDeg`, `LonDeg` | Position |
| `Ground` | `true`: `ground`-Schlüsselwort — auf dem Fahrwerk auf der aufgelösten Geländehöhe sitzen. `false`: `AltM` ist eine literale ASL-Höhe (Luftstart) |
| `AltM` | literale Zielhöhe m ASL — nur bei `!Ground` bedeutsam |
| `HeadingDeg`, `SpeedKt` | Kurs, Geschwindigkeit |

**Es gibt hier keinen getrennten Boden-/Luft-Codepfad jenseits dieses einen `bool`.** Runner/Boot
machen daraus GENAU EINE JSBSim-IC-Anwendung (`FBFdmBoot::Spawn` wendet Position + Lage +
Geschwindigkeit gemeinsam an — `app/FBMissionBoot.h`s `FBMissionSpawnActor`) plus die
Anfangs-`FBPilot`-Phase des Moduls.

#### 5.5 `FBObjective` — Kampfziele + das Roster

`core/FBObjective.h`.

**Warum das Format es brauchte** (`doc/mission-format.md`, „Urteil"): vorher konnte ein Waffentreffer
nur den FEHLSCHLAG dessen erzeugen, der getroffen wurde, weil keine Einheit erklären konnte, dass
dieser Fehlschlag ihr eigenes ZIEL war. Eine Mission, deren feindliche Einheit abgeschossen wurde,
endete deshalb als FAIL — das Urteil war team-blind. Ein Ziel ist das, was dieselbe beobachtete
Tatsache von zwei Seiten lesbar macht: das FAIL des Verlierers und das SUCCESS des Schützen sind
derselbe Schuss.

| `FBObjectiveKind` | `.fbm`-Schreibweise | Bedeutung |
|---|---|---|
| `Survive` | `survive` | bis zum Ende des Laufs kampffähig bleiben |
| `KillUnit` | `kill unit <callsign>` | eine benannte Einheit kampfunfähig machen |
| `KillTeam` | `kill team <faction>` | jede Einheit einer Fraktion |
| `Waypoints` | `waypoints` | jede `wp`/`land`-Zeile des eigenen Flugplans erreichen — das, worauf eine Einheit OHNE Ziele implizit beurteilt wird; explizit deklariert ist es der Weg, es zu BEHALTEN, wenn man andere Ziele deklariert |

**Die Beobachtungstypen — bewusst minimal:**

```cpp
struct FBUnitObservation { const char *Id; FBUnitTeam Team; bool CombatEffective; };
struct FBMissionRoster   { const FBUnitObservation *Units; int Count; };
```

`FBUnitObservation` trägt **keine Position, keine Selbstauskunft, kein Modul-Handle** — nur wer es
ist, auf wessen Seite es steht, und ob es noch kämpfen kann (`FBSystemHealth::CombatEffective`). `Id`
BORGT den Namen der Einheit für den Tick. `FBMissionRoster` ist eine geborgte SICHT auf den
Pro-Tick-Puffer des Aufrufers (ein Container statt einer Sicht würde pro beurteilter Einheit pro Tick
allokieren — der Client füllt EINEN wiederverwendeten Vektor, im Tick-Pfad allokiert nichts).

**Die zwei Prädikate:**

| Funktion | Vertrag |
|---|---|
| `FBObjectiveCovers(o, id, team)` | Nennt dieses Ziel jene Einheit? Das eine geteilte Primitiv hinter BEIDEN Fragen, die an ein Ziel gestellt werden: das „ist es schon erfüllt" des Monitors und das „war dieser Verlust das erklärte Ziel von jemandem" der Kombinationsregel des Runners (`app/FBMissionRunner.cpp`). `Survive`/`Waypoints` decken nie eine Einheit ab. |
| `FBObjectiveMet(o, roster)` | Ist ein KILL-Ziel erfüllt? Jede genannte Einheit muss kampfunfähig sein, UND es muss mindestens eine geben — ein Ziel gegen eine Fraktion, der in dieser Mission niemand angehört, ist **nie** erfüllt statt trivialerweise wahr, denn eine Mission, die ihren Feind falsch schreibt, soll nicht bestehen. `Survive`/`Waypoints` werden hier NICHT entschieden. |

`FBObjectiveStr(o)` liefert die `.fbm`-Schreibweise zurück — für Logs und Parser-Fehlermeldungen.

**Es bleibt eine BEOBACHTUNG, keine BEHAUPTUNG**: ein Ziel wird von `FBMissionMonitor` gegen das
Roster ausgewertet, das der Client aus den Gesundheitsregistern füllt, die IHM gehören — genau so, wie
er das Positions-Sample des Monitors füllt. Ein Modul kann seinen Gegner so wenig für tot erklären wie
sich selbst für gelandet.

#### 5.6 `FBTeam`, `FBMode`, `FBMasterMode`, `FBArmState`

| Typ | Datei | Werte | Warum in `core/` |
|---|---|---|---|
| `FBUnitTeam` | `FBTeam.h` | `Friendly`, `Hostile`, `Neutral` | es ist BEIDES: Welt-Entitäts-Identität (`units/FBUnit`) und Missions-DATEN (eine `.fbm`-`team`-Zeile). In `units/` zu parken hieße, `core/` von `units/` abhängig zu machen, nur um eine Fraktion zu benennen; das Enum zu duplizieren gäbe Missionsdatei und Welt zwei Begriffe von „hostile". |
| `FBMode` | `FBMode.h` | `Manual`, `Direct`, `Course` | der LIVE-Autopilot-Zustand, den jede Schicht liest (FBState/HUD, Telemetrie, die Guidance-Systeme selbst) |
| `FBMasterMode` | `FBMasterMode.h` | `Nav`, `AirToAir`, `AirToGround`, `Dogfight` | die AUTORITÄT liegt am Modul, nicht global; das Enum ist geteilt, weil Input-, Display- und Weapons-Systeme es alle als Parameter nehmen |
| `FBArmState` | `FBArmState.h` | `Sim`, `Arm` | die ARM/SIM-Zeile des HUD; `FBState` trägt es, mehrere Schichten brauchen dasselbe Enum |

`FBUnitTeamStr` / `FBUnitTeamFromString` definieren die einzigen akzeptierten `.fbm`-Schreibweisen
(kleingeschrieben, exakt die `FBUnitTeamStr`-Strings) — der Parser ist überall sonst strikt, also ist
er es auch hier.

---

### 6. Schaden

Drei getrennte Dinge in einer klaren Rollenverteilung: der ZUSTAND (`FBSystemHealth`), die AUFLÖSUNG
(`FBDamageModel`) und die ZONENDATEN (Moduldaten, z.B. `modules/f16/FBF16Damage` — nicht in `core/`).

#### 6.1 `FBSystemHealth` — das Gesundheitsregister

`core/FBSystemHealth.h/.cpp`. EIN Register je `units/FBSimUnit`, dem CLIENT gehörend, ausschließlich
von einem core-eigenen Urteil gefüttert und vom Modul **gelesen, nie geschrieben**.

**Das Schreibtor ist der TYP, keine Konvention.** Jeder Mutator ist `private`, und es gibt genau EINEN
`friend`: `core/FBDamageModel`, das eine Zustandsänderung nur als Ergebnis eines aufgelösten
Waffenbursts erzeugt. Es gibt also nirgends eine API — weder auf einem const- noch auf einem
non-const-Handle —, mit der ein System, ein Pilot oder ein Modul sich selbst (oder jemand anderen)
beschädigen oder reparieren könnte. `grep -rn FBSystemHealth src/systems src/modules` findet nur
Lesezugriffe, **und kann nichts anderes finden, weil nichts anderes kompiliert**.

**MONOTON per Entscheidung**: ein Zustand verbessert sich nie. Es gibt keine Reparatur im Flug, und
ein monotones Register ist das, was das Schadensbild eines Laufs zu einer Funktion allein der
empfangenen Bursts macht — keine Reihenfolgefrage, kein Heilungs-Race zwischen zwei Beobachtern.

**`FBSystemId`** — das adressierbare Inventar. Bewusst der Modul-SLOT-Satz plus die drei physischen
Dinge, die ein Treffer ausschalten kann und die keine Avionikboxen sind (Triebwerk, Flugsteuerung,
Struktur), weil genau deren Folge JSBSim selbst tragen kann. **Nur anhängen**: das Ordinal ist
telemetriesichtbar (die `dmg_*`-Bitmasken).

| Ordinal | Id | String | Bedeutung |
|---|---|---|---|
| 0 | `Engine` | `engine` | Antrieb: Schub |
| 1 | `FlightControls` | `flight_controls` | FLCS/Hydraulik: Ruderautorität |
| 2 | `Structure` | `structure` | Zelle: Widerstand |
| 3 | `AirData` | `air_data` | ADC + Sonden |
| 4 | `RadarAlt` | `radar_alt` | Radarhöhenmesser (CARA) |
| 5 | `Nav` | `nav` | INS/Navigation |
| 6 | `Radar` | `radar` | das aktive Luft-Luft-Set |
| 7 | `FireControl` | `fire_control` | der Startbereich des Feuerleitrechners |
| 8 | `Stores` | `stores` | SMS: Träger und Verkabelung |
| 9 | `Datalink` | `datalink` | das Netzterminal |
| 10 | `Rwr` | `rwr` | der Warnempfänger |
| 11 | `Countermeasures` | `countermeasures` | der Werfer |
| 12 | `Gun` | `gun` | Bordkanone: Trommel, Zuführung, Rohre — ANGEHÄNGT nach der eigenen Regel des Enums |

**`FBHealthState`** und was er dem Konsumenten bedeutet (die Kopplung, die dieser Datei ihren Sinn
gibt):

| Zustand | Bedeutung |
|---|---|
| `Intact` (0) | das System läuft und publiziert seinen Ausgabeblock normal |
| `Degraded` (1) | es läuft und publiziert weiter, mit reduzierter Leistung **dort, wo eine reduzierte Leistung ABLEITBAR ist** (Radarreichweite, Triebwerksdecke, FLCS-Autorität). Wo nicht, hat ein System kein degradiertes Verhalten, und sein Layout-Eintrag erzeugt schlicht keines |
| `Failed` (2) | das System läuft nicht und publiziert nicht. Sein Block wird `Invalid`, und alles Weitere folgt von selbst aus dem, was der Avionikbus ohnehin tut: das HUD strichelt, der Warnsatz meldet die Bedingung als INHIBIERT statt als abwesend, und der Pilot verweigert die Handlungen, deren Datenbasis weg ist. **Nichts davon ist hier nachimplementiert; dass es umsonst passiert, ist der Grund, warum der Bus überhaupt Gültigkeitsköpfe trägt.** |

**Die Lese-API**: `State(id)`, `Ok(id)`, `Degraded(id)`, `Failed(id)`, `Working(id)` (= nicht
`Failed` — das Tor, das ein Modul fragt, bevor es einen Slot taktet), `Damaged()`, `FailedMask()`,
`DegradedMask()`, `Hits()`.

**`CombatEffective()` — die Missions-Frage und eine ausgesprochene Modellentscheidung:**

```cpp
bool CombatEffective() const {
  return !Failed(Engine) && !Failed(FlightControls) && !Failed(Structure);
}
```

Ein Flugzeug ist kampfunfähig, sobald die ZELLE die Sortie nicht mehr zu Ende bringen kann.
**Avionikverluste, wie total auch immer, gehören ausdrücklich NICHT dazu**: ein Jet mit totem Radar
und toten Trägern ist aus dem Kampf, fliegt aber noch — und was dieses Prädikat füttert
(`core/FBMissionMonitor`) beurteilt die SORTIE, nicht das Gefecht. **Die Einheit ist nicht „tot", wenn
das falsch wird**: sie fliegt genau so lange weiter, wie die Physik es zulässt.

**Die privaten Mutatoren (nur für `FBDamageModel`):**

| Methode | Vertrag |
|---|---|
| `Worsen(id, s)` | monoton: `if (s <= cur) return false`. Setzt die Bitmasken konsistent nach (erst beide Bits löschen, dann das richtige setzen). Liefert `true`, wenn dieser Aufruf tatsächlich etwas geändert hat. |
| `NoteHit()` | Trefferzähler |
| `AddKinetic(zone, fluxJm2)` | **das eine Stück Schadenszustand, das keinem System gehört**: wieviel Flächenenergie eine ZONE dieser Zelle kumulativ von GESCHOSSEN genommen hat. Liefert die neue Summe. Es existiert, weil eine Kanone ein kontinuierlicher Strom ist, den dieser Simulator notwendig in Pro-Tick-Bündel schneidet (`core/FBGun.h`) — jedes Bündel für sich zu beurteilen machte den Schaden zu einer Funktion der TICKRATE, was Prinzip 4 gerade verbietet. **Ein Gefechtskopf hat kein Äquivalent und benutzt das nicht: ein Burst ist EIN Ereignis, und die Energie eines Ereignisses ist, was sie ist.** `kMaxZones = 5` (die `FBDamageZone`-Werte inkl. `None`). |

**`FBSystemHealthTelemetry`** — eigene Quelle (`"dmg"`), von der Einheit ZULETZT registriert, nach der
Anhänge-Regel: `dmg_hits`, `dmg_failed` (Bitmaske), `dmg_degraded`, `dmg_effective`.

#### 6.2 `FBDamageModel` — die Auflösung

`core/FBDamageModel.h/.cpp`. Der EINE Schreiber von `FBSystemHealth`, dem Client gehörend. Ein Modul
löst seinen eigenen Schaden so wenig auf, wie es seinen eigenen Absturz beurteilt.

**Es ist ein MODELL, und es sagt das.** Nichts hier ist eine Messung. Was BEOBACHTET und prüfbar ist,
ist die EINGABE: die Burst-Geometrie (die eigene Closest-Approach-Rechnung des Runners auf den
publizierten Posen), die Annäherungsrate und die Gefechtskopfmasse aus dem Store-Katalog. MODELLIERT
ist der Schritt von diesen drei Zahlen zu einem Systemzustand, und er ist aus den zwei Dingen gebaut,
die tatsächlich Physik sind — isotrope Splitterausbreitung und kinetische Energie — plus einer
Schwelle je System, und die ist eine Setzung.

##### Die Energiekette, in drei Schritten mit je genannter Annahme

| Schritt | Formel | Annahme |
|---|---|---|
| 1. SPLITTERMASSE | `m_frag = kCaseFraction · WarheadKg` | `kCaseFraction = 0,5` **[SET]** — die übliche Größenordnung für ein Splitter-Sprengmantelgehäuse; `doc/f16/weapons.md` §4.7 führt Gefechtskopf-Interna als echte Lücke, also ist das eine erklärte Setzung und kein Zitat |
| 2. FLÄCHENDICHTE | `ρ_A = m_frag / (4π r²)` [kg/m²] | die Splitter breiten sich ISOTROP aus. Das ist die EINE geometrische Annahme des Modells — ein echter Gefechtskopf sprüht in ein fokussiertes Band, was das Ergebnis vom Winkel des Bursts zur Raketenachse abhängig machte, und nichts hier behauptet, dieses Band zu kennen |
| 3. SPEZIFISCHE ENERGIE | `v_eff = sqrt(v_eject² + v_closure²)`, `flux = ½ · ρ_A · v_eff²` [J/m²] | jeder Splitter kommt mit der VEKTORSUMME aus seiner Ausstoßgeschwindigkeit (`kFragSpeedMs = 1800 m/s` **[SET]**, radial) und der Annäherung der beiden Flugzeuge an. Für ein radialsymmetrisches Sprühbild ist der mittlere Betrag dieser Summe genau `sqrt(v_eject² + v_closure²)` — **bewusst nicht `v_eject + v_closure`**, was nur für die geradeaus geworfenen Splitter gälte |

**Das Ergebnis ist ein 1/r²-Gesetz in der Energie: doppelte Fehldistanz = ein Viertel der Ankunft.**
Das, und nicht irgendeine einzelne Schwelle, ist es, was das Modell auf Entfernungen vernünftig
verhalten lässt, an denen es niemand kalibriert hat.

**Der Entfernungsboden**: `r = max(rangeM, 0,5 m)` — keine physikalische Aussage, sondern ein Schutz:
das 1/r²-Gesetz divergiert bei null, und ein Burst INNERHALB der Zelle ist nicht lehrreicher als einer
gegen ihre Haut. 0,5 m ist etwa die halbe Breite eines Jägerrumpfs, also das Nächste, was ein Burst an
der Achse sein kann und trotzdem außerhalb des Flugzeugs ist.

Die Funktion ist ÖFFENTLICH, damit ein Report, ein Harness oder eine Log-Zeile die exakte Zahl hinter
einem Schadensurteil reproduzieren kann, statt ihr zu glauben:

```cpp
double FBFragmentFluxJm2(double warheadKg, double rangeM, double closureMs);
```

##### Die Zonen

Ein Flugzeug ist kein Punkt: WO der Burst relativ zur ZELLENACHSE sitzt, entscheidet, welche Systeme
in seiner Nähe sind. Das LAYOUT (Moduldaten) schneidet die Zelle entlang ihrer eigenen Längsachse in
Zonen und benennt, welche Systeme in jeder sitzen; diese Datei rechnet PRO ZONE den Abstand vom Burst
zu DEREN Achsabschnitt und den Fluss dort.

**Jede Zone wird ausgewertet, nicht nur die nächste**: Splitter gehen überall hin, sie kommen nur
weiter draußen dünner an — und das 1/r²-Gesetz das sagen zu lassen ist ehrlicher, als die Zelle zu
partitionieren und einer Partition alles zu geben. **Die Zelle ist als jener Achsabschnitt modelliert
und nichts weiter** — kein Querschnitt, keine Abschirmung, keine Splitterzahl.

`ZoneRangeM(b, z)`: die Längskoordinate wird in den Abschnitt der Zone GEKLEMMT, die Quer-/
Vertikalversätze werden unverändert mitgeführt. Ein Burst querab der Mitte einer Zone ist damit so nah
wie sein seitlicher Fehler; einer vor der Nase muss zusätzlich an der Achse zurückreichen.

```
FBDamageZone { None=0, Nose, Forward, Center, Aft }   // Namen generisch; WO sie sitzen ist Moduldaten
FBZoneSystem { FBSystemId Id; double DegradeJm2; double FailJm2; }
FBDamageZoneSpec { FBDamageZone Zone; double AftM, FwdM; const FBZoneSystem *Systems; int SystemCount; }
```

`AftM`/`FwdM` sind Meter vom CG, **+ = vorwärts**, also `AftM < FwdM`. Ein System ohne ableitbares
degradiertes Verhalten deklariert schlicht `DegradeJm2 == FailJm2` und hat deshalb nie eines. Alles
sind einfache Arrays mit Zählern — das ganze Layout ist eine Compile-Zeit-Tabelle, die ein Modul per
const-Referenz herausgibt; nichts allokiert.

##### `FBDamageLayout` — plus die Geometrie, die nur ein Geschossstrom braucht

| Feld | Bedeutung |
|---|---|
| `Zones`/`ZoneCount` | die Zonentabelle |
| `FrontalAreaM2` | präsentierte Fläche von vorn/hinten |
| `LateralAreaM2` | präsentierte Fläche von der Seite/von oben |
| `FrontalExtentM` | wie weit die Zelle in dieser Sicht REICHT: halbe Spannweite von achtern |
| `LateralExtentM` | halbe Länge von der Seite |

Ein Gefechtskopf sprüht isotrop, und der Querschnitt der Zelle kommt in seiner Arithmetik nie vor. Ein
Feuerstoß ist ein schmales Muster, das entweder auf dem Flugzeug landet oder nicht — dort entscheidet
die PRÄSENTIERTE FLÄCHE, wieviel ankommt. **Zwei Zahlen statt einer**, weil der Unterschied bei jedem
Jäger ein Faktor drei ist und die Interpolation gratis ist. **Zwei MASSSTÄBE** (Fläche + Ausdehnung),
weil die Fläche sagt, wieviel Material da ist, und die Ausdehnung, wie weit draußen es verstreut ist —
ein Jäger hat viel Spannweite und wenig Material. Ein Modul, das keines von beiden deklariert (der
Default, und jeder abgeworfene Store) präsentiert nichts und nimmt keinen Kanonenschaden — was richtig
ist: eine Bombe im freien Fall ist nichts, worauf jemand schießt.

```cpp
double FBPresentedAreaM2(layout, fwd, right, down);   // |cos| · frontal + |sin| · lateral
double FBPresentedExtentM(layout, fwd, right, down);  // dieselbe Interpolation
```

Die einfachste Interpolation, die an beiden Enden exakt ist — und **keine Behauptung über die Form
dazwischen**. Der Richtungsvektor ist im KÖRPERRAHMEN des Ziels und muss nicht normiert sein.

##### Die zwei Eingänge

```cpp
struct FBBurst        { FwdM, RightM, DownM; ClosureMs; WarheadKg; };      // Gefechtskopf
struct FBKineticBurst { FwdM; FluxJm2; SpreadM; Rounds; ImpactSpeedMs; };  // Geschossstrom
```

**Warum zwei Typen statt eines Flags**: die beiden Waffenwirkungen sind durch VERSCHIEDENE Dinge
bekannt. Ein Gefechtskopf ist durch seine MASSE bekannt, und das Modell leitet daraus ab, welche
Energie eine Fläche erreicht. Ein Feuerstoß ist durch die ENERGIEDICHTE bekannt, die der Besitzer der
Simulation bereits aus Trefferzahl, Aufschlaggeschwindigkeit und Streuung gerechnet hat
(`core/FBGunBallistics.h`s `FBGunFluxJm2`) — eine Zahl, die diese Datei nicht neu herleiten darf und
könnte, denn sie sieht nie ein Geschoss.

**Was sie TEILEN, und warum das legitim ist**: das ZIEL. Beide drücken das Ankommende als J/m²
Flächenenergie an einer Stelle der Zelle aus, und beide werden gegen dieselben Pro-System-Schwellen
beurteilt, sodass EIN Schadensregister für beide antwortet, ohne einen zweiten, unkalibrierten
Zahlensatz. **Das ist eine ausgesprochene Modellentscheidung und keine physikalische Behauptung**:
20-mm-Einschläge und Gefechtskopfsplitter beschädigen Struktur nicht über denselben Mechanismus, und
beide als Flächenenergie auszudrücken ist die gemeinsame Währung dieses Simulators, nicht die Aussage,
dass sie äquivalent wären.

##### Die zwei Auflösungen im Vergleich

| | `Apply` (Gefechtskopf) | `ApplyKinetic` (Geschossstrom) |
|---|---|---|
| Erreicht | JEDE Zone (isotropes Sprühbild), mit 1/r²-Abfall je Zone | nur die Zonen, die der FUSSABDRUCK überlappt: `[FwdM − half, FwdM + half]` mit `half = max(SpreadM, 0,5 m)` |
| Fluss | pro Zone neu gerechnet (`FBFragmentFluxJm2`) | vom Aufrufer geliefert |
| Summierung | **KEINE** — eine Detonation ist EIN Ereignis, seine Energie ist, was sie ist | **JA**, pro Zone kumulativ (`FBSystemHealth::AddKinetic`): fünfzig Geschosse in fünf Bündeln richten den Schaden von fünfzig Geschossen an; sonst hinge der Schaden an der Tickrate |
| `res.RangeM` | Abstand Burst → Struktur der Spitzenzone | **0,0** — ein Treffer, kein Abstandsburst: es gibt keine Entfernung |
| Der 0,5-m-Boden | Entfernungsboden gegen die 1/r²-Divergenz | Fußabdruck-Boden, damit ein Burst aus nächster Nähe (Muster zentimeterbreit) auf der durchquerten Zone landet statt auf einem mathematischen Punkt zwischen zweien |

Beide gemeinsam: `NoteHit()` einmal je Burst; Schwellenlogik `want = Failed` wenn
`FailJm2 > 0 && flux >= FailJm2`, sonst `Degraded` wenn `DegradeJm2 > 0 && flux >= DegradeJm2`;
`Worsen` entscheidet, ob sich wirklich etwas geändert hat; ein Layout ohne Zonen (der Default jedes
Moduls, das keines deklariert hat — z.B. ein abgeworfener Store) nimmt keinen Schaden und liefert ein
leeres Ergebnis.

```cpp
struct FBDamageResult {
  FBDamageZone Zone;      // die Zone mit dem höchsten Fluss
  double RangeM;          // Burst -> Struktur dieser Zone
  double PeakFluxJm2;
  uint32_t NewlyFailed, NewlyDegraded;   // Bitmasken: was DIESER Burst geändert hat
  bool WasEffective, NowEffective;
  bool Changed() const;
};
```

**DETERMINISMUS IST STRUKTURELL**: keine Zufallszahl irgendwo in dieser Datei, keine Zeitabhängigkeit,
kein verborgener Zustand. Gleiche Geometrie, gleicher Gefechtskopf, gleiche Annäherung → immer
dieselben Masken (thread-unabhängig nachgemessen).

##### Die physischen Folgen — die Konstanten mit Herleitung

Alle laufen durch JSBSim (`units/FBSimUnit::ApplyDamageToAirframe` → `fdm/FBFdm`), nie durch ein
zweites, paralleles Flugmodell.

| Konstante | Wert | Herleitung |
|---|---|---|
| `kAuthorityDegraded` | **0,5** | halbe kommandierte Ruderausschläge. **[SET, aber die eine Zahl mit strukturellem Grund]**: die F-16 hat ZWEI unabhängige Hydrauliksysteme, die ihre Aktuatoren treiben — eines zu verlieren ist die natürliche Bedeutung von „degradiert". |
| `kAuthorityFailed` | **0,0** | keine Autorität: die Ruder antworten nicht mehr, das Flugzeug fliegt auf dem, was an Trimm und Stabilität übrig ist — genau das Departure, das JSBSim dann von selbst integriert |
| `kThrottleLimitDegraded` | **0,6** | Nachbrenner weg: der Gashebel kann nicht über Militärleistung hinaus kommandiert werden. 0,6 ist, wo das AB-Tor in der eigenen `throttle-cmd-norm`-Konvention des F-16-Modells liegt **[DERIVED]**. Ausgefallen = Fuel-Cutoff, also JSBSims eigenes Engine-out — hier wird kein Schubterm erfunden. |
| `kDamageDragFt2Degraded` / `kDamageDragFt2Failed` | **1,5 / 6,0 ft²** | Kampfschaden ist Löcher und aufgerissene Haut: Zusatzwiderstand, angelegt als WIDERSTANDSFLÄCHE über dieselbe `<external_reactions>`-Mechanik wie der Zuladungswiderstand (`fdm/FBFdm::SetDamageDrag`), **durch den CG**, damit kein Nickmoment behauptet wird, das niemand belegen kann. **[SET]** — zur Einordnung: die eigene Nullauftriebs-Widerstandsfläche einer sauberen F-16 liegt in der Größenordnung 4 ft², eine degradierte Zelle ist also „merklich schmutzig" und eine ausgefallene „fliegt mit einem Loch drin". |
| `kRadarRangeDegraded` | **0,70710678…** | **[DERIVED]** über die Radargleichung: ein degradiertes Set hat die halbe Antennenapertur, `R⁴ ~ Pt·G²` mit `G ~ A`, also `R ~ sqrt(A)` — halbe Apertur = 1/√2 der Reichweite |

---

### 7. Waffen-Wertetypen und Ballistik

#### 7.1 `FBStore.h` — der Store-Katalog

`core/FBStore.h`. **Jede Zahl darin stammt entweder aus dem EIGENEN gepinnten JSBSim-Modell des Stores
oder ist per genannter Formel daraus abgeleitet** — nichts an einer Waffe wird hier erfunden, denn ein
abgeworfener Store fliegt als eigene FDM-Instanz genau dieses Modells, und die Trageziffern müssen
dasselbe Objekt beschreiben.

**Warum ein Katalog und keine Klasse**: ein Store hat am Pylon KEIN Verhalten — er ist Masse,
Widerstand und ein Modellname. Sein Verhalten IST das JSBSim-Modell, das er im Moment des Abwurfs wird
(`modules/stores/FBStoreModule` bzw. `modules/missile/`). Das, was am Flugzeug zurückbleibt, ist ein
Wertetyp. Liegt in `core/` aus demselben Grund wie `FBRunway`/`FBSpawn`: der Missionsparser, das SMS
des Moduls und der app-seitige Spawn-Pfad nennen ihn alle, und keiner darf die anderen inkludieren.

```
FBStoreKind { None = 0, Mk82, Aim120 }   // anhängen; None muss 0 bleiben, damit ein genullter Block "leer" liest
```

**`FBStoreSpec`:**

| Feld | Bedeutung |
|---|---|
| `Kind`, `Key` | Ordinal; Missionsdatei-/Registry-Name |
| `FdmModel` | JSBSim-Modellverzeichnis unter der EINEN Modellwurzel (`sim/assets/aircraft`) |
| `MassLbs` | Trage-Masse |
| `DragAreaFt2` | CdA: Trage-Widerstand = dies × qbar (lbf) |
| `MaxFlightS` | Lebensdauerkappe nach Abwurf |
| `Guided` | `true`: von `modules/missile` geflogen (Suchkopf + Lenkgesetz); `false`: von `modules/stores` (integrieren und fallen) |
| `RequiresLock` | das SMS verweigert den Start ohne Feuerleitlösung |
| `FuzeRadiusM` | Näherungszünder: eine Einheit näher als dies zu passieren ist ein Treffer. **0 = gar kein Näherungszünder** (eine Bombe trifft, worauf sie landet) |
| `WarheadKg` | Spreng- + Mantelmasse — die EINE storeseitige Eingabe ins Schadensmodell. 0 = eine inerte Runde, die nichts verletzt |
| `Perf` | `FBWeaponPerf`, s.u. |

##### `FBWeaponPerf` — die Leistungstabelle des FEUERLEITRECHNERS

Die grobe Tabelle, auf der eine Startbereichs- oder Bombenfall-Rechnung läuft
(`modules/f16/FBF16FireControl`). **Bewusst eine getrennte, vereinfachte Kopie dessen, was das
JSBSim-Modell der Waffe tut**: ein echter FCC integriert eine gespeicherte Tabelle, nicht die
tatsächliche Aerodynamik der Waffe, und die Differenz zwischen beiden ist eine echte Eigenschaft jeder
je geflogenen DLZ und jedes je auf ein Ziel gelegten CCIP-Pippers. Die Abfangmission misst sie für den
gelenkten Fall (vorhergesagte Flugzeit gegen die geflogene), die Angriffsmissionen für den
ungelenkten — statt sie zu verstecken, indem man der Rechnung dieselben Zahlen füttert, mit denen die
Waffe fliegt.

| Feld | Bedeutung |
|---|---|
| `BoostThrustN`, `BoostS`, `SustainThrustN`, `SustainS` | der Motor, wie der FCC ihn kennt |
| `LaunchMassKg`, `BurnoutMassKg` | Massen |
| `DragCoefA` | Überschall-Axialkraftbeiwert auf `RefAreaM2` |
| `RefAreaM2` | Referenzfläche |
| `MinSpeedMs` | darunter kann die Runde kein Abfangen mehr fliegen |
| `ActivationRangeM` | Entfernung, bei der der Suchkopf eingeschaltet wird (der „Radar Activation Range"-Cue der DLZ, `weapons.md` §2.5) |
| `SeekerRangeM` | worauf der Suchkopf tatsächlich erfassen kann |
| `ArmingS` | Trennung + Schärfung. Bei einer gelenkten Runde deckt es auch die Motorzündung ab und setzt `Rmin`; bei einer Bombe ist es die Fallzeit, die der Zünder braucht — die eigene Zahl des Pull-up-Anticipation-Cue. Eine Größe, ein Feld. |

**Ein UNGELENKTER Store benutzt dieselbe Tabelle** und nur die vier Einträge, die ein fallender Körper
hat: `LaunchMassKg`, `DragCoefA`, `RefAreaM2`, `ArmingS`. Die Motorfelder bleiben null, weil er keinen
Motor hat, die Suchkopffelder, weil er keinen Suchkopf hat — die Tabelle ist nicht „der gelenkte
Block", sie ist das, was der Rechner über die Runde weiß.

##### `kMk82` — Mk-82, 500-lb-Freifallbombe

`doc/f16/weapons.md` §3.

| Zahl | Wert | Herleitung |
|---|---|---|
| `MassLbs` | **500,0** | das modell-eigene `<emptywt>` (`mk82.xml`: 500 LBS). Ein Objekt, eine Masse — die Zahl, die der Träger beim Abwurf verliert, ist dieselbe, mit der die abgeworfene FDM-Instanz dann fliegt |
| `DragAreaFt2` | **0,366** | der modell-eigene Nullauftriebswiderstand bei Trage-Mach, als FLÄCHE ausgedrückt, damit er mit dem Staudruck des TRÄGERS multipliziert werden kann: `mk82.xml`s CDmin-Tabelle gibt Cd = 0,144 bei M 0,8 über `<wingarea>` 2,54 ft² → **CdA = 0,366 ft²**. Bewusst der eigene Beiwert des Stores und KEIN „Drag Index" aus einem Beladungshandbuch: eine T1/T2-Quelle dafür existiert nicht (§4.5 markiert Stations-/Beladungszahlen als T4, nur Gegenprobe), und Interferenz-/Pylonwiderstand ist ein realer Effekt, den hier niemand quantifizieren kann — also ist der Trage-Widerstand exakt der Eigenwiderstand des Stores, ohne erfundenen Installationsfaktor |
| `MaxFlightS` | **300,0** | Leck-Schutz, keine Physik: ein abgeworfener Store, der nach dieser Zeit weder etwas getroffen hat noch divergiert ist, wird stillgelegt, damit ein Lauf keine Zombie-Akteure ansammelt. Fallzeiten dieser Klasse sind Zehnersekunden (§4.2), 300 s kürzt also nie eine echte Bahn ab |
| `Guided`/`RequiresLock`/`FuzeRadiusM` | `false`/`false`/**0,0** | eine Bombe hat keinen Näherungszünder; nichts löst einen Mk-82-Burst gegen ein Flugzeug auf. Was ihn LIEST, ist der Bodenburst am Aufschlagpunkt (`app/FBMissionRunner.cpp`), durch dasselbe `core/FBDamageModel` wie ein Gefechtskopf neben einem Jet |
| `WarheadKg` | **87,0** (192 lb Tritonal) | **[T3, die Standardfüllung der Mk-82]** |
| `Perf.LaunchMassKg` | **226,796** | = die modell-eigenen 500 lb, ein Objekt eine Masse |
| `Perf.RefAreaM2` | **0,235974** | = das modell-eigene `<wingarea>` (2,54 ft²), die Fläche, auf die seine ganze Widerstandstabelle bezogen ist |
| `Perf.DragCoefA` | **0,142** | **[DERIVED, und bewusst grob]**: `mk82.xml`s CDmin-Tabelle läuft von 0,140 bei M 0,2 bis 0,144 bei M 0,8 und steigt dann transsonisch steil an; der Rechner trägt EINE Unterschallzahl — das IST eine gespeicherte Tabelle —, und der daraus folgende Vorhersagefehler gegen den mach-abhängigen Widerstand des Modells ist genau das, was die CCIP/CCRP-Missionen MESSEN statt wegzutunen |
| `Perf.ArmingS` | **2,0** | **[SET]**: für die Standardzünder der Mk-82 existiert keine zitierbare Schärfverzögerung (§4.7 markiert Zünder-Interna als Lücke, §4.2s PUAC-Text gibt das KONZEPT ohne Zahl); 2 s ist die Größenordnung einer Bugzünder-Schärfnadel und ist das, woraus der Pull-up-Anticipation-Cue gerechnet wird |

##### `kAim120` — AIM-120 AMRAAM

`doc/f16/weapons.md` §2.5, §3, §4.4. Die ERSTE gelenkte Runde: `sim/assets/aircraft/aim120` — das
einzige Modell in der Wurzel OHNE Upstream-Gegenstück, weil das gepinnte Submodul keine AMRAAM hat.
Modul: `modules/missile`.

| Zahl | Wert | Herleitung |
|---|---|---|
| `MassLbs` | **335,0** | Startgewicht **[T3]** — dieselbe Zahl, auf die Struktur + Treibsatz in `aim120.xml` sich summieren, also verliert der Pylon, was die abgesetzte FDM dann fliegt |
| `DragAreaFt2` | **0,115** | **[DERIVED]**: der modell-eigene Unterschall-CA (0,43 bei Trage-Mach 0,8) über seine 0,2672 ft² Referenzfläche. Dieselbe Regel wie bei der Mk-82 — Eigenwiderstand des Stores, kein erfundener Installationsfaktor |
| `MaxFlightS` | **120,0** | **[SET]** Leck-Schutz weit jenseits jedes glaubwürdigen Gefechts (ein 40-nm-Schuss kommt in unter 90 s an, s. DLZ-Integration), nie ein Ausschalter auf einer lebenden Bahn |
| `FuzeRadiusM` | **10,0** | **[SET]**. Der Aktivradar-Näherungszünder der AMRAAM und der Letalradius ihres WDU-41/B-Splittergefechtskopfs sind mit keiner Präzision publiziert (§4.7 führt genau diese Zahlenklasse als echte Lücke). 10 m ist die konservative Lesart eines 50-lb-Splittergefechtskopfs gegen einen Jäger: nah genug, dass es ein TREFFER ist und keine Behauptung, klein genug, dass ein Lenkgesetz, das nur „ungefähr" ankommt, keinen erzielt |
| `WarheadKg` | **20,5** (45 lb, WDU-41/B) | **[T3 — die publizierte Zahl ist durchgängig „etwa 40–50 lb", und §4.7 markiert Gefechtskopf-Interna als echte Lücke]**. Es ist die EINE waffenseitige Zahl, die das Schadensmodell liest; alles andere über einen Treffer kommt aus der gemessenen Geometrie des Bursts |
| `Perf.BoostThrustN`/`BoostS` | **24020 N / 3,0 s** | aus `engine/WPU-6.xml` |
| `Perf.SustainThrustN`/`SustainS` | **6228 N / 7,7 s** | aus `engine/WPU-6.xml` |
| `Perf.LaunchMassKg`/`BurnoutMassKg` | **152,0 / 99,8** | aus dem Modell |
| `Perf.DragCoefA`/`RefAreaM2` | **0,55 / 0,02482** | aus `aim120.xml`s CA bei Mach 3 |
| `Perf.MinSpeedMs` | **340,0** | **[SET]** grob Mach 1 in Höhe: darunter hat die Runde weder die Annäherung noch den Staudruck für ein Abfangen — was die DLZ-Integration beendet |
| `Perf.ActivationRangeM` | **18520 m (10 nm)** | **[SET]** — der eigene „Radar Activation Range"-Cue der DLZ ist pro Gefecht verschieden und hat keine publizierte Konstante (§4.4 sagt das ausdrücklich); 10 nm ist die doktrinäre Größenordnung und ist, wo dieser Simulator den Suchkopf einschaltet |
| `Perf.SeekerRangeM` | **14816 m (8 nm)** | **[SET]** ebenfalls unpubliziert; **bewusst KÜRZER als die Aktivierungsentfernung**, damit der Suchkopf schon schaut, wenn das Ziel in seine Erfassungsreichweite kommt, und die Übergabe ein ERFASSUNGS-Ereignis ist, nie ein Timer |
| `Perf.ArmingS` | **1,5** | **[SET]** Trennung (0,5 s bis Motorzündung, s. `FBFdm`s Throttle-Slew) plus Zünder-Schärfverzögerung; es ist das, was `Rmin` setzt |

**Katalog-Zugriff**: `kStoreCatalogue[]`, `FBFindStore(key)` (Missionsdatei-/Registry-Name),
`FBStoreSpecOf(kind)`.

#### 7.2 Abwurf-Wertetypen

**`FBDeliveryMode { Ccip = 0, Ccrp }`** — nur anhängen: das Ordinal ist der missionssichtbare
`set attack_mode`-Wert und eine Telemetriespalte. `FBDeliveryModeStr` → `"ccip"`/`"ccrp"`.

**`FBReleaseSolution` — womit ein ungelenkter Abwurf gezielt wurde**, die Antwort der Feuerleitung im
Moment, in dem der Pickle angenommen wurde, mit der Waffe aus dem Flugzeug getragen. Das exakte
Gegenstück zu `FBWeaponTargetState` bei einem gelenkten Start, und es existiert aus demselben Grund:
**die Vorhersage muss den Jet MIT der Waffe verlassen**, damit der Besitzer der Simulation sie neben
den dann gemessenen Aufschlag legen und den Fehler beziffern kann. Nichts darin steuert etwas — eine
Bombe hat keine Lenkung; es ist ein PROTOKOLL.

| Feld | Bedeutung |
|---|---|
| `Valid`, `Mode` | gültig? in welchem Modus gelöst? |
| `ImpactLatDeg`/`ImpactLonDeg`/`ImpactElevM` | wo der Rechner sagte, dass sie landet — und die Ebene, gegen die er löste |
| `TofS` | Flugzeit |
| `AimLatDeg`/`AimLonDeg` | worauf gezielt wurde (der designierte Punkt) |
| `AimMissM` | vorhergesagter Aufschlag → Zielpunkt, im Abwurfmoment |
| `ArmMarginS` | < 0 = unter der Schärfmarge abgeworfen (ein Blindgänger) |
| `StampS` | **WANN der Rechner sie erzeugt hat.** Ein Abwurf wird vom SMS in der Stores-Kommandogruppe des Moduls beantwortet, die VOR dem eigenen Takt der Feuerleitung im selben Sensor-Sweep bedient wird — die Lösung, mit der eine Runde gestempelt wird, ist also notwendig die des VORIGEN Sweeps. Diese Verzögerung ist eine echte Eigenschaft der Busreihenfolge und bei Jägergeschwindigkeit Zehnermeter wert; sie wird deshalb PROTOKOLLIERT statt versteckt, und jede aus dieser Struktur gemachte Messung kann sagen, wieviel ihres Fehlers schlicht das Alter der Zahl ist |

**`FBStoreRelease` — ein abgeworfener Store, wie das SMS ihn übergibt:**

| Feld | Bedeutung |
|---|---|
| `Station`, `Kind`, `MassLbs`, `SimTimeS` | welche Station was wann losgelassen hat |
| `OffFwdM`/`OffRightM`/`OffDownM` | wo diese Station relativ zum CG des Trägers sitzt (Körperachsen, Meter). Der Versatz reist MIT dem Abwurf, weil das SMS das Einzige ist, das seine eigene Pylon-Geometrie kennt, und der app-seitige Spawn — der einzige Code, der ein FDM erzeugen darf (`fdm/FBFdmBoot.h`) — die neue Einheit am PYLON platzieren muss, nicht am Schwerpunkt des Trägers |
| `LauncherId`, `Target` (`FBWeaponTargetState`) | **die Startprogrammierung einer gelenkten Runde**: wer schießt, und was die Feuerleitung des Schützen im Startmoment aus dem Ziel gemacht hatte. Eine Rakete verlässt die Schiene und weiß schon, wo sie zu suchen anfangen soll — das ist es, was eine inertiale Mittelphase überhaupt möglich macht — und sie weiß, auf wessen Uplink sie danach für Korrekturen hören muss. Beide null/ungültig für einen ungelenkten Store |
| `Solution` (`FBReleaseSolution`) | die ungelenkte Hälfte derselben Idee. Ungültig für eine gelenkte Runde, die von ihrem Suchkopf gezielt wird und nicht von einer Tabelle |

#### 7.3 `FBBallistics` — wo ein ungelenkter Store landet

`core/FBBallistics.h/.cpp`. **Die EINE Arithmetik hinter BEIDEN Luft-Boden-Abwurfverfahren**
(`doc/f16/weapons.md` §2.5), damit die zwei nicht auseinanderdriften können: es ist dieselbe
Vorwärtsintegration, zwei verschiedene Fragen gestellt.

| Modus | Frage | Funktion |
|---|---|---|
| CCIP | „wenn ich jetzt auslöse, wo trifft sie?" | `FBSolveImpactPoint` |
| CCRP | „gegeben jener Punkt da unten, wann muss ich auslösen?" | dieselbe Vorhersage, auf den aktuellen Bodenkurs projiziert (`FBSolveAim`) |

**Was integriert wird — und warum bewusst NICHT das, was die Bombe dann fliegt**: die Runde wird im
Moment des Verlassens des Pylons ihre eigene JSBSim-Instanz (`modules/stores/FBStoreModule`), mit der
vollen Aerodynamik des vendored Modells — mach-abhängiger Widerstand, Auftrieb bei dem Alpha, auf das
sie sich trimmt, Nickdämpfung. Ein echter Feuerleitrechner hat nichts davon: er trägt eine
gespeicherte ballistische Tabelle (Masse, ein Widerstandsbeiwert, eine Referenzfläche) und integriert
einen Massenpunkt. Genau das tut auch dies, aus `core/FBStore.h`s `FBWeaponPerf` — DIESELBE
Tabellenstruktur, auf der schon der Startbereich der gelenkten Runden läuft, aus demselben erklärten
Grund.

**Das Modell, vollständig, mit jeder Annahme an der Oberfläche:**

```
a = -g·u_up  -  (0,5 · ρ(h) · v² · Cd · S / m) · v̂
```

Schwerkraft plus Axialwiderstand entlang des (negativen) Geschwindigkeitsvektors. **Die Dichte ist ISA
auf der AKTUELLEN Höhe der fallenden Runde** (`core/FBAtmosphere.h`), in JEDEM Schritt neu ausgewertet
— eine aus 4 km abgeworfene Bombe fällt auf ihrem Weg nach unten durch ein Drittel der Atmosphäre, ein
Einzeldichte-Ansatz (den sich der Startbereich über das flache Band eines Gefechts leisten kann) wäre
hier um mehr falsch, als der gemessene Effekt groß ist.

**NICHT modelliert** — jedes davon eine erklärte Auslassung DES RECHNERS, nicht der Simulation:
Auftrieb (die Runde ist ein Massenpunkt, der nie Alpha entwickelt), Wind (den gibt es in diesem
Simulator nicht), der Coriolis-Term, die Mach-Abhängigkeit von Cd.

**Die Aufschlagebene wird ÜBERGEBEN, nie nachgeschlagen**: diese Datei kennt kein Gelände. Der
Aufrufer liefert die Elevation, gegen die gelöst wird — bei der F-16-Feuerleitung dieselbe
Elevation-Provider-Probe, die schon der Radarhöhenmesser und die Bodenwahrheit der Mission benutzen.
Eine flache Ebene auf dieser Höhe ist genau das, was ein Jet mit barometrischer/
Steerpoint-Elevations-Entfernungslösung hat (`'B'`, derselbe Provider-Buchstabe, den auch die
Schrägentfernung trägt).

**Numerik:**

| Parameter | Wert | Herleitung |
|---|---|---|
| `kStepS` | **0,05 s** | eine bei 450 kt aus 4 km abgeworfene Mk-82 fällt ~30 s, also 600 Schritte eines Sechs-Term-Updates — billig genug für den 10-Hz-Feuerleitslot und fein genug, dass der Schrittfehler weit unter dem Modellierungsfehler liegt, den die ganze Vorhersage sichtbar machen soll (**gemessen: Halbieren verschiebt den Aufschlagpunkt um deutlich unter einen Meter**) |
| `kMaxTofS` | **120 s** | Leck-Schutz, keine Physik: nichts, was diese Datei integriert, fällt zwei Minuten |
| Integrator | **Heun** (Prädiktor + Korrektor auf demselben Beschleunigungsgesetz) | der Widerstandsterm ist quadratisch in der Geschwindigkeit, und einfaches Euler verzerrt bei dieser Schrittweite die Reichweite über einen langen Fall um einige Meter — ein Fehler derselben Größenordnung wie der gemessene Effekt, also keiner, den man gratis hinnimmt |
| Aufschlag-Interpolation | linear im Schritt, der die Ebene kreuzt | die ganze Vorhersage ist eine Sub-Meter-Aussage, und den Aufschlagpunkt auf ein 0,05-s-Raster zu quantisieren würfe bei Abwurfgeschwindigkeit ~15 m Reichweite weg |

`k = 0,5 · DragCoefA · RefAreaM2 / LaunchMassKg` (also `Widerstand/(ρ·v·v)`).

**`FBReleaseState`**: `LatDeg`, `LonDeg`, `AltM`, `VelE`, `VelN`, `VelU` — die Pylonposition und der
vom Träger geerbte Geschwindigkeitsvektor (ENU m/s, geodätische Grad, m ASL).

**`FBImpactPrediction`**: `Valid`, `LatDeg`/`LonDeg` (**geodätisch, daher `double`**: ein `float`
trägt ~1e-5° = einen Meter, und der Meter ist die gemessene Größe), `ElevM` (die Ebene des Aufrufers),
`TofS`, `RangeM`, `BearingDeg` (true, 0..360), `ImpactSpeedMs` (womit sie ankommt — die Annäherung,
mit der ein Bodenburst aufgelöst wird), `ArmMarginS`.

`Valid == false` bei einer nicht integrierbaren Tabelle (keine Masse, keine Referenzfläche) oder bei
einem Abwurf schon auf/unter der Aufschlagebene.

**`ArmMarginS` = `TofS − ArmingS`** — der Pull-up-Anticipation-Cue (`weapons.md` §2.5: die
Höhenmarge, die ein Abwurf braucht, „for the fuze to arm"), ausgedrückt als die Marge, die die
Rechnung tatsächlich erzeugt: wieviel Fall übrig ist, NACHDEM die Schärfverzögerung abgelaufen ist.
Negativ = ein Abwurf von hier kommt unscharf an (der Blindgänger-Fall des Guides). Der echte Jet
zeichnet das als eine auf den FPM zulaufende Bildschirmposition; eine Marge in Sekunden ist dieselbe
Tatsache in der Form, in der eine Entscheidung getroffen wird, und sie braucht keine zweite
Integration. Ein Store ohne deklarierte Verzögerung lässt sie gleich der ganzen Fallzeit.

**`FBSolveAim` — dieselbe Vorhersage, gegen ein Ziel gemessen**: BEIDE Punkte (der vorhergesagte
Aufschlag und der designierte Zielpunkt) werden auf den AKTUELLEN Bodenkurs des Flugzeugs projiziert
(`FBTrackProjectM`) — das ist die Achse, auf der ein Freigabe-Cue lebt: die Runde kann darauf nur
durch WARTEN verschoben werden, und quer dazu nur durch DREHEN. Eine Projektion beantwortet beide
Modi; **es gibt bewusst keine zweite Geometrie für den zweiten Modus**.

| `FBAimSolution` | Bedeutung | Wer liest es |
|---|---|---|
| `AlongErrM` | + = die Runde fiele ZU KURZ; 0 = jetzt auslösen | CCRP |
| `CrossErrM` | + = sie fiele RECHTS daneben (Steuerlinienfehler) | beide |
| `MissM` | beide kombiniert — Abstand des CCIP-Pippers vom Ziel | CCIP („bin ich drauf") |
| `TimeToGoS` | `AlongErrM` bei aktueller Bodengeschwindigkeit; < 0 = der Auslösepunkt ist vorbei | CCRP-Countdown |

**Ein Freigabe-Cue braucht einen KURS**: beide Fehler sind Projektionen auf die Bewegungsrichtung, und
ein Flugzeug, das sich nicht bewegt, hat keine — ein stehender (oder noch nicht geschrittener) Zustand
liefert deshalb KEINE Lösung statt einer Nulllösung (`groundSpeedMs <= 1,0`). Das ist der Unterschied
zwischen „jetzt auslösen" und „keine Antwort".

#### 7.4 `FBGun.h` — der Kanonen-Katalog

`core/FBGun.h`. Das Geschwister von `FBStore.h`, und bewusst eine SEPARATE Datei, weil sich die beiden
Waffen in ihrer ART unterscheiden: ein Store ist ein Objekt, das an einem Pylon hängt und im Moment
des Abwurfs seine eigene JSBSim-Einheit wird; eine Kanone ist eine feste Installation, die das
Flugzeug nie verlässt und deren Produkt ein Strom von Geschossen ist, viel zu zahlreich, um überhaupt
Einheiten zu sein (6.000 rd/min gegen einen 0,1-s-Tick sind **zehn Geschosse pro Tick pro feuerndem
Flugzeug**).

**DIE EINE MODELLENTSCHEIDUNG**: ein FEUERSTOSS IST EIN BALLISTISCHES BÜNDEL. Jedes Geschoss, das ein
Tick Abzugsdruck erzeugt, teilt EINEN Startpunkt, EINE Startgeschwindigkeit und EINE Integration
(`core/FBGunProjectiles`); was es zu einem Feuerstoß statt zu einem Einzelschuss macht, ist, dass es
eine ZAHL und einen STREUWINKEL trägt und dass beide als DICHTE in die Trefferauflösung eingehen, nie
als Position. **Über die Position eines einzelnen Geschosses wird nichts behauptet, denn nichts hier
kennt sie.**

**Was Physik ist und was Modellierung** (der ganze Sinn der Aufteilung — keine Zahl unten kann für
eine Messung gehalten werden):

| Kategorie | Inhalt |
|---|---|
| **PHYSIK** (integriert, nicht tabelliert) | die Bahn des Bündels. Mündungsgeschwindigkeit addiert sich zum eigenen Geschwindigkeitsvektor des Flugzeugs, Schwerkraft wirkt, quadratischer Widerstand bremst gegen die ISA-Dichte auf der eigenen Höhe (`core/FBGunBallistics.h`). Flugzeit, Fall und Aufschlaggeschwindigkeit sind damit GERECHNET, und die Vorhaltelösung der Feuerleitung ist ein Solve gegen dieselbe Bahn statt eines Lookups |
| **MODELLIERUNG** | (a) dass ein Bündel für N Geschosse steht, (b) dass die Geschosse darin als zirkulare Normalverteilung um seine Achse liegen, (c) dass ein Treffer eine ERWARTETE Geschosszahl und eine Flächenenergiedichte ist statt einer Menge von Einzeleinschlägen. (b) ist an die eine Streuungsangabe der Quellen GEFITTET (s. `kM61A1`); (c) ist, was das Modell deterministisch macht — **es gibt keine Zufallszahl irgendwo im Kanonenpfad** |
| **WEDER NOCH, und als abwesend erklärt** | Rohrverschleiß, Schuss-zu-Schuss-Geschwindigkeitsstreuung, Leuchtspur/HEI/API-Mischung (§3 listet sechs Munitionstypen; die Trommel hier ist EIN homogenes Geschoss) und die Masse der Munition selbst |

##### `kM61A1` — die M61A1 Vulcan

`doc/f16/weapons.md` §2.5, §3, §4.1. Quelle und Konfidenz je Zahl:

| Feld | Wert | Herleitung |
|---|---|---|
| `MuzzleVelMs` | **1030** | 3.380 ft/s für Standardgeschosse **[T4, §4.1 — „consistent across sources, no T1/T2 found"]**. PGU-28/B ist 20 m/s schneller; modelliert ist EIN Geschosstyp |
| `RoundsPerMin` | **6000** | **[ED-Zahl, §2.5 und §3 stimmen überein]** |
| `Capacity` | **510** | **[ED §3s Trommelzahl.** §2.5 nennt im selben Guide 512; die beiden differieren um zwei Schuss, und §3 ist die Spezifikationstabelle, also gewinnt §3 — **die Diskrepanz wird notiert statt gemittelt]** |
| `SpoolUpS` | **0,3 s** | **[T4, §4.1 — dort als T1/T2-bedürftig markiert.** Es ist modelliert, weil es wegzulassen still volle Rate ab dem ersten Moment behauptete, was der GRÖSSERE Fehler ist: bei 6.000 rd/min sind das ~15 Schuss je Betätigung] |
| `RoundMassKg` | **0,100** | **[SET — und dies ist die eine Zahl, die §4.1 ausdrücklich nicht zertifiziert: „do not treat the ED dispersion footnote's projectile mass as authoritative spec data".** FlightBox braucht eine Masse, um einen Treffer in Energie zu verwandeln, und benutzt die ~100-g-Klasse als erklärte SETZUNG. **Jede kinetische Schadenszahl dieses Simulators ist linear darin**, weshalb sie hier und nirgendwo sonst benannt ist] |
| `RoundDiaM` | **0,020** | 20×102 mm **[ED §3]** — das Kaliber, also die Widerstands-Referenzfläche |
| `DragCoef` | **0,30** | **[SET]** für ein drallstabilisiertes Überschallgeschoss. Kein Zitat, aber prüfbar statt frei: mit obiger Masse und Kaliber ergibt sich eine Flugzeit auf 1.000 m von ~1,3 s (`make -C sim test-gun` druckt sie), was die Größenordnung ist, die jede publizierte 20-mm-Schusstafel zeigt |
| `DispersionSigmaRad` | **2,2295e-3** | **[DERIVED** aus der einen Streuungsspezifikation der Guides, §2.5s MIL-DTL-45500/1A-Zitat: „80 % eines 75-Schuss-Feuerstoßes innerhalb eines 8,0-in-Kreises auf 1.000 in", also **80 % innerhalb eines 4-mil-RADIUS**. Für ein zirkular-normales Muster ist `P(r<R) = 1 − exp(−R²/2σ²)`, also **σ = 4 mil / sqrt(2·ln 5) = 2,2295 mil**. Der Fit ist gegen die ZWEITE Zahl desselben Zitats prüfbar, die nicht zum Fitten benutzt wurde: er sagt 97,3 % innerhalb des 12-mil-Kreises (6 mil Radius) voraus, den der Guide „100 %" nennt. Eine Gleichverteilung auf einer Scheibe — die naheliegende Alternative — hätte nur 44 % in den 8-mil-Kreis gelegt und ist damit **von der Quelle ausgeschlossen, nicht vom Geschmack**] |
| `MaxBurstS` | **1,0** | **[SET]** — der längste Abzugsdruck, den die Kanone in einem Kommando honoriert, also 100 Schuss. Ein Trigger-Kommando ist EINE Pilotenhandlung (`core/FBAvionicsCommand.h`), also braucht es eine Dauer; dies ist die Obergrenze, keine Doktrin |

**BEWUSST ABWESEND — die MASSE der Munition.** 510 Schuss sind in der Größenordnung 110 lb, und sie zu
verschießen verschöbe Gewicht und Schwerpunkt des Flugzeugs. Es ist NICHT modelliert, weil das
Leergewicht der Vanilla-`f16.xml` nicht zerlegt werden kann (Prinzip 1: das Modell ist read-only und
seine Massenaufteilung ist seine eigene) — eine Trommel als Punktmasse hinzuzufügen würde sie ebenso
wahrscheinlich doppelt zählen wie korrigieren. Die Auslassung ist unter einem halben Prozent des
Startgewichts und wird ERKLÄRT statt versteckt.

**`FBGunBurst` — ein Bündel, wie die Kanone es übergibt:** `LauncherId`, `Kind`, `Rounds`,
`LatDeg`/`LonDeg`/`AltM`, `VelE`/`VelN`/`VelU`, `SimTimeS`. **Die Geschwindigkeit ist bereits die
SUMME** aus Flugzeug- und Mündungsgeschwindigkeit entlang der Rohrachse (diese Summe ist Physik, und
die Kanone kennt beide Hälften), sodass der Empfänger ein schlichtes Geschoss integriert und über das
feuernde Flugzeug nichts wissen muss.

**Die Grenze**: das Kanonensystem produziert Burst-Datensätze und hört dort auf — genau wie das SMS
Abwurf-Datensätze produziert und dort aufhört. Was ein Geschoss einer anderen Einheit antut, löst der
CLIENT auf den publizierten Posen auf, nie das System, das gefeuert hat.

#### 7.5 `FBGunBallistics` — die geteilten ballistischen Primitive

`core/FBGunBallistics.h/.cpp`. Reine Funktionen auf Werten, kein Zustand, keine Allokation — genau
das, was die DREI Konsumenten, die sich einig sein MÜSSEN, buchstäblich dieselbe Arithmetik benutzen
lässt statt dreier Kopien davon:

| Konsument | Rolle |
|---|---|
| `modules/f16/FBF16FireControl` | rechnet die EEGS-Ziellösung VOR dem Schuss |
| `core/FBGunProjectiles` | fliegt die Geschosse DANACH |
| `app/FBTestGun` | prüft beide gegen die eigenen Zahlen von `doc/f16/weapons.md` |

**Warum das hier KEIN Cheat ist**, obwohl es bei der Rakete genau deshalb vermieden wird
(`FBWeaponPerf` ist bewusst eine grobe SEPARATE Kopie der Raketenaerodynamik): der Flug einer AMRAAM
ist der einer gelenkten Zelle — eigenes JSBSim-Modell, eigener Autopilot, eigenes Energiemanagement —
und keine Tabelle sagt ihn exakt vorher. **Ein 20-mm-Geschoss ist ein ungelenkter Klumpen auf einem
ballistischen Bogen, und der FCC der F-16 löst genau diesen Bogen.** Eine Diskrepanz zwischen beiden
zu modellieren hieße, einen Fehler zu ERFINDEN, nicht einen zu messen.

**Das Bahnmodell** (Physik, mit seiner einen genannten Vereinfachung): ein Massenpunkt unter
Schwerkraft und quadratischem Widerstand, `dv/dt = −k·v²` entlang der Geschwindigkeit, mit
`k = 0,5·ρ·Cd·A/m` gegen die ISA-Dichte auf Feuerhöhe.

**DIE VEREINFACHUNG**: der Widerstand wirkt auf die GESCHWINDIGKEIT (Betrag) und die Schwerkraft auf
die vertikale Komponente **getrennt**, statt auf die Vektorsumme. Über die ganze nutzbare Lebensdauer
eines Geschosses (unter 2 s, unter 2 km) beträgt der Fall Meter gegen einen Weg von Kilometern, der
Winkel zwischen beiden ist also klein und die Entkopplung ist ein paar Zentimeter wert. **Was sie
kauft, ist eine GESCHLOSSENE FORM** — und damit ein exaktes, iterationsfreies Inverses:

| Funktion | Formel |
|---|---|
| `FBGunRetardation(spec, rho)` | `k = 0,5·ρ·Cd·(π/4·d²)/m` [1/m] |
| `FBGunSpeedAfter(k, v0, t)` | `v(t) = v0 / (1 + k·v0·t)` |
| `FBGunPathAfter(k, v0, t)` | `s(t) = ln(1 + k·v0·t) / k` |
| `FBGunTimeToPath(k, v0, s)` | `t(s) = (exp(k·s) − 1) / (k·v0)` — das exakte Inverse. **Schutz**: `k·s > 20` → `−1`, denn `exp(k·s)` überläuft lange vor jeder Entfernung, auf der eine Kanone benutzt wird, und eine Unsinn-Eingabe soll keine Unendlichkeit in eine Pose propagieren |

Das ist es, was den Vorhalte-Solve unten in einer Handvoll fester Schritte konvergieren lässt — **ohne
Suche und ohne Pro-Frame-Allokation**.

##### Das Treffer-/Energiemodell

**`FBGunFluxJm2(rounds, spec, impactSpeedMs, missM, sigmaM, targetAreaM2, extentM)`** [J/m²] — dieselbe
Währung, in der der Splitterfluss von `core/FBDamageModel` ausgedrückt ist, damit EIN Schadensregister
für beide Waffenwirkungen antwortet, ohne einen zweiten Schwellensatz.

**Das Modell in einer Zeile**: die Geschosse sind ein zirkular-normales Muster der Breite `σ` um die
Bündelachse, das ZIEL ist eine Scheibe seiner präsentierten Fläche, und die erwartete Trefferzahl ist
die ÜBERLAPPUNG der beiden. Schreibt man die Zielscheibe als ihre eigene äquivalente Normalverteilung
(`σ_t² = A/(2π)`, die Breite, deren zentrale Dichte einer Scheibe der Fläche A entspricht), wird diese
Überlappung eine geschlossene Form:

```
hits = N · A/(A + 2π σ²) · exp( −d² / (2·(σ² + A/(2π))) )
```

mit `d` = Fehldistanz vom Zielmittelpunkt. Jedes Geschoss trägt `E = ½·m·v_rel²` kinetische Energie IM
BEZUGSSYSTEM DES ZIELS, und der Fluss ist diese Treffer verteilt über die KLEINERE der zwei Flächen —
das Muster (`2π σ²`) oder das Ziel.

**Jeder Grenzfall fällt aus derselben Formel:**

| Fall | Ergebnis |
|---|---|
| Muster viel größer als das Ziel | das Ziel fängt `N·A/(2πσ²)` Geschosse, und der Fluss fällt mit `1/range²`, weil σ linear mit der Entfernung wächst. **Das ist der Grund, warum eine Kanone eine Nahbereichswaffe ist — hier HERGELEITET statt durch ein Entfernungslimit aufgezwungen** |
| Muster viel kleiner als das Ziel | jedes Geschoss trifft, auf einer Fläche `2πσ²`, und der Fluss sättigt bei dem, was ein Burst aus nächster Nähe tut |
| Burst ein paar Meter daneben | die eigene AUSDEHNUNG des Ziels fängt trotzdem einen Teil — der Term, den ein Punktziel-Modell falsch macht: ein Flugzeug ist Meter breit, und ein auf seine Flügelspitze zentriertes Muster setzt Geschosse in den Flügel |

**`FBGunExpectedHits(rounds, missM, sigmaM, targetAreaM2, extentM)`** — die erwartete Trefferzahl
fürs Protokoll (der Fluss braucht sie nicht, aber ein Trefferbericht, der „0,4 Geschosse" sagt, ist
ehrlicher als einer, der „ein Treffer" sagt).

**ZWEI MASSSTÄBE, weil ein Flugzeug zwei hat.** `targetAreaM2` ist, wieviel MATERIAL es präsentiert;
`extentM` ist, wie weit dieses Material von seinem Zentrum reicht (halbe Spannweite von achtern, halbe
Länge von der Seite). Eine einzelne Scheibe der präsentierten Fläche kann beides nicht ausdrücken: sie
ist richtig für einen Burst auf dem Rumpf und sagt „gar nichts" für einen vier Meter draußen, wo eine
echte F-16 noch Flügel hat. Also sind die erwarteten Treffer das **GRÖSSERE zweier Lesarten desselben
Musters**:

| Lesart | Modell | Wo richtig |
|---|---|---|
| COMPACT | das Material als EINE Scheibe der Fläche A (`σ_t² = A/(2π)`) | exakt, wenn der Burst auf dem Zentrum liegt — wo ein tödlicher Burst liegt |
| EXTENT | dasselbe Material dünn über die ganze Silhouettenscheibe des Radius `extentM` verteilt, sodass ein Geschoss darin mit Wahrscheinlichkeit `A/(π·extent²)` etwas trifft (`σ_t² = extent²/2`) | richtig für den Flügel |

Keine der beiden ist eine Messung, und beide sind ERKLÄRT. Was das Paar kauft: **ein Burst geht nicht
über einen Meter Zielfehler von tödlich auf buchstäblich nichts.** `extentM == 0` schaltet die zweite
Lesart ganz ab (ein Ziel ohne deklarierte Ausdehnung gilt als kompakt). Ein Bündel kann nie mehr
Geschosse landen, als es hält (`hits = min(hits, rounds)`).

##### Die Vorhaltelösung

**`FBGunSolveLead(spec, altM, ownVel…, rel…, tgtVel…) → FBGunAim`**

Ein Fixpunkt-Solve EINER Gleichung: die Weglänge `s(t)` des Geschosses muss der Entfernung zu dem Ort
gleichen, an dem das Ziel bei `t` sein wird, in einem Bezugssystem, in dem das Geschoss am Flugzeug
startet. Ausgeschrieben ist die zu überbrückende Verschiebung:

```
D(t) = rel + v_target·t + up·(½·g·t²)
```

— der aktuelle Versatz des Ziels, seine Bewegung während der Flugzeit, und der Fall, ÜBER den das
Geschoss gezielt werden muss, um ihn auszugleichen. Gegeben eine Richtung, folgt `t` exakt aus
`FBGunTimeToPath`; gegeben `t`, folgt die Richtung aus `D(t)`. **Sechs Durchläufe** setzen das auf
deutlich unter einen Meter fest, auf jeder Entfernung, auf der die Kanone benutzt wird.

**Der letzte Schritt ist der, den eine naive Vorhalterechnung falsch macht**: das Geschoss verlässt
das Rohr mit der GESCHWINDIGKEIT DES FLUGZEUGS zusätzlich zur Mündungsgeschwindigkeit — die Richtung,
in die das Geschoss FLIEGT, ist also NICHT die Richtung, in die das Rohr ZEIGT. Das ist geschlossen
lösbar: die Eigengeschwindigkeit in Komponenten längs und quer zur geforderten Flugrichtung zerlegen;
das Rohr muss genau so weit quer angestellt werden, dass die Mündungsgeschwindigkeit die
Querkomponente aufhebt:

```
μ    = sqrt(v_muzzle² − |v_own_quer|²)
bore = (μ·flightdir − v_own_quer) / |…|
v0   = v_own_längs + μ
```

**Diese Anstellung ist der Grund, warum die Geschosse eines hart kurvenden Jägers dorthin gehen, wo
seine Nase nicht ist — und sie ist der physikalische Ursprung der Form des EEGS-Trichters.**

Alles ist ENU-Meter/m-pro-Sekunde relativ zur Position des feuernden Flugzeugs.

| `FBGunAim` | Bedeutung |
|---|---|
| `Valid` | `false` = es gibt keine Lösung: das Ziel läuft schneller, als das Geschoss aufholen kann (`v0 <= 1`), oder das Flugzeug quert so schnell, dass die Mündungsgeschwindigkeit es nicht aufheben kann (`cross² >= v_muzzle²` — kann mit 1.030 m/s und einem Flugzeug nicht passieren, **wird aber geprüft statt angenommen**) |
| `TofS` | Geschossflugzeit zum Schnittpunkt |
| `RangeM` | Entfernung zu diesem Punkt |
| `BoreE`/`BoreN`/`BoreU` | Einheitsvektor, entlang dessen die Kanone zeigen muss |
| `SpreadM` | Sigma des Musters dort (`DispersionSigmaRad × Weglänge`) |
| `ImpactSpeedMs` | Geschossgeschwindigkeit RELATIV zum Ziel bei Ankunft (`v(t) − v_target,längs`, auf ≥ 0 geklemmt) — die Energie ist die, die das Ziel SIEHT, sodass ein Head-on-Burst härter ankommt als eine Heckverfolgung, ohne dass das irgendwo separat gesagt werden müsste |

`kGravityMs2 = 9,80665`.

#### 7.6 `FBGunProjectiles` — die Geschosse in der Luft

`core/FBGunProjectiles.h/.cpp`. Ein fester Pool ballistischer BÜNDEL, dem CLIENT gehörend, von ihm
geschritten und von ihm gelesen, um aufzulösen, was ein Feuerstoß getroffen hat — das strukturelle
Geschwister von `core/FBDamageModel` in jeder Hinsicht, die zählt:

- ein Modul kann ihn weder erreichen noch eines konstruieren, **also fliegt kein Flugzeug seine eigenen
  Geschosse und keines kann entscheiden, was sie angerichtet haben**;
- nichts darin ist zufällig, zeitabhängig oder verborgen: derselbe Burst aus derselben Geometrie
  fliegt dieselbe Bahn — was ein Kanonengefecht über Threadzahlen hinweg reproduzierbar macht;
- **es allokiert nichts.** Der Pool ist ein schlichtes Array; ein Bündel, das nicht aufgenommen werden
  kann, wird GEZÄHLT (`DroppedCount()`) statt still verloren zu gehen, denn ein Pool, der einen
  Feuerstoß stillschweigend fräße, brächte die Arithmetik eines Magazins zum Nicht-mehr-Aufgehen.

**Warum ein Bündel keine `units/FBUnit` ist**: ein abgeworfener Store WIRD eine Einheit, weil er EIN
Objekt mit eigener Zelle, eigenem FDM und eigenem Urteil ist. Ein Tick Kanonenfeuer sind zehn
Geschosse, von einem Flugzeug, alle 0,1 s — ein anhaltendes Gefecht produzierte tausende davon, jedes
mit JSBSim-Instanz, Telemetriedatei und Monitor. Was die Geschosse physisch SIND, rechtfertigt das
nicht: ungelenkte Klumpen ohne Systeme und ohne Entscheidungen, an die nur eine einzige Frage gestellt
wird — wo sind sie und was haben sie getroffen. Also leben sie hier, als Arithmetik.

| Konstante | Wert | Herleitung |
|---|---|---|
| `kMaxBundles` | **64** | genug für vier kontinuierlich feuernde Flugzeuge über die ganze Lebensdauer eines Bündels (10 Ticks), mit Reserve; eine Betätigung erzeugt ein Bündel pro Tick |
| `kMaxAgeS` | **3,0 s** | Lebensdauerkappe |
| `kMaxPathM` | **3000 m** | dito; beide weit jenseits der Entfernungen, auf denen die Kanone benutzt wird (`doc/f16/weapons.md` §2.5 setzt das eigene Limit des Trichters auf 3.000 ft) |

**`Bundle`** trägt SOWOHL die vorige ALS AUCH die aktuelle Position, weil ein Treffer eine
Closest-Approach-Rechnung über das SEGMENT des Ticks ist: ein Geschoss legt ~100 m je 0,1-s-Tick
zurück, ein Pro-Tick-Abstandstest verfehlte also fast alles. Derselbe Grund, aus dem der
Näherungszünder auf Segmenten arbeitet — der Aufrufer benutzt denselben Helfer.

Felder: `Live`, `LauncherId`, `Spec`, `Rounds`, `LatDeg`/`LonDeg`/`AltM`,
`PrevLatDeg`/`PrevLonDeg`/`PrevAltM`, `VelE`/`VelN`/`VelU`, `PathM` (zurückgelegte Weglänge — **der
Hebelarm des Streumusters**), `AgeS`, `FiredS`.

**`Step(dt)`**: Widerstand auf die GESCHWINDIGKEIT (geschlossene Form), Schwerkraft auf die
Vertikale, und ein TRAPEZ-Positionsupdate auf dem Mittel der beiden Geschwindigkeiten — **zweiter
Ordnung in dt, was bei 0,1-s-Ticks und ~1.000 m/s zählt**. Die Dichte wird auf der aktuellen Höhe des
Bündels neu ausgewertet. Ein Bündel unter 1 m/s wird stillgelegt.

**`Launch(burst)`** liefert `false`, wenn der Pool voll war (und zählt `Dropped_`).
**`Retire(index)`** ist das Urteil des Aufrufers: dieses Bündel wurde gegen ein Ziel aufgelöst und ist
verbraucht — **ein Bündel kann einmal treffen**, die Geschosse, für die es stand, sind ins Ziel
gegangen.

**Was diese Klasse bewusst NICHT modelliert**: Geschosse werden NICHT bis zum Boden verfolgt, und es
gibt keinen ballistischen Geländeaufschlag. Luft-Luft-Bordkanonenbeschuss ist, wofür dieser Pool da
ist, und einen Beschuss-Fußabdruck zu behaupten, den hier nichts rechnet, wäre schlimmer als die
erklärte Abwesenheit.

#### 7.7 `FBWeaponUplink` — die Lenkfunk-Wertetypen

`core/FBWeaponUplink.h`. Was ein startendes Flugzeug einer von ihm unterstützten Rakete sendet, und
womit diese Rakete auf der Schiene programmiert wurde.

**Warum es eine ABGESTRAHLTE Signatur ist und kein Funktionsaufruf**: die Anfangslenkung der AMRAAM
ist „datalink command from the launching aircraft … transitions to onboard active radar terminal
homing" (`doc/f16/weapons.md` §2.5, §4.4). Dieser Uplink ist eine ÜBERTRAGUNG: der Schütze strahlt
ihn ab, und er hört in dem Moment auf, in dem der Schütze die Unterstützung einstellt. Also wird er in
der `FBUnitSignature` des Schützen publiziert — neben dem Datalink-XMT-Schalter und dem
IFF-Transponder, unter demselben Snapshot-Vertrag — und die Rakete LIEST ihn über ihren eigenen
Comms-Slot (`modules/missile/FBMissileUplink`), genau wie ein Empfänger jede andere Emission liest.
**Nichts reicht der Rakete einen Zeiger auf den Schützen, und nichts reicht einem von beiden die
Wahrheit.**

**Was darin reist, ist eine SCHÄTZUNG, keine Position.** `FBWeaponTargetState` ist das, was das RADAR
DES SCHÜTZEN aus dem Ziel gemacht hat: eine aus Entfernung/Peilung/Elevation an der eigenen Nase
abgeleitete Position, eine aus aufeinanderfolgenden Looks differenzierte Geschwindigkeit, und die
**SIM-ZEIT DES LOOKS**, auf dem sie steht. Die Rakete fliegt damit auf Daten, die so alt, so verrauscht
und so falsch sind wie das Sensorbild des Schützen — der ganze Sinn des Lost-Lock-Falls: hört der
Uplink auf, ist das Letzte davon alles, was die Rakete hat.

```
FBWeaponTargetState { Valid; LatDeg, LonDeg, AltM; VelE, VelN, VelU (ENU m/s); StampS }
FBWeaponUplink      { Active; LauncherId; FBWeaponTargetState Target }
```

**Keine Identität**: das Radar des Schützen weiß auch nicht, wen es ansieht (`core/FBRadarContact.h`),
also kann es die Rakete auch nicht. `Active` wird in dem Augenblick falsch, in dem die Feuerleitung
den unterstützten Track verliert, und die Rakete hat dann nichts mehr zu empfangen — der taktisch
entscheidende Moment, und der Grund, warum dies ein PUBLIZIERTER ZUSTAND ist und kein Strom von
Nachrichten, den niemand beobachten könnte. `LauncherId`: **eine Rakete hört nur auf ihren eigenen
Schützen.**

---

### 8. Sensor- und EW-Wertetypen

Die vier Kontakt-/Bedrohungstypen sind bewusst als GEGENSÄTZE konstruiert. Was sie NICHT tragen, ist
jeweils das Modell:

| Typ | Was es ist | Trägt | Trägt bewusst NICHT |
|---|---|---|---|
| `FBDatalinkTrack` | eine NACHRICHT | Callsign, Team, gemeldete Position/Vektor — Identität ist gratis | Frische (es ist immer „die letzte Nachricht, die ankam") |
| `FBRadarContact` | ein ECHO | Geometrie: Entfernung, Peilung, Winkel an der Nase, Annäherung | **Unit-Id, Callsign, Team** |
| `FBRwrThreat` | eine RICHTUNG | relative Peilung, Modus, geschätzte Emitter-Art, Letalität | **Entfernung**, sichere Identität |
| `FBEmitterSignature` | eine ABSTRAHLUNG | Modus, Art, das körperfeste Keulenfenster, das Entfernungstor | Identität, Sendeleistung, Frequenz |

#### 8.1 `FBRadarContact` + `FBIffReply`

`core/FBRadarContact.h`. Ein Rückstrahler, wie ein AKTIVES Radar ihn meldet, und der bewusste
Gegenentwurf zu `FBDatalinkTrack`. **Es gibt hier kein Callsign-Feld, kein Team-Feld und keine
Unit-Id, und diese Abwesenheit IST das Modell, kein Versäumnis** (`doc/f16/radar-sensors.md`: das FCR
verarbeitet Rückstrahler nach Entfernung/Doppler; Identifikation ist eine separate Box).

**Der eine legitime Identitätskanal ist IFF** (`doc/f16/datalink-iff.md`, AN/APX-113): der
Interrogator fordert den Kontakt heraus, und eine gültige Mode-4-Antwort BEWEIST FREUNDLICH. Alles
andere bleibt UNBEKANNT — ein Feind und ein Freund mit totem Transponder erzeugen dasselbe `NoReply`.
**Deshalb hat dieses Enum überhaupt keinen Wert „hostile":**

```
FBIffReply { NotInterrogated, NoReply, Friendly }
```

`NoReply` ist NICHT „feindlich": es ist die ABWESENHEIT EINES BEWEISES, und die beiden dürfen nie
zusammenfallen. Alles oberhalb der Sensoren, das schießen will, muss damit leben — genau wie der Pilot
des echten Jets.

| Feld | Bedeutung |
|---|---|
| `TrackNum` | die EIGENE Dateinummer des Radars (1..), in Erfassungsreihenfolge vergeben und nach einem Drop wiederverwendet. Sie existiert, damit ein Display oder ein Pilot demselben Echo über Frames folgen kann, ohne dass der Sensor das herausgeben muss, was er nicht weiß — wer das ist. **Nie eine Unit-Id.** |
| `RangeM` | SCHRÄG-Entfernung zum letzten Look (vor `LookAgeS`) |
| `BearingDeg` | true Peilung eigen → Kontakt, 0..360 |
| `ElevAngleDeg` | Elevation über der lokalen Horizontalen (+ = oben) — der WELT-bezogene Partner von `BearingDeg`, damit ein Konsument das Echo im Raum platzieren kann, ohne einen look-alten Körpervektor durch eine jetzt-aktuelle Lage zurückdrehen zu müssen. Weiterhin reine Geometrie: nennt eine Richtung, nie eine Identität |
| `AzDeg` | Azimut AN DER NASE, −180..180 (+ = rechts), körperbezogen |
| `ElDeg` | Elevation über der Boresight-Ebene (+ = oben), körperbezogen |
| `ClosureMs` | Entfernungsrate, + = annähernd |
| `LookAgeS` | Simsekunden, seit die Keule ihn zuletzt wirklich getroffen hat; > 0 = coasting |
| `Coasting` | auf dem letzten Look gehalten, in diesem Scan-Frame nicht gesehen |
| `Iff` | `FBIffReply` |

**`kMaxRadarContacts = 8`** — feste Kapazität, kein Heap: `FBState` trägt die Liste inline, ein Bildaufbau
allokiert nichts. Acht entspricht `kMaxDatalinkTracks` und übertrifft die Relevanz der zehn
TWS-Trackfiles der APG-68 für die Nahkämpfe, die dieser Simulator fliegt, bequem.

#### 8.2 `FBDatalinkTrack`

`core/FBDatalinkTrack.h`. Ein Kontakt, wie ein KOOPERATIVES Datalink ihn meldet (MIDS/Link-16, DCS'
TNDL — `doc/f16/datalink-iff.md`). **Kein Sensor-Rückstrahler**: der Sender strahlt seine eigene
INS/GPS-Position und seine eigene Identität ab, also kommen Callsign und Team gratis, und die
Genauigkeit ist die Navigationsgenauigkeit DES SENDERS, nicht die des Empfängers. Was ein Empfänger
hinzufügt, ist nur, WANN er es gehört hat — `ReportTimeS` und das daraus abgeleitete `AgeS`, weshalb
ein Track nie „live" ist: er ist die letzte Nachricht, die ankam.

| Feld | Bedeutung |
|---|---|
| `UnitId` | die Unit-Id des Senders — der Identitätsschlüssel des Tracks |
| `Callsign[25]` | der eigene Name des Senders, NUL-terminiert (`kDatalinkCallsignLen = 25`: `.fbm`-Callsigns sind 1..24 Zeichen + NUL) |
| `Team` | `FBUnitTeam` |
| `LatDeg`/`LonDeg`/`AltM` | wie GEMELDET (der eigene Positionsfix des Senders) |
| `HeadingDeg`/`SpeedMs` | der gemeldete Geschwindigkeitsvektor, in Polarform |
| `RangeM`/`BearingDeg` | EMPFÄNGERSEITIG gerechnet: eigene Position → gemeldete Position |
| `ReportTimeS` | Simzeit der Nachricht, auf der dieser Track noch steht |
| `AgeS` | `now − ReportTimeS`; 0 nur in dem Tick, in dem sie ankam |

**`kMaxDatalinkTracks = 8`** — feste Kapazität, kein Heap. Acht ist eine Viererrotte plus ihr Package —
genug für die Missionen dieses Simulators, und die Zahl, die die Pro-Frame-`FBState`-Kopie des
HUD-Pfads begrenzt.

#### 8.3 `FBEmitter` — was ein Radar in die Luft setzt

`core/FBEmitter.h`. Die dritte Emission in `units/FBUnit`s `FBUnitSignature` (nach dem
Datalink-Sender und dem IFF-Transponder) und **die erste, die eine RICHTUNG hat** — der ganze Grund,
warum es diese Datei gibt.

**EIN RADAR STRAHLT NICHT IN ALLE RICHTUNGEN.** Es steckt seine Energie in eine Keule, und wohin die
zeigt, entscheidet, wer sie hört — eine Signatur, die nur „Radar an/aus" wäre, modellierte einen
allwissenden Warnempfänger, und genau das ist die echte Box laut `doc/f16/defence-rwr-cm.md` §2.1
nicht („a geometry-gated emission detector, not a ground-truth threat oracle"). Was hier reist, ist
also die Geometrie der Keule, **KÖRPERBEZOGEN auf das abstrahlende Flugzeug**: der Empfänger hat
dessen publizierte Pose (`FBUnit::GetPose`) bereits, kann sich also mit DERSELBEN Transformation in
dessen Rahmen drehen, die die Antenne des Emitters für sich selbst benutzt
(`core/FBGeodesy.h`s `FBEnuToBodyLos`), und die eine Frage stellen, auf die es ankommt: **bin ich in
dieser Keule?**

**Die drei Signale, und warum sie nicht eines sind** — der Unterschied ist taktisch, nicht kosmetisch:

| `FBEmitterMode` | Keulenfenster | Bedeutung |
|---|---|---|
| `Search` | das GANZE Suchvolumen — die Antenne überstreicht ein Volumen, die Keule kreuzt also einmal je Frame alles darin | **Information**: jemand schaut, niemand hat dich gefunden |
| `Track` | ein schmaler Kegel auf das verfolgte Ziel — Single-Target-Track kollabiert das Muster auf EIN Ziel: alle Leistung, eine Bleistiftkeule, kontinuierlich; nur dieses Ziel hört sie | **Warnung**: er hat dich |
| `Guidance` | dieselbe Keule | ein verfolgendes Radar, das zugleich eine Rakete im Flug unterstützt (der Midcourse-Uplink des Schützen, `core/FBWeaponUplink.h`) — `doc/f16/defence-rwr-cm.md` §1s blinkender Kreis, also das MISSILE-LAUNCH-Licht |
| `None` | — | diese Einheit strahlt gar nicht |

Der eigene Suchkopf einer Rakete ist **kein vierter MODUS, sondern eine andere ART von Emitter**
(`FBEmitterKind`), denn was ihn zur Bedrohung macht, ist, was hinter der Antenne steckt, nicht wie sie
scannt.

```
FBEmitterKind { Unknown = 0, AirborneFireControl, MissileSeeker }
```

`Kind` ist, WAS strahlt, wie der Emitter selbst es weiß. Ein Empfänger SCHÄTZT das immer nur (er hört
eine Wellenform, kein Typenschild) — weshalb `systems/FBRwrSystem` eine eigene geschätzte Kopie führt,
statt dieses Feld durchzureichen.

**`FBEmitterSignature`**: `Mode`, `Kind`, `AzCenterDeg`/`AzHalfDeg`, `ElCenterDeg`/`ElHalfDeg`
(Mitte + Halbbreite des Fensters, körperbezogen), `RangeM`.

**KEINE IDENTITÄT, KEINE LEISTUNGSANGABE, KEINE FREQUENZ** — bewusst dieselbe Askese, die
`core/FBRadarContact.h` auf der anderen Seite des Zauns hält: die Signatur sagt, WAS abgestrahlt wird
und von wo, nie WER es tut. **`RangeM` ist das eigene Erfassungstor des Emitters: die EINE Zahl, die
hier für Sendeleistung einsteht**, denn ein Set, das einen Jäger auf 40 nm erfassen kann, setzt etwa
das Zehnfache der Energie eines auf 10 nm gedeckelten um. **Wie weit das GEHÖRT wird, ist Sache des
Empfängers** (ein Einwegpfad gegen den Zweiwegpfad des Emitters —
`systems/FBRwrSystem::kBeamRangeFactor`), nicht des Emitters.

#### 8.4 `FBRwrThreat`

`core/FBRwrThreat.h`. Ein Emitter, wie ein WARNEMPFÄNGER ihn meldet — der bewusste Gegenentwurf zu
BEIDEN oben. Ein Datalink-Track ist eine Nachricht und trägt ein Callsign; ein Radarkontakt ist ein
Echo und trägt Entfernung; eine RWR-Bedrohung ist keines von beidem — sie ist eine RICHTUNG, aus der
ein Signal ankommt, plus dem, was der Empfänger daraus gemacht hat.

**DIE ZWEI ABWESENHEITEN SIND DAS MODELL:**

1. **KEINE ENTFERNUNG.** Ein RWR misst Peilung und empfangene LEISTUNG; er kann keine Entfernung
   messen, weil er nie etwas gesendet hat, dessen Rücklauf er stoppen könnte.
   `doc/f16/defence-rwr-cm.md` §2.1 ist ausdrücklich: der Abstand des Symbols vom Scope-Mittelpunkt
   ist RELATIVE LETALITÄT, nicht physische Entfernung. Also trägt diese Struktur eine Letalitätszahl
   und keine Meter, **und nichts stromabwärts kann versehentlich eine Entfernungslösung aus einem
   Warnempfänger fliegen.**
2. **KEINE SICHERHEIT ÜBER WEN.** `Kind` ist, was der Empfänger aus dem Signal GESCHÄTZT hat, nicht
   was der Emitter publiziert hat — dieselbe Beziehung, die `FBRadarContact::Iff` zur Wahrheit hat.
   Heute ist die Schätzung perfekt (die Bedrohungsbibliothek ist einen Eintrag tief); das Feld
   existiert, damit an dem Tag, an dem sie es nicht mehr ist, kein Konsument sich ändern muss.

**`FBRwrThreatMode { Search = 0, Track, Missile }`** — der TAKTISCHE INHALT
(`doc/f16/defence-rwr-cm.md` §1s Symboltabelle, eins zu eins): ein schlichtes Symbol für ein suchendes
Set, ein eingerahmtes für ein verfolgendes, ein blinkendes für eines, das eine Rakete auf dich lenkt —
Information, Warnung, Bedrohung. **Die ORDNUNG IST DIE PRIORITÄTSORDNUNG**: ein höheres Ordinal
schlägt auf dem Display ein niedrigeres. Der Suchkopf einer Rakete, der dich anstrahlt, ist der dritte
Fall, wie auch immer er gerade scannt.

| Feld | Bedeutung |
|---|---|
| `Id` | die EIGENE Symbolnummer des Empfängers (1..), in Erfassungsreihenfolge — **nie eine Unit-Id**, exakt die Rolle von `FBRadarContact::TrackNum`, aus exakt demselben Anti-Cheat-Grund |
| `BearingDeg` | RELATIV zur eigenen Nase, −180..180 (+ = rechts): die TWA ist ein Relativpeilungs-Display mit der eigenen Nase oben (§1) |
| `ElDeg` | Elevation, unter der das Signal ankommt, körperbezogen (+ = oben) — auf dem echten, rein azimutalen Scope NICHT angezeigt, aber es ist das, woran die Antennenabdeckungsgrenze entschieden wird, also wird es publiziert statt versteckt |
| `LethalityNorm` | 0..1, die radiale Position auf dem Scope: 1 = Mitte (am tödlichsten) |
| `SignalNorm` | empfangene Leistung, 0..1 dessen, was dieser Empfänger überhaupt hören kann — **der EINE Näherungshinweis, den ein RWR wirklich hat** |
| `AgeS` | seit der letzten Erfassung; > 0 = gehalten, die Emission hörte auf oder die Keule wanderte weg und das Symbol ist noch nicht gefallen |
| `Mode`, `Kind` | s.o.; `Kind` ist GESCHÄTZT |
| `New` | innerhalb des Neue-Bedrohung-Tonfensters |

**`kMaxRwrThreats = 8`** — die OPEN-Anzeige der ALR-56M zeigt 16 und PRIORITY 5
(`doc/f16/defence-rwr-cm.md` §2.1); das sind **DISPLAY-Deckel über der erfassten Menge**, dies hier ist
die Größe der Erfassungstabelle selbst und passt zu `kMaxRadarContacts`/`kMaxDatalinkTracks`.

#### 8.5 `FBCountermeasure` — Programme und Chaff-Wolken

`core/FBCountermeasure.h`.

**DAS PROGRAMM-SCHEMA IST DAS DER AN/ALE-47** (`doc/f16/defence-rwr-cm.md` §2.2, „CMDS CHAFF/FLARE DED
pages"), Feld für Feld und Bereich für Bereich: je Gegenmaßnahmen-TYP eine Salvengröße (Patronen in
einer Salve, 0–99), ein Salvenintervall im Sinne des Patronenabstands (0,020–10,000 s), eine
Salvenzahl (Salven im Programm, 0–99) und ein Salvenabstand (0,50–150,00 s).

```
FBCmProgramType { int BurstQty; double BurstIntervalS; int SalvoQty; double SalvoIntervalS; }
FBCmProgram { FBCmProgramType Chaff, Flare; }
```

| Feld | DED-Name | Bereich |
|---|---|---|
| `BurstQty` | BQ | 0..99 Patronen je Salve (**0 = dieser Typ ist nicht in diesem Programm**) |
| `BurstIntervalS` | BI | 0,020..10,000 s zwischen Patronen |
| `SalvoQty` | SQ | 0..99 Salven (**0 = Typ nicht im Programm**) |
| `SalvoIntervalS` | SI | 0,50..150,00 s zwischen Salven |

**Das Nullen der Salvengröße oder -zahl eines Typs entfernt ihn aus dem Programm** — so wird ein
Nur-Chaff- oder Nur-Flare-Programm ausgedrückt: eine Regel der echten DED-Seite, hier reproduziert
statt durch ein „Typ"-Flag ersetzt. Ein Programm ist damit Missions-/Beladungsdaten, kein Verhalten.
`Present()`, `Valid()` (prüft alle vier Bereiche), `Cartridges() = BurstQty · SalvoQty`.

**`FBCmType { Chaff = 0, Flare }`** — nur die zwei Verbrauchsgüter, die diese Zelle wirklich trägt:
§2.2 hält fest, dass die OTHER1/OTHER2-Stationen auf dem Panel existieren und KEINE Funktion haben.

**`FBCmdsMode { Off = 0, Stby, Man, Semi, Auto, Byp }`** — der Modusknopf (§2.2s
Zustandsmaschinen-Tabelle). Telemetriesichtbare Ordinale: anhängen, nie umsortieren.
`FBCmdsModeStr`/`FBCmdsModeFromString` (letzteres ohne `<cstring>`, per Zeichenvergleich).

**`FBCmdsStatus { NoGo = 0, Go, DispenseReady }`** — die 3-Zustands-Statusanzeige des Panels (§2.2):
bestromt-aber-ausgefallen, bereit, und bereit-und-auf-Zustimmung-wartend (die SEMI-„Counter"-Aufforderung).

**DIE WOLKE IST, WAS DAS PROGRAMM ÜBERHAUPT BEDEUTSAM MACHT.** Eine ausgeworfene Chaff-Patrone blüht
zu einer Wolke resonanter Dipole auf, die binnen etwa einer Sekunde im Wesentlichen die ganze
Geschwindigkeit des Flugzeugs verloren hat und in der Luftmasse hängt. Diese zwei Tatsachen — **ein
großer Radarrückstrahl und KEINE Eigengeschwindigkeit** — sind die gesamte Physik, die ein Radar
sieht, und beide stehen in dieser Struktur: die Position der Wolke ist, wo sie geworfen wurde, und sie
bewegt sich nicht (FlightBox hat kein Windfeld, also ist „stationär in der Luftmasse" = „stationär").

```cpp
struct FBChaffCloud { bool Active; double LatDeg, LonDeg, AltM; double BloomS; };
```

**Die Alterskurve und warum sie ihre Form hat [SET]**: eine Patrone ist beim Ausstoß ein gepacktes
Bündel und erst nach dem Aufblühen ein nützlicher Reflektor; danach wächst und verdünnt sie weiter,
bis sie nicht mehr dicht genug ist, um mit dem Rückstrahl eines Flugzeugs zu konkurrieren. Also:
nichts vor `kChaffBloomS`, volle Stärke beim Aufblühen, dann linearer Abfall auf null bei
`kChaffLifeS`.

| Konstante | Wert | Status |
|---|---|---|
| `kChaffBloomS` | **0,3 s** | **[SET]** — Aufblühen ist schnell (die Patrone ist darauf ausgelegt, im Fahrtwind aufzugehen) |
| `kChaffLifeS` | **8,0 s** | **[SET]** — die nutzbare Lebensdauer ist die Größenordnung zehn Sekunden, bevor die Wolke zu dünn ist, um einen Suchkopf zu halten |
| `kMaxChaffClouds` | **8** | die frischesten acht Patronen werden in der Emissionssignatur der werfenden Einheit publiziert. Acht deckt jedes Programm ab, dessen Salvenabstand innerhalb einer Wolkenlebensdauer bleibt; ältere sind die zerstreuten und sind das Richtige zum Verlieren |

**Die Quellen dokumentieren Ausstoß-PARAMETER, nie Aufblüh- oder Bestandszeiten** — diese beiden
Zahlen sind FlightBox' eigene, und sie sind die zwei Knöpfe, die entscheiden, wie lange eine Salve
schützt; deshalb stehen sie hier als benannte Konstanten und nicht im Radar, das sie liest.

```cpp
inline double FBChaffRcsNorm(double ageS);   // 0..1 relativ zum eigenen Maximum
```

Eine freie Funktion, weil BEIDE Seiten DIESELBE Kurve brauchen: der Werfer, um zu wissen, wann eine
Wolke aufgehört hat zu zählen, und das Radar, um zwei Wolken gegeneinander zu wiegen.

---

### 9. Der Elevation-Hook

`core/FBElevationProvider.h`. Die EINE Naht, durch die jeder Core-Konsument von Bodenhöhe geht —
Missions-Bodenspawn, AGL/Radarhöhe, Absturzerkennung —, damit „wo ist der Boden" eine INJIZIERTE
Abhängigkeit ist und kein hartverdrahteter fb-tiles-Draht.

```cpp
class FBElevationProvider {
  virtual double GroundElevM(double latDeg, double lonDeg) const = 0;
  virtual bool GroundElevPatch(latMin, lonMin, latMax, lonMax, cols, rows, double *out) const;
};
```

**`GroundElevPatch`** ist die Flächenabfrage für künftige terrain-bewusste Guidance: füllt `out`
zeilenweise (`cols`×`rows`, **Zeile 0 = Südrand, Spalte 0 = Westrand**). Die Default-Implementierung
schleift schlicht über `GroundElevM` — korrekt für jeden Provider; eine Implementierung darf sie
überschreiben, sobald ein echter Batch-Pfad (z.B. EIN DEM-Kachel-Decode über den ganzen Patch) den
Code wert ist. Liefert `false` genau dann, wenn `out` null ist oder das Gitter entartet ist
(`cols<2` oder `rows<2`); eine einzelne unaufgelöste Probe schreibt den Sentinel und lässt den Patch
NICHT fehlschlagen.

**Der Sentinel:**

| Symbol | Wert | Bedeutung |
|---|---|---|
| `kFBElevationUnresolved` | **−1e9** | „noch nicht aufgelöst" — passt zur bestehenden Konvention von `fb_stream_ground` (`FBTerrainLoader.h`), damit `FBTilesElevation` ein reiner Pass-through ist |
| `FBElevationResolved(m)` | `m > -1e8` | die EINE „ist diese Probe benutzbar"-Prüfung. Jeder Aufrufer schrieb `sample > -1e8` von Hand — dieselbe magische Schwelle im Runner, im Browser-Loop und im Boot-Pfad; ein benanntes Prädikat hält sie zu einer Regel |

**Alle Implementierungen sind aus Aufrufersicht SYNCHRON**: ein selbst asynchroner Client (WASM) pollt
`GroundElevM`, bis es aufhört, den Sentinel zu liefern — genau wie es die Aufrufer von
`fb_stream_ground` schon tun.

#### Die vier Implementierungen

| Klasse | Ort | Gym-Flag | Verhalten |
|---|---|---|---|
| `FBConstantElevation` | `core/` | — | eine feste Höhe überall |
| `FBRunwayPlateauElevation` | `core/` | `--elev const` | Runway-Plateaus + Smoothstep-Abfall |
| `FBBakedDemElevation` | `core/` | `--elev swiss` | eingebackenes Insel-Raster, bilinear |
| `FBTilesElevation` | **`world/`** (nicht Teil der Core-Lib) | `--elev tiles` | dünner Pass-through auf das Live-fb-tiles-DEM |

##### `FBConstantElevation`

`core/FBConstantElevation.h`. Eine feste Bodenhöhe, beim Konstruieren setzbar (`SetElevM` danach). Der
Gym-Client setzt sie automatisch auf die Schwellen-Elevation der Runway der Mission
(`FBRunway::ThresholdElevM`), sodass eine Bodenmission mit ÜBERHAUPT KEINEN Elevationsdaten läuft
(Prinzip-4-freundlich: deterministisch, kein Netz). Default 0 m = Meereshöhe, wie jeder andere
„noch keine Daten"-Fallback dieses Codebaums.

##### `FBRunwayPlateauElevation`

`core/FBRunwayPlateauElevation.h/.cpp`. Der DEM-freie Provider des Gyms. **Warum nicht einfach eine
Konstante**: eine Mission kann MEHRERE Runways bei UNTERSCHIEDLICHER Höhe haben (heute eine; Phase 3s
`dest_runway` fügt eine zweite hinzu), also ist ein einzelner flacher Wert für Start + Landung im
selben Lauf falsch.

| Zone | Antwort |
|---|---|
| innerhalb des Runway-Fußabdrucks (Länge × Breite) + `kPlateauMarginM` = **5.000 m** | die eigene `ThresholdElevM` dieser Runway |
| bis `kFalloffM` = **10.000 m** darüber hinaus | Smoothstep (`1 − (3t² − 2t³)`) hinunter auf die flache Basis |
| jenseits davon | `BaseElevM` (Default 0 m) |

Der Abfall existiert, damit der Reiseflug noch eine plausible (wenn auch approximative) AGL liest statt
einer harten Kante am Fußabdruckrand.

**Überlappende Plateaus folgen NUR der NÄCHSTEN Runway** — die einfachste stetige Wahl. Ein harter
Wechsel zwischen zwei Plateaus verschiedener Höhe ist nur dort möglich, wo ihre Fußabdrücke einander
nahe genug kommen, um zu überlappen, was echte Flugplätze nie tun: **dokumentieren, nicht für einen
Fall überkonstruieren, der mit den heutigen Ein-Runway-Missionen nicht eintreten kann.**

`FootprintDistM` ist die Abstands-zum-Rechteck-Variante derselben Längs-/Quer-Projektion
(`FBTrackProjectM`), die das Off-Runway-Tor des Missionsmonitors benutzt — mit demselben **60-m**-
Fallback bei `WidthM <= 1`.

##### `FBBakedDemElevation`

`core/FBBakedDemElevation.h/.cpp`. Lädt EINMAL ein kleines eingebackenes Insel-Raster und beantwortet
`GroundElevM` per **bilinearer Interpolation**; **0 m außerhalb der Bbox des Rasters** (der
„Insel"-Vertrag: das Asset deckt nur die Schweiz ab, alles andere liest Meereshöhe). Bei Lade-/
Formatfehler ist `Ok()` false und `GroundElevM` liefert immer 0 — es DEGRADIERT auf den flachen
Meereshöhen-Fallback, statt einen Missionsboot abstürzen zu lassen.

**Die Zahlen des Schweiz-Rasters** (`sim/tools/bake_swiss_dem.py`, kein Build-Target — läuft nur bei
Änderung):

| Parameter | Wert |
|---|---|
| Datei | `sim/assets/swiss-dem-90m.bin` |
| Bounding-Box | **5,96–10,49 °E / 45,82–47,81 °N** |
| Zielauflösung | **90 m** (`TARGET_M`) auf der mittleren Breite der Box |
| Rastergröße | `cols = round(Δlon · 111320 · cos(lat_mid) / 90) + 1`, `rows = round(Δlat · 111320 / 90) + 1` (~3900 × 2450) |
| Dateigröße | **18.888.520 Bytes** (~18,9 MB) |
| Quelle | Terrarium-Kacheln, Zoom **11** (~52–53 m/px hier, feiner als die 90-m-Ausgabe), Bbox + 1 Kachel Rand (~580 Kacheln), bilinear in-process auf das 90-m-Gitter resampelt |
| Randbehandlung | die äußeren **15.000 m** (`EDGE_BLEND_M`) der Box smoothstepen auf 0 m herunter — vermeidet eine harte Klippe am Bbox-Rand, wo echtes Gelände ungleich null ist (z.B. die Alpen am Südrand) |
| Außerhalb der Box | 0 m |

**Das Asset-Layout** (little-endian; **alle Felder an festen Byte-Offsets gelesen, NIE als Cast auf
eine C-Struktur**, damit Padding/Alignment den Leser nie vom Writer desynchronisieren können):

| Offset | Typ | Feld |
|---|---|---|
| 0 | `char[8]` | Magic `"FBDEM01\0"` |
| 8 | `uint32` | `cols` |
| 12 | `uint32` | `rows` |
| 16 | `double` | `lonMin` |
| 24 | `double` | `latMin` |
| 32 | `double` | `lonMax` |
| 40 | `double` | `latMax` |
| 48 | `float` | `scaleM` (int16-Sample × `scaleM` = Meter) |
| 52 | `uint32` | reserviert (0) |
| 56 | `int16[rows·cols]` | zeilenweise, **Zeile 0 = `latMin` (Süd), Spalte 0 = `lonMin` (West)** |

`kHeaderBytes = 56`. Ein Gitter mit `cols < 2` oder `rows < 2` oder eine zu kurze Datei wird verworfen.

**Gym-Default**: `swiss`, wenn das Asset vorhanden ist, sonst `const` — ein blankes
`fb-gym --mission FILE` läuft immer, mit oder ohne Netz.

---

### 10. Geodäsie, Atmosphäre, Einheiten, Mathematik

#### 10.1 `FBGeodesy` — die EINE planare ENU-Geodäsie

`core/FBGeodesy.h`, header-only, keine Übersetzungseinheit.

**Warum diese Datei existiert**: derselbe fünfzeilige Block `dlat*111320, dlon*111320*cos(lat)` stand
an SECHS Stellen (`core/FBMissionMonitor`, `core/FBRunwayPlateauElevation`, `systems/FBPilot`,
`systems/FBNavSystem`, `systems/FBAutopilot`, `app/FBAppWasm`) — **und nur einige davon wickelten die
Längendifferenz in [−180,180]**. Die ungewickelten Kopien lasen über den Antimeridian ein ~360°-Delta,
also **~38.000 km Entfernung zu einem Punkt einen Meter weiter**: auf 180° Länge waren die
Wegpunkterfassung des Missionsmonitors, die Runway-Plateau-Elevation und die Home-Distanz des HUD
schlicht falsch. Das Wickeln ist jetzt Teil des Primitivs, nicht etwas, woran jeder Aufrufer denken
muss.

**KONVENTION**: der Referenzpunkt kommt ZUERST und besitzt den Kosinus. `FBEnuOffsetM(ref, p)` liefert
den Versatz von `p` VON `ref`, mit der Längenskalierung auf der REFERENZ-Breite — eine Regel, damit
eine Peilung und eine Entfernung, die zwei verschiedene Subsysteme rechnen, übereinstimmen. Wer nur
eine Entfernung braucht, darf jeden der beiden Punkte als Referenz übergeben (die Versätze
unterscheiden sich nur im Vorzeichen, der Betrag ist identisch).

**GELTUNGSBEREICH**: bewusst planar/kleinwinklig, passend zu dem, was jede Aufrufstelle ohnehin tat —
Steerpoints, Runway-Achsen und Wegpunkterfassungen liegen zehner Nautische Meilen entfernt, nicht
interkontinental. Echte geodätische Mathematik gehört dem, was sie braucht, nicht den Aufrufern dieser
Datei.

| Funktion | Vertrag |
|---|---|
| `FBGeoToEcef(lat, lon, alt, out[3])` | **WGS84 geodätisch → ECEF (m). Die eine Funktion hier, die NICHT kleinwinklig ist** — die exakte Ellipsoid-Konversion, auf der die kamera-relative ECEF-Welt des Renderers steht (`a = 6378137,0`, `e² = 6,69437999014e-3`). Sie stand zeichenidentisch in beiden App-Einstiegspunkten, bevor sie hierher zog |
| `FBEnuAxesEcef(lat, lon, E, N, U)` | die lokalen ENU-Achsen in ECEF — die Rotation, mit der jede ECEF-Vektor-/Kamerakonversion beginnt (`render/FBCamera.h`s `FBCameraBasisEcef`) |
| `FBWrap180(deg)` | Winkeldifferenz in [−180,180]. Die SCHLEIFEN-Form (nicht `fmod`) ist die, die jede bestehende Aufrufstelle benutzte; sie ist exakt für die Ein-oder-zwei-Umläufe-Deltas, die tatsächlich vorkommen |
| `FBEnuOffsetM(refLat, refLon, lat, lon, eastM, northM)` | planarer Versatz: `north = Δlat · kMPerDeg`, `east = FBWrap180(Δlon) · kMPerDeg · cos(refLat)` |
| `FBPlanarDistM(...)` | horizontale Entfernung (vorzeichenfrei) |
| `FBBearingDeg(ref, p)` | true Peilung 0..360 (`atan2(e, n)`) |
| `FBEnuToBodyLos(roll, pitch, yaw, e, n, u, azDeg, elDeg)` | **Sichtlinie → Körperrahmen**: ENU hinein, körperbezogener Azimut/Elevation heraus (+az = rechts der Nase, +el = über der Boresight-Ebene) — die Standard-NED→Körper-Euler-Sequenz `Rx(roll)·Ry(pitch)·Rz(yaw)` auf den Versatz angewandt, also **WAS DIE ANTENNE SIEHT** statt dessen, was eine Karte zeigte. Der eine Aufrufer der Vorwärtsrichtung ist `systems/FBRadarSystem::RelativeLos`, der eine der Umkehrung ist die BFM-Steuerung von `systems/FBPilot` — **sie MÜSSEN exakt übereinstimmen, sonst steuerte der Pilot auf einen Punkt, den das Radar woanders meldet** |
| `FBBodyLosToEnu(...)` | das exakte Inverse, Einheitslänge |
| `FBBodyVecToEnu(roll, pitch, yaw, fwd, right, down, e, n, u)` | ein KÖRPER-Vektor (+vorwärts/+rechts/+unten, beliebige Einheit) in lokales ENU. **Auf `FBBodyLosToEnu` gebaut statt auf einer zweiten Kopie der Euler-Sequenz** — zwei auseinanderdriftende Schreibweisen derselben Rotation sind genau die Fehlerklasse, gegen die diese Datei existiert. Der eine Aufrufer ist die Store-Abwurf-Geometrie (`app/FBMissionBoot.h`): ein Pylon-Versatz und die Rotationsgeschwindigkeit an diesem Pylon sind beide Körpervektoren, die im Weltrahmen landen müssen |
| `FBEnuToBodyVec(...)` | das exakte Inverse davon, auf `FBEnuToBodyLos` gebaut. Der eine Aufrufer ist die Schadensauflösung (`app/FBMissionRunner.cpp`): eine Detonation passiert an einem Punkt in der Welt, und was entscheidet, welche Systeme sie zerstört hat, ist, wo dieser Punkt entlang der Zellenachse des ZIELS sitzt |
| `FBTrackProjectM(refLat, refLon, courseDeg, lat, lon, alongM, acrossM)` | **Längs-/Quer-Projektion** auf die Linie durch die Referenz auf true Kurs: +längs den Kurs hinunter, +quer nach rechts. Das Runway-Achsen-Primitiv, das das On-Runway-Tor des Missionsmonitors, der Fußabdruck des Plateau-Providers, die Mittellinien-Steuerung von `FBPilot` und der Localizer von `FBAutopilot` alle brauchen — **eine Definition, damit „auf der Linie" für den Piloten, der sie fliegt, und den Monitor, der sie beurteilt, dasselbe bedeutet** |

#### 10.2 `FBAtmosphere` — ISA

`core/FBAtmosphere.h`, header-only. **Für die zwei Konsumenten, die über Luft rechnen müssen, in der
sie gerade nicht fliegen** — alles, was fliegt, hat JSBSims eigene Atmosphäre hinter sich
(`aero/qbar-psf` etc.):

1. die Startbereichs-Integration von `modules/f16/FBF16FireControl`, die den Flug einer Waffe
   vorhersagt, bevor diese Waffe existiert;
2. der Verstärkungsplan von `modules/missile/FBMissileGuidance`, der den auf seine eigene Zelle
   wirkenden Staudruck aus der übergebenen Pose braucht (`fb_fdm_state` trägt kein qbar).

Eine Definition statt zweier privater Kopien derselben vier Konstanten.

| Funktion | Modell |
|---|---|
| `FBIsaDensity(altM)` | Troposphäre bis **11.000 m** mit dem Standard-Temperaturgradienten **6,5 K/km**: `T = 288,15 − 0,0065·h` (auf ≥ 1 K geklemmt), `ρ = 1,225·(T/288,15)^4,2561`. Darüber isotherm: `ρ = 0,36391·exp(−(h−11000)/6341,62)` |
| `FBDynamicPressure(tasMs, altM)` | `q = ½·ρ(h)·v²` [Pa] |

**Kein Wind, kein Wetter, kein Nichtstandardtag**: die Konsumenten oben sind eine gespeicherte
Feuerleittabelle und ein Verstärkungsplan, und keiner würde durch eine Detailtreue besser, die der
Rest des Gefechts nicht hat.

#### 10.3 `FBUnits` — die EINE Definition jedes Umrechnungsfaktors

`core/FBUnits.h`, header-only, `constexpr`.

**Warum diese Datei existiert**: dieselben Zahlen wurden Datei für Datei privat neu deklariert —
`kMPerDeg` sechsmal, `kMsToKt` fünfmal, π sechsmal, `kR2D` viermal — **und eine davon war GEDRIFTET**:
Knoten→m/s stand als `0.51444444444` in `app/FBMissionBoot.h` (der Spawn-IC) und als `0.5144444444`
in `modules/f16/FBF16Module.cpp` (die kommandierte Zielgeschwindigkeit). Die Geschwindigkeit, die eine
Mission DEKLARIERTE, und die, die der Pilot KOMMANDIERTE, wurden also mit verschiedener Präzision
umgerechnet — genau die Fehlerklasse, die sich vervielfacht, sobald mehrere Einheiten gleichzeitig
fliegen, und die kein Leser aus einer Datei heraus sehen kann.

| Konstante | Wert | Status |
|---|---|---|
| `kPi` | 3,14159265358979323846 | |
| `kDeg2Rad` | `kPi/180` | |
| `kRad2Deg` | 57,29577951308232 | |
| `kMPerDeg` | **111320,0** | Meter je Breitengrad, sphärische Näherung. Die planare ENU-Konvention dieses Codebaums (s. `FBGeodesy.h`): gültig für die Zehner-Nautische-Meilen-Skalen, über die FlightBox tatsächlich misst, nicht für interkontinentale Geodäsie |
| `kFtToM` | **0,3048** | **exakt**, per Definition des internationalen Fußes |
| `kMToFt` | `1/kFtToM` | |
| `kNmToM` | **1852,0** | **exakt**, per Definition der Seemeile |
| `kMToNm` | `1/kNmToM` | |
| `kKtToMs` | `kNmToM/3600` | **exakt**: 1 kt = 1 nm/h |
| `kMsToKt` | **1,9438444924406** | **historisches 14-stelliges Literal, bewusst behalten**: es wird von den Telemetriespalten und vom Bodengeschwindigkeits-Tor des `FBMissionMonitor` konsumiert, und jede Stelle war bereits bitgenau einig — es als `3600/1852` neu abzuleiten verschöbe gemessene Zahlen ohne Gewinn |

**Werte sind EXAKTE Definitionen, wo eine existiert** — das Verhältnis zu schreiben statt einer
gekürzten Dezimalzahl ist zugleich genauer und selbstdokumentierend.

#### 10.4 `math/FBMat4` — Renderer-Mathematik

`sim/src/math/FBMat4.h`. Der einzige Inhalt von `math/`. **Spaltenweise (column-major),
OpenGL-Konvention**: Element `m[c*4+r]` ist Spalte c, Zeile r; die Multiplikation mit einem
Spaltenvektor `v` ergibt `m*v` (passend zu `glUniformMatrix4fv(..., GL_FALSE, m)`).

**Warum es aus `world3d.h` herausgelöst wurde**: es ist der eine Teil des Renderers, der KEINEN
GL-Kontext braucht — reine Float-Mathematik, also direkt assertierbar, statt an Pixeln beurteilt zu
werden. **Ein falsches Vorzeichen hier stürzt nicht ab**; es spiegelt still die Welt oder stülpt die
Kamera um — genau die Fehlerklasse, die ein Unit-Test fängt und ein Augenpaar nicht.

Stilhinweis: die Datei folgt NICHT der `FB`-Klassenkonvention des restlichen Baums — sie exponiert
freie `static`-Funktionen im C-Stil (`m_identity`, `m_mul`, `m_persp`, `m_lookat`, `v_norm`,
`v_cross`), ohne `namespace FlightBox`.

| Funktion | Vertrag |
|---|---|
| `m_identity(m)` | Einheitsmatrix |
| `m_mul(o, a, b)` | `o = a·b` (über einen Zwischenpuffer, also aliasing-sicher) |
| `m_persp(m, fovy, asp, zn, zf)` | **REVERSED-Z-Perspektive**: nah bildet auf NDC z=+1 (Fenstertiefe 1,0) ab, fern auf −1 (0,0) — der Standard-`zn`↔`zf`-Tausch in der z-Zeile. Zusammen mit `glClearDepthf(0)` + `GL_GEQUAL` und dem 32-Bit-Float-Tiefenpuffer hebt die 1/z-Kurve der Projektion die Verteilung der Float-Mantisse auf und liefert **nahezu gleichförmige Präzision über 0,01 m…240 km**, wo einfache Tiefe fernes Gelände ins Flimmern z-fightet. x/y (`m[0]`, `m[5]`) und w (`m[11]`) sind unverändert, also bleiben Bildschirmprojektion, die manuelle Projektion des HUD und die Frustum-Extraktion unberührt |
| `m_lookat(m, eye, ctr, up)` | Welt → Sicht: Kamera bei `eye`, blickt auf `ctr`, mit ungefähr `up` als Oben |
| `v_norm(v)` | normiert (No-Op unter Länge 1e-6) |
| `v_cross(o, a, b)` | Kreuzprodukt |
