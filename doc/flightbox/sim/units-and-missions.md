# Welt-Entitäten und Missions-Kern — `units/`, `app/FBMission*`, `modules/FBModule*`

**Quellen dieser Datei:** die Kommentar-Banner von `sim/src/units/` (`FBUnit.h`, `FBSimUnit.h/.cpp`,
`FBUnitRegistry.h`), `sim/src/app/FBMissionRunner.h/.cpp`, `sim/src/app/FBMissionBoot.h`,
`sim/src/app/FBTickPool.h/.cpp`, `sim/src/app/FBModelRoots.h`, `sim/src/app/FBLogSinks.h`,
`sim/src/modules/FBModule.h`, `sim/src/modules/FBModuleRegistry.h/.cpp`, plus CLAUDE.md für die
Etappen-Historie und die Messzahlen. Das Missionsdateiformat selbst steht in
[`doc/mission-format.md`](../mission-format.md) und wird hier **nicht** wiederholt — nur referenziert.

Gegenstand: was eine simulierte Einheit IST, wer sie besitzt, wer sie sehen darf, und die vier Schritte,
mit denen der Orchestrator aus einer Textdatei einen Lauf mit Urteil macht.

---

## 1. Dateien

| Datei | Rolle |
|---|---|
| `units/FBUnit.h` | Basisschnittstelle jeder Welt-Entität: Identität, Pose, Emissions-Signatur, `Run`. Plus `FBUnitKind`, `FBUnitPose`, `FBUnitSignature`. |
| `units/FBSimUnit.h/.cpp` | EINE simulierte Einheit als EIN Objekt (Zelle + Modul + Zustand + Telemetrie + Gesundheitsregister + beide Richter). Plus `FBActorList`. |
| `units/FBUnitRegistry.h` | „Wer existiert" — Liste geborgter `const FBUnit*`, in Deklarationsreihenfolge. |
| `app/FBMissionRunner.h/.cpp` | Der Orchestrator (`FBRunMission`) + `FBMissionTickHook` + `FBMissionResult`. |
| `app/FBMissionBoot.h` | `FBMissionSpawnActor` (Akteur aus Missionsblock) und `FBMissionSpawnStore` (Akteur aus Trägerzustand). Header-only. |
| `app/FBTickPool.h/.cpp` | Der GYM-ONLY Lockstep-Worker-Pool der STEP-Phase. |
| `app/FBModelRoots.h` | Die zwei JSBSim-Modellwurzeln eines Clients. |
| `modules/FBModule.h` | Die Modul-Schnittstelle: Wiring, generische System-Accessoren, `ApplySetup`. |
| `modules/FBModuleRegistry.h/.cpp` | Name → Factory. |

---

## 2. `FBUnit` — die Basisschnittstelle

Jede Welt-Entität, steuerbar oder nicht. Pilot, Sensoren und Waffen fragen Einheiten **durch diese
Schnittstelle** ab, nie über den konkreten Typ — heute Ownship + KI-Jets + Stores + Bodenziele, morgen
mehr, gleiche Form.

**Identität** (`Id`, `Name`, `Kind`, `Team`) wird EINMAL bei der Konstruktion gesetzt und ist unveränderlich.
`Name` ist das Callsign, also das `unit <id>`-Token der `.fbm`.

**Pose** wird bei jeder Abfrage frisch gelesen: eine Unit ist eine SICHT auf die Wahrheit ihres Besitzers,
nie eine duplizierte Kopie, die auseinanderdriften kann.

| `FBUnitPose`-Feld | Einheit |
|---|---|
| `LatDeg`, `LonDeg`, `ElevM` | geodätisch, m ASL |
| `RollDeg`, `PitchDeg`, `YawDeg` | deg |
| `SpeedMs` | TAS/Groundspeed, wie der Unit-Typ es definiert |
| `HeadingDeg` | Bodenkurs, rechtweisend, 0..360 |

### `FBUnitKind` — und was die Unterscheidung genau bewirkt

| Kind | Was gleich bleibt | Was UNTERSCHIEDLICH ist (und warum es dem BESITZER gehört) |
|---|---|---|
| `Aircraft` | — | der Normalfall |
| `Weapon` | eigene FDM-Instanz, eigenes Modul, dieselben Monitore, eigene Telemetriedatei | (1) sein physikalisches K.O. ist eine **Detonation**, beendet den Lauf also nicht (`FirstFlightKo` überspringt es, `ActorResultStr` nennt es `IMPACT`); (2) Luft-Luft-Sensoren suchen keine Waffen |
| `Ground` | Roster, Gesundheitsregister, Schadensmodell, eigene Telemetriedatei — volle Einheit | (1) **keine Flugdynamik**, wird also dem Physik-Monitor nie gezeigt (es fliegt nicht, es gibt nichts zu schließen); (2) Luft-Luft-Sensoren suchen es nicht |

Genau an `Ground` hängt die EINE Ausnahme in `FBSimUnit`: das Airframe ist optional (§3).

### `FBUnitSignature` — was fremde Sensoren wahrnehmen dürfen

Der Teil des Systemzustands, den ein anderer Sensor **legitim** bemerken darf. Publiziert an derselben
Barriere wie die Pose (§4) — kein Empfänger liest je einen halben Tick.

| Feld | Was es ist | Wer es liest |
|---|---|---|
| `DatalinkXmt` | MIDS-Terminal bestromt UND sendend (XMT ON) | `systems/FBDatalinkSystem` der anderen |
| `Uplink` (`FBWeaponUplink`) | die Mittelphasen-Lenkfunk-Aussendung an eine selbst gestartete Waffe | `modules/missile/FBMissileUplink` |
| `IffXpdr` | AN/APX-113 antwortet auf Mode-4 | der Interrogator im `FBRadarSystem` |
| `Radar` (`FBEmitterSignature`) | **die Keule**: Modus, Emitter-Art, körperfestes Fenster, Entfernungstor als Leistungsmaß — inkl. WOHIN sie zeigt | `systems/FBRwrSystem` |
| `Chaff[kMaxChaffClouds]` | die geworfenen Wolken | das gegnerische Radar (Doppler-Notch) |

Zwei ausdrückliche Entwurfsentscheidungen im Header:

- Chaff hängt an der **werfenden Einheit** statt als eigene Unit. Ausgesprochene Konsequenz: eine Wolke
  kann nur ein Radar täuschen, das auf das Flugzeug schaut, das sie geworfen hat — nie eines, das
  jemand anderen in der Nähe verfolgt.
- Eine Wolke ist keine Aussendung, sondern eine REFLEXION — publiziert wird sie trotzdem hier, weil es
  dieselbe Art Tatsache ist: was fremde Sensoren an dieser Einheit wahrnehmen dürfen.

`Run(dt, units, world)` ist der Default-NoOp; `units` ist die Besetzung, wie simulierte SENSOREN sie
sehen dürfen (jeder Eintrag ein Snapshot des letzten abgeschlossenen Ticks, **inklusive dieser Einheit
selbst**), `world` die Terrain-Seite daneben. Beide geborgt, beide dürfen in einem Client ohne sie
`nullptr` sein.

---

## 3. `FBSimUnit` — eine simulierte Einheit, ganz

Alles, was vorher verstreute Locals im Missions-Runner und ein Satz file-scope Statics im Browser-Client
war, ist hier EIN Objekt mit EINEM Besitzer. Es IST ein `FBUnit`.

### Was sie besitzt — und die Deklarationsreihenfolge

```
std::unique_ptr<FBFdm>    Fdm_;        // owned
std::unique_ptr<FBModule> Module_;     // owned
fb_fdm_state              St_;
FBUnitPose                Pose_;       // publiziert
FBUnitSignature           Sig_;        // publiziert
std::string               LogLabel_;
double                    GroundAslM_;
FBSystemHealth            Health_;
FBFdmTelemetrySource      FdmSrc_;     // borgt Fdm_/St_/GroundAslM_
FBStateBusTelemetry       BusSrc_;     // borgt den Bus des Moduls
FBSystemHealthTelemetry   HealthSrc_;  // borgt Health_
FBTelemetryBus            Bus_;
FBFlightMonitor           Flight_;
std::unique_ptr<FBMissionMonitor> Mission_;   // fehlt, wenn die Einheit keine Ziele hat
```

**Warum diese Reihenfolge:** `Fdm_` steht VOR `Module_`, damit die Zelle das Modul überlebt, das sie nur
borgt (Zerstörung läuft rückwärts). Ebenso stehen `St_`, `GroundAslM_` und `Health_` VOR den
Telemetriequellen, die Referenzen darauf halten.

Alles, was die Einheit herausgibt, ist geborgt (`const&`/`*`). Der Telemetrie-**Sink** bleibt beim
Client: File-I/O gehört `app/`, `core/` bleibt I/O-frei.

### Das optionale Airframe

Das EINE, was an einer Einheit nicht universell ist. Universell sind: Identität, Fraktion, publizierte
Pose, Gesundheitsregister, beide Richter, eine Telemetrie-Trace — ob sie nun jemand fliegt oder nicht.

Ein statisches Bodenziel (`modules/ground/FBGroundModule`) hat keine Flugdynamik und daher keine `FBFdm`.
Die Alternative wäre gewesen, einem Bunker ein erfundenes JSBSim-Modell zu geben und es bei 100 Hz zu
integrieren, um die Position zu reproduzieren, an der er gespawnt wurde. Also darf der Zeiger null sein —
in genau den Stellen, an denen das Verhalten wirklich vom Airframe abhängt:

| Stelle | Verhalten ohne Zelle |
|---|---|
| `Run`/`PrimeState` (FDM-Step) | kein Step; die Spawn-Pose IST die ganze Wahrheit |
| `UpdateGroundAsl` (Push nach JSBSim) | entfällt |
| `RunMonitors` (Physik-Richter) | die Einheit wird ihm **nie gezeigt** |
| `BuildMissionSample` (`AnyWow`) | `true` — eine Einheit ohne Airframe ist per Definition am Boden |
| `ApplyDamageToAirframe` | entfällt; „zerstört" heißt für sie: im Register zerstört und nirgends sonst |
| `FBFdmTelemetrySource` (`fuelLbs`, `gearLoadFactor`) | 0, Spaltenset unverändert |

### Anti-Cheat, ungeschwächt durch das Bündeln

Ein `FBSimUnit` lässt sich nur aus einer **bereits gespawnten** `FBFdm` bauen, und die gibt es nur über
`fdm/FBFdmBoot` (app/-only). Also kann nichts unter `systems/` oder `modules/` eine Einheit bauen, einen
Richter erreichen oder ein Airframe neu platzieren. Beleg:
`grep -rn 'FBSimUnit\|FBFlightMonitor\|FBMissionMonitor' src/systems src/modules` ist leer und bleibt es.

Das Modul sieht die Richter nie: sie werden **hier** gefüttert, aus beobachteter FDM-Wahrheit, und das
Einzige, was ein Auslösen an der Zelle tut, ist der Triebwerks-Cutoff, den die App immer angewandt hat.

### `TakeBurst` / `TakeKineticBurst`

Die ganze Konsequenzkette in einem Aufruf, in dieser Reihenfolge und nirgends sonst:

1. `core/FBDamageModel::Apply` (bzw. `ApplyKinetic`) entscheidet, was die Geometrie welchem System antat
   — das Modul liefert dazu ausschließlich `DamageLayout()`, also WO seine Systeme sitzen;
2. `ApplyDamageToAirframe()` schiebt das Ergebnis direkt in die Zelle.

| Register-Zustand | Physik (über `fdm/FBFdm`) |
|---|---|
| Engine `Failed` | `Controls().EngineCutoff()` (durch denselben Steuerpfad, den ein Pilot benutzt) + `SetThrottleLimit(0)` |
| Engine `Degraded` | `SetThrottleLimit(kThrottleLimitDegraded = 0.6)` |
| FlightControls `Failed`/`Degraded` | `SetControlAuthority(0.0 / 0.5)` |
| Structure `Failed`/`Degraded` | `SetDamageDrag(6.0 / 1.5 ft²)` |

Idempotent und nur aufgerufen, wenn sich das Register geändert hat — keine Pro-Frame-Arbeit. **Nichts
wird „tot" markiert:** die Einheit wird weiter getaktet und von denselben zwei Monitoren weiter beurteilt.
Ab dem nächsten Schritt integriert JSBSim das Flugzeug, das sie jetzt ist.

Das Register (`core/FBSystemHealth`) gehört der Einheit aus demselben Grund wie die zwei Richter: es ist
eine Tatsache ÜBER die Einheit, die ihr eigenes Modul lesen, aber nie schreiben darf. Das Modul bekommt
bei der Konstruktion ein `const&` (`FBModule::AttachHealth`).

### `Retire()` — das Ende einer Einheit

```cpp
void Retire() { Active_ = false; Sig_ = FBUnitSignature{}; }
```

- **Kein Löschen aus der Akteursliste.** Das würde jeden späteren Index verschieben und die Tick-Ordnung
  eines Laufs davon abhängig machen, wann zufällig eine Bombe einschlug. Das Objekt lebt weiter, weil die
  Registry rohe Zeiger borgt und alles bereits Geschriebene (Telemetriedatei, Zeilen in `events.log`)
  gültig bleiben muss. Die Flugbahn endet, der Datensatz nicht.
- **Retirieren VERSTUMMT die Einheit** im selben Akt. Der Sucher einer detonierten Runde strahlte im
  letzten Snapshot noch — und weil dieser Snapshot ab dann eingefroren ist, hätte alles mit einem
  Warnempfänger eine Rakete weitergehört, die nicht mehr existiert (**gemessen: der eigene RWR des
  Schützen meldete einen lebenden Sucher zwei Minuten nach der Detonation**). Die Pose bleibt — dort ist
  die Einheit gelandet.

### `CheckEnvelope()` — generische Hüllkurven-Diagnostik

Latched je Einheit, nicht je Lauf. Reine `FBLog::Warn`-Ausgabe, kein Urteil.

| Warnung | Auslöser | Rücksetzung |
|---|---|---|
| `stall` | `cas > 15 m/s` UND `alphaDeg > 25` | `cas <= 15` oder `alphaDeg < 20` |
| `overspeed` | `mach > 1.2` | `mach < 1.1` |
| `sink` | `AGL < 150 m` UND `vy < −15 m/s` | `vy > −5` |

Das `cas`-Gate existiert, weil AoA bei nahezu null Fluggeschwindigkeit numerisch undefiniert ist — sonst
gäbe es eine Einschwing-Warnung beim Spawn.

### Telemetrie-Registrierung — die Anhäng-Regel

`StartTelemetry(sink)` registriert die Quellen in **fester Spaltenreihenfolge**. Die Regel im Code, an
sechs Stellen wiederholt: **eine neue Quelle hängt Spalten HINTEN an, sie verschiebt nie alte.** Jede je
gemessene Spalte behält ihre Position; Regressions-Baselines und Analyse-Skripte lesen nach Position.

Reihenfolge (aus `FBSimUnit.cpp`): `fdm` → AirData → Pilot → FlightControl → Controls → Datalink → Radar
→ `PilotSystem().BfmTrack()` → WarningSystem → Commands → `FBStateBusTelemetry` → Stores → Rwr →
Countermeasures → `PilotSystem().Engagement()` → HealthSrc (`dmg_*`) → Guns.

Ein `nullptr`-Sink lässt den Bus ein billiges No-Op (der Browser-Fall).

### Sonstige Verträge

| Methode | Vertrag |
|---|---|
| `UpdateGroundAsl(sampleM)` | Unaufgelöste Probe behält den letzten guten Wert (`FBElevationResolved`). **EINE Zahl** erreicht sowohl JSBSims Kontaktboden als auch den HUD-/Radarhöhen-Pfad des Moduls — die beiden können sich über den Boden nicht uneinig werden. Für `FBUnitKind::Weapon` wird JSBSim stattdessen `kWeaponNoGroundElevM = −100000 m` gegeben (§8). |
| `HudState()` | Der Bus des Moduls mit der Pose DIESES Frames eingefaltet — das Modul publiziert den Platform-Block in seinem eigenen Takt, der Client re-publiziert ihn zur Framerate, damit die konforme Symbologie gegen die tatsächlich gerenderte Pose gezeichnet wird. Gleicher Block, gleiche Schreiberrolle — keine zweite Kopie der Wahrheit. |
| `PrimeState()` | Boot-only: ein FDM-Step + `PublishPose`, damit das erste Frame keine leere Pose liest. |
| `SetLogAttribution(bool)` | Setzt das `unit=`-Label — einmal beim Boot, nie pro Tick. Leer bei genau einem Akteur. |
| `Displays() const` | Nur-Lese-Sicht auf den Displays-Slot für den Renderer, damit kein Aufrufer `const_cast`en muss (`FBModule`s Accessoren sind absichtlich nicht-const: Systeme werden über sie KOMMANDIERT). |

---

## 4. `FBUnitRegistry` und die Snapshot-Disziplin

### Warum die Registry in der Core-Lib liegt

Sie war ein Member-Vektor von `world/FBWorld` — also auf der **Renderer**-Seite des Lib/Client-Splits.
`fb-gym` linkt `world/` gar nicht und reichte deshalb jedem Modul einen Null-World: ein simulierter
Sensor hätte im einzigen Client, der die Missionsschleife wirklich fährt, **nie** eine andere Einheit
sehen können. Die Besetzung ist Simulationszustand (sie existiert mit oder ohne Kamera), also steht sie in
`units/`, der Client besitzt genau EINE, und `FBWorld` **borgt** sie nur noch (`SetUnits`/`Units()`) für
die Zeichenseite.

### Vertrag

| Eigenschaft | Regel |
|---|---|
| Ownership | geborgt, nie besessen — der Client besitzt die `FBActorList` |
| Reihenfolge | Registrierungsreihenfolge = Missions-Deklarationsreihenfolge, ändert sich während eines Laufs nie (Wachstum nur am Ende, §8) |
| Element-Typ | `const FBUnit *` — read-only **by construction**: ein System kann Identität und die PUBLIZIERTE Pose/Signatur des letzten abgeschlossenen Ticks beobachten, sonst nichts. Es kann eine andere Einheit nicht takten, steuern oder schreiben. |
| Wer sie halten darf | nur ein simulierter SENSOR, und deshalb reist sie als `Run()`-ARGUMENT den System-Zyklus des Moduls hinunter, statt Member zu sein, den jeder erreichen kann. Heute vier Dateien in `systems/`+`modules/`: `FBDatalinkSystem.cpp`, `FBRadarSystem.cpp`, `FBRwrSystem.cpp`, `modules/missile/FBMissileUplink.cpp`. |

### Die Snapshot-Disziplin

**`PublishPose()` ist die Barriere.** Der Client taktet ERST jede Einheit und macht DANN in einer zweiten
Schleife die neuen Posen + Signaturen gemeinsam sichtbar.

```
für jede Einheit:  UpdateGroundAsl            (sequenziell)
für jede Einheit:  Run(dt, &registry, world)  (STEP-Phase, ggf. parallel)
für jede Einheit:  PublishPose()              (die Barriere)
… danach erst: Monitore, Telemetrie, Hook
```

Daraus folgt der Vertrag von `GetPose()`/`GetSignature()`: **immer der Stand des zuletzt ABGESCHLOSSENEN
Ticks**, nie ein halb integrierter. Keine Einheit kann einen Nachbarn sehen, der in diesem Tick bereits
gestept hat — also **kann die Tick-Reihenfolge kein Ergebnis beeinflussen**. Das ist genau der Grund,
warum die Parallelisierung der STEP-Phase (§9) eine reine Parallelisierung ist und kein Redesign.

Der Konstruktor von `FBSimUnit` ruft `PublishPose()` sofort: der deklarative Spawn ist bereits eine
gültige Pose, also liest niemand je eine leere.

Beide Renderer-Clients folgen derselben Disziplin: `app/FBAppWasm.cpp` hält seine eigene `FBActorList`,
ruft `PublishPose` für alle, dann `RunMonitors` — dieselben zwei Richter, keine zweite Parallelprüfung.

---

## 5. Der Missions-Runner als reiner Orchestrator

`FBRunMission(missionPath, timeoutOverride, outDir, models, elevation, hook, threads)` —
geteilt von `fb-gym` und `gpu_native --mission`. **Genau vier Schritte**, keine Missions-Spezifika im
Code.

| Schritt | Was passiert |
|---|---|
| **1 — Mission laden** | Datei einlesen, `FBParseMissionFile` (reine Text→`FBMission`-Funktion), Timeout auflösen (`timeoutOverride > 0` schlägt den Dateiwert), `MISSION_START` loggen. |
| **2 — Welt mit ihren Akteuren aufsetzen** | Je `unit`-Block: Elevation am Spawnpunkt auflösen, konsistenzprüfen, `FBMissionSpawnActor` rufen, in die `FBActorList` hängen. Danach: Kapazitäten reservieren, `FBUnitRegistry` füllen, Telemetriedatei je Akteur öffnen, `hook->OnMissionStart`. |
| **3 — Akteure ausführen** | Die Tick-Schleife bei `dt = 0.1 s` (10 Hz Entscheidungstakt) — Elevation, STEP, Barriere, Monitore, Telemetrie, Geschosse/Stores, Hook, Wachstum, Pose-Merken. |
| **4 — Welt validieren** | Die Monitore haben längst entschieden; hier werden N Urteile zu EINEM Exit-Code kombiniert und `UNIT_RESULT`/`RESULT`/`SUMMARY` emittiert. |

### Was er NICHT weiß

- **Keinen konkreten Modultyp.** `FBMissionRunner.cpp`/`FBAppGym.cpp` inkludieren nie einen konkreten
  Modul-Header; alles läuft über `FBModule`s generische Accessoren.
- **Keine Missionssemantik.** Wegpunkt-Sequenzierung ist Akteurs-Verhalten (`FBNavSystem::AdvanceWaypoint`,
  vom Modul selbst gerufen), nicht Runner-Buchhaltung. Das Urteil fällen `FBFlightMonitor`/
  `FBMissionMonitor`, nicht dieser File.
- **Keine `set`-Schlüssel.** Der Runner reicht die rohe KV-Liste durch; das MODUL interpretiert seine
  eigenen Schlüssel.
- **Keinen Renderer.** Ground Truth kommt aus einem injizierten `FBElevationProvider`, nicht aus einem
  fest verdrahteten fb-tiles-Wire — deshalb hat diese Datei keine Renderer-/World-/Dawn-Abhängigkeit und
  ist Teil der Core-Lib, gegen die `fb-gym` linkt. Wer MEHR will als headless Telemetrie, liefert einen
  `FBMissionTickHook`, dessen Interface bewusst GPU-typ-frei ist; `FBAppNative.cpp` implementiert den
  konkreten Hook mit `FBRenderer`/`FBWorld` in SEINER eigenen Übersetzungseinheit.

### Modul-Auflösung: `FBModuleRegistry`

- `FBRegisterBuiltinModules()` ruft vier familienweise Einstiegspunkte: `FBRegisterF16Module`,
  `FBRegisterStoreModules`, `FBRegisterMissileModules`, `FBRegisterGroundModules`. Jede ist in der
  EIGENEN Familie definiert — die einzigen Dateien, die einen konkreten Modultyp nennen dürfen.
- Die Map ist ein **function-local static** (Meyers Singleton), **kein** namespace-scope Global:
  explizit an einem bekannten Punkt gefüllt statt über Static-Initialization-Order zwischen
  Übersetzungseinheiten — das umgeht sowohl das SIOF-Problem als auch die Falle „eine nicht
  referenzierte `.o` in einem statischen Archiv wird nie gelinkt", die eine Selbstregistrierung hier
  träfe.
- Idempotent (Neuregistrierung überschreibt den Eintrag), also gefahrlos einmal pro Lauf aufrufbar.
- `Create(name)` → `std::unique_ptr<FBModule>` oder `nullptr`. Der Registry-Name ist ein **reiner
  Schlüssel**: WELCHES JSBSim-Modell dazu gehört, sagt das Modul (`FdmModelName`); wo es liegt, ist seit
  der einen Modellwurzel keine Frage mehr.

### `FBModule` — was der Orchestrator über die Basis erreicht

| Kategorie | Methoden |
|---|---|
| Wiring (einmalig, vor dem ersten `Run`) | `AttachFdm(FBFdm&)`, `SetUnitIdentity(id, team)`, `AttachHealth(const FBSystemHealth&)` |
| Selbstauskunft | `FdmModelName()`, `UnitKind()`, `DamageLayout()` |
| Takt | `Run(fb_fdm_state&, dt, const FBUnitRegistry*, const FBWorld*)` |
| System-Slots | `Autopilot`, `FlightControl`, `PilotSystem`, `Controls`, `Displays`, `AirDataSystem`, `NavSystem`, `WarningSystem`, `RadarAltimeter`, `Commands`, `Datalink`, `Radar`, `Rwr`, `Countermeasures`, `Stores`, `Guns`, `Telemetry` |
| Diagnostik | `LastGuidance()`, `LastSubsteps()` |
| Missionsdaten | `FlightPlan()`, `SetRunway()`, `SetGroundAsl()`, `ProgramRelease()`, `ApplySetup(key,value)` |

**`ApplySetup` ist die Modul-eigene Interpretation.** Der Runner/Boot parst nur die flache KV-Liste; das
Modul kennt seine Schlüssel. Rückgabe `false` = **unbekannter Schlüssel** (oder unparsbarer/außerhalb des
Bandes liegender Wert), was der Aufrufer in ein Missions-**FAIL** (Exit 1) verwandelt — nie ein stiller
No-Op. Das Modul hat den Grund bereits geloggt (nur es kennt seine Schlüssel); der Boot loggt zusätzlich
`SET_REJECTED` und macht den Spawn nichtig.

`UnitKind()` ist Aircraft per Default; `Ground` erklärt zugleich, dass es **gar kein Airframe** gibt —
was die leere `FdmModelName()` an der einen Stelle sagt, die der Spawn-Pfad liest. **`Weapon` wird hier
NICHT gesetzt:** ein Store ist ein Kind, weil er FREIGEGEBEN wurde, und das ist eine Aussage des
Freigabepfads (`app/FBMissionBoot.h`), nicht des Moduls.

### `UNIT_RESULT` und die `unit=`-Attribution

**Attributions-Regel (entschieden an genau einer Stelle, `FBMissionBoot.h`):** bei genau einem Akteur
bleiben die Zeilen unattribuiert — sie sind die der Mission. Ab zwei Akteuren trägt JEDE akteursbezogene
Zeile (auch die modulinternen) `unit=<callsign>` als erstes Feld. Ein abgeworfener Store ist immer
attribuiert (er fliegt nie allein — mindestens der Jet, der ihn abwarf, ist da).

**`UNIT_RESULT` wird nur bei > 1 Akteur emittiert** — bei einem einzigen IST die `RESULT`-Zeile dessen
Urteil, und eine Aufschlüsselung würde es nur wiederholen (dieselbe Regel wie bei der Attribution).
Felder: `result`, `reason`, `team`, `decisive`, `lat`/`lon`/`altM`, `telemetry` (Pfad der Trace).

`result` je Akteur (`ActorResultStr`):

| Kind / Lage | Ergebnis |
|---|---|
| `Weapon` | `IMPACT` wenn der Physik-Richter ausgelöst hat, sonst `IN_FLIGHT` |
| `Ground` | `INTACT` / `DESTROYED` (das Einzige, was ihm je passiert) |
| Aircraft, Physik-K.O. ohne vorherigen Abschuss | `LOC` oder `CRASH` |
| Aircraft mit Missions-Monitor | dessen Verdikt |
| Aircraft ohne Ziele | `NONE` |

**`ShotDownFirst`:** ein Jet, der abgeschossen wurde und danach ins Gelände flog, hat ZWEI wahre Urteile,
und das nützliche ist das erste — der Abschuss erklärt den Absturz, der Absturz erklärt nichts. Also
tritt der physische Richter zurück, wenn der Missions-Richter dieser Einheit bereits geschlossen hat UND
sie kampfunfähig ist (die einzige Konstellation, in der dieses Paar auftritt). Ein unbeschädigtes Wrack
(CFIT, Departure) meldet sich unverändert als solches.

### Wie aus N Urteilen eines wird

Die Schleifenbedingung ist die Kombination selbst:

```
while (!FirstFlightKo && !FirstDecidingFailure && !AllJudgedConcluded && simT < timeoutS)
```

| Helfer | Regel |
|---|---|
| `FirstFlightKo` | Ein physisches K.O. IRGENDEINES **Aircraft**-Akteurs beendet den Lauf — die konservative Lesart: kein Wrack integriert im Hintergrund weiter. `Weapon`/`Ground` werden übersprungen. Gibt die EINHEIT zurück, nicht `bool`, weil WESSEN K.O. es war die `RESULT`-Zeile entscheidet. |
| `ExpectedLoss` | Der Verlust eines Akteurs ist **ERWARTET**, wenn er das erklärte Ziel eines anderen war: (a) diese Einheit ist kampfunfähig UND (b) irgendein anderer Akteur mit Monitor deklariert ein `objective`, das sie abdeckt (`FBObjectiveCovers(o, name, team)`). Zwei beobachtete Tatsachen und eine Deklaration — **keine Team-Heuristik, kein Begriff „Spielerseite"**, und für eine Mission ohne `objective`-Zeile passiert gar nichts (alte Missionen kombinieren exakt wie zuvor). |
| `FirstDecidingFailure` | Der erste beurteilte Akteur, dessen Monitor geschlossen und NICHT `Success` ist und dessen Verlust nicht erwartet war. |
| `AllJudgedConcluded` | Alle beurteilten Akteure haben eine Antwort, welche auch immer. Ohne Ziele: derselbe Moment wie „alle erfolgreich", weil dort jedes Scheitern entscheidend ist und die Schleife vorher stoppt. |
| `FirstJudged` | Wessen Verdikt die kombinierte `RESULT`-Zeile zitiert, wenn nichts den Lauf entschied: der erste beurteilte Akteur, dessen Verlust NICHT erwartet war (den Verlierer eines entschiedenen Duells zu zitieren wäre genau die Teamblindheit, die `objective` beseitigt). Verloren ALLE beurteilten so (gegenseitiger Abschuss), spricht doch der erste — niemand kam heim, und der Datensatz soll das sagen statt einen Sieger zu erfinden. |

**Warum es die Regel gibt:** ein Duell hat einen Sieger und einen Verlierer, nicht zweimal FAIL. Vor den
Zielen war das FAIL des Verlierers das einzige Verdikt im Lauf und wurde dessen — eine Mission, deren
FEINDLICHE Einheit abgeschossen wurde, meldete FAIL, und der Erfolg des Schützen war unsichtbar. Ein
erwarteter Verlust wird weiterhin als das eigene FAIL dieses Akteurs in seiner `UNIT_RESULT`-Zeile
gemeldet; was er nicht mehr tut, ist den Lauf entscheiden.

**Nach der Schleife (Schritt 4):** ist das K.O. ein erwarteter Verlust, wird es aus der Wertung genommen
(`ko = nullptr`) und jeder Monitor bekommt `FinalizeMission` — ein `survive`-Ziel kann nur hier
beantwortet werden, weil der zu überlebende Lauf jetzt vorbei ist.

| Priorität der kombinierten `RESULT` | Quelle |
|---|---|
| 1. `ko` (unerwartetes K.O.) | `FBFlightMonitor::Reason()` → `LOC` oder `CRASH`, Detail aus dem Richter |
| 2. `failed` (erste entscheidende Missions-Niederlage) | dessen `FBMissionMonitor` |
| 3. `judged` (sonst) | dessen `FBMissionMonitor` |
| 4. niemand trug Ziele | `TIMEOUT`, „sim time exceeded the mission timeout" |

`deciding` (K.O. oder erste entscheidende Niederlage) liefert das `unit=`-Label der `RESULT`-Zeile und
das `decisive=`-Feld der `UNIT_RESULT`s. Bei sauberem Erfolg entschied niemand allein, also bleibt es
leer.

**Exit-Codes:** `SUCCESS 0`, `FAIL 1`, `CRASH 2`, `LOC 2`, `TIMEOUT 3`. `LOC` teilt sich Code 2 mit
`CRASH` — beide sind `FBFlightMonitor`-Terminierungen, unterschieden über das `result`-Feld der
`RESULT`-Zeile, nicht über den Exit-Code; ein Aufrufer, der nur auf `exit != 0` verzweigt (der
dokumentierte Vertrag), sieht keinen Unterschied.

`SUMMARY` misst `wallS`/`speedup` über `steady_clock`, **nicht** `clock()`: letzteres summiert bei
mehreren Threads die CPU-Zeit und hätte einen SCHNELLEREN Lauf als langsameren gemeldet.

### Log-Sink-Lebensdauer

`FBLogSinkScope` wird als LETZTES deklariert, also als ERSTES zerstört: der Sink-Zeiger von `FBLog` ist
weg, bevor die Sinks und das `FILE*` dahinter verschwinden — auf JEDEM Return, nicht nur dem
erfolgreichen. Eine zweite Mission im selben Prozess (die geplanten Piloten-Turniere) würde sonst durch
einen baumelnden, längst geschlossenen Sink loggen.

---

## 6. Der Spawn eines Missions-Akteurs

`FBMissionBoot.h::FBMissionSpawnActor(models, mission, unitIdx, groundAsl, timeoutS, err)` — header-only,
generisch über `FBModule`, nennt selbst **keinen konkreten Modultyp**. Es ist zugleich die app-seitige
Hälfte des IC-Gates: es inkludiert `fdm/FBFdmBoot.h`, den einzigen Weg zu einer `FBFdm` — und weil ein
`FBSimUnit` nur AUS einer gespawnten Zelle gebaut werden kann, ist dieser Header auch der einzige
Produzent eines vollständigen Akteurs.

**Der Sonderfall zuerst: ein Modul OHNE Airframe** (leeres `FdmModelName()`). Bewusst ein früher Return
statt eines durch die IC gefädelten Zweigs — beide haben nichts gemeinsam: alles ab dort existiert, um
eine JSBSim-Instanz in einen Zustand zu bringen, und diese Einheit hat keine. Geprüft wird nur: Spawn
muss `ground` sein, und `set`-Zeilen sind unzulässig. Geteilt bleibt alles danach (Unit-Objekt,
Identität, Gesundheitsregister, Telemetrie, Missions-Monitor).

**Der Flugzeug-Pfad, der Reihe nach:**

| Schritt | Inhalt |
|---|---|
| 1 | `FBModuleRegistry::Create(block.ModuleName)` — unbekannt ⇒ `nullptr` + Grund |
| 2 | `FBFdmSpawn` füllen: `ModelsRoot = models.Aircraft`, `Aircraft = module->FdmModelName()`, Position, `GroundElevM`, `HeightOffsetM = Ground ? −1 : (AltM − groundAsl)`, `SpeedMs = SpeedKt·kt→m/s`, Heading |
| 3 | `FBFdmBoot::Spawn(ic)` — **die EINE IC-Anwendung**, Boden ODER Luft, kein zweiter Codepfad |
| 4 | `SetGroundElevM(groundAsl)`, `module->AttachFdm(*fdm)` — VOR jedem `Controls()`/`ApplySetup`, das die Zelle erreicht |
| 5 | Missionsdaten aufs Modul: Runway (falls vorhanden), `FlightPlan() = block.Plan` |
| 6 | Startzustand: `Autopilot().SetManual(0,0,0,0)` (Leerlauf-Knüppel — eine Zelle mit echter FLCS wie die F-16 hält von allein die Flügel level, also hat der Preflight-Halt eines Bodenspawns etwas Stabiles zum Sitzen), `Controls().SetGear(true)`, beide Radbremsen auf 1 (die reale Grundstellung; `set gear up` einer Mission überschreibt es für den Luftstart) |
| 7 | `PilotSystem().SetPhase(Ground ? Preflight : Route)` — Boden bekommt die WOW-gegatete Halte-/Startrollmechanik, Luft ist schon etabliert |
| 8 | jede `set <key> <value>`-Zeile via `module->ApplySetup` — Ablehnung ⇒ Spawn nichtig ⇒ Missions-FAIL |
| 9 | `FBSimUnit` bauen (Id = `unitIdx + 1`, Callsign, `module->UnitKind()` **vor** dem Move gelesen: Argument-Auswertungsreihenfolge ist unspezifiziert) |
| 10 | Missions-Monitor **iff** der Block Ziele hat — Wegpunkte ODER `objective`-Zeilen. Gebaut aus der Missions-DATEI (Plan/Objectives/Runway) plus dem AUFGELÖSTEN Timeout, nie aus der lebenden, mutierten Kopie des Moduls. Ohne Ziele: kein Monitor, also erscheint der Akteur nie im Missionsverdikt. |
| 11 | `SetLogAttribution(mission.Units.size() > 1)` |

Der Runner prüft **vor** dem Aufruf zwei Dinge: die Elevation muss aufgelöst sein, und ein expliziter
Luftspawn unter dem Gelände (`AltM < groundAsl − 1 m`) ist ein echter Widerspruch, kein legaler
Sonderfall — die 1-m-Marge fängt Rundung der Elevationsquelle ab, nicht echte Durchdringung.

### `FBMissionSpawnStore` — derselbe Vier-Schritt-Spawn aus dem Trägerzustand

Der zweite Produzent einer vollständigen Einheit und **strukturell dieselben Schritte**: Modul über die
Registry auflösen (der Katalogschlüssel des Stores IST sein Registry-Schlüssel), EINE deklarative IC
anwenden, Airframe anhängen, in ein `FBSimUnit` wickeln. Er unterscheidet sich in genau zwei Dingen, und
beide sind Eigenschaften dessen, was ein abgeworfener Store IST: die IC kommt aus dem **Trägerzustand**
statt aus einer Missionsdatei, und die Einheit ist `FBUnitKind::Weapon`.

**Der Separationszustand, vollständig:**

| Größe | Herleitung |
|---|---|
| **Position** | Trägerposition + Stationsversatz, aus Körperachsen mit der Trägerlage herausgedreht (`FBBodyVecToEnu`). Ein Store verlässt den Pylon, nicht den CG. Meter → Grad über `kMPerDeg` bzw. `kMPerDeg·cos(lat)`. |
| **Lage** | die des Trägers, unverändert. Nichts kippt ihn vom Träger; was die Zelle in dem Moment tat, tut der Store weiter. |
| **Geschwindigkeit** | Trägergeschwindigkeit **an dieser Station**, also CG-Geschwindigkeit + **ω × r**. Die Rotationskomponente zählt in dem Moment, in dem ein Abwurf in einer Rolle passiert; sie wegzulassen wäre eine stille Vereinfachung statt einer Modellentscheidung. Berechnet in Körperachsen (`p,q,r` deg/s → rad/s), dann dieselbe Rotation nach ENU. |
| **Ejektor-Impuls** | **bewusst KEINER.** Ein realer Pylon drückt den Store mit einigen ft/s nach unten, und `doc/f16/weapons.md` hat dafür keine zitierbare Zahl (§4.5s Stationsdaten sind bestenfalls T4). Also separiert der Store mit der Trägerbewegung und sonst nichts. Kommt eine Quelle, ist es EIN zusätzlicher körperfester Geschwindigkeitsterm, hier, an dieser einen Stelle. |

`HeightOffsetM` wird auf mindestens 0,5 m gehoben (die IC braucht einen positiven Offset; darunter
schlägt der Store ohnehin gleich auf). `ic.Ballistic = true` schaltet im Adapter Trimm und
Triebwerksstart ab (→ [`fdm.md`](fdm.md) §6). `module->ProgramRelease(rel)` ist die generische
Startprogrammierung: eine Bombe ignoriert sie, eine gelenkte Runde nimmt Schützen-Id und Zielschätzung
daraus — diese Datei nennt keinen Waffentyp. Geloggt wird `stores SEPARATION` mit dem vollen
Zustandsvektor.

### Die Tick-Semantik neu angehängter Einheiten

**Die Akteursliste wächst an genau einer Stelle**: am ENDE des Ticks, in dem der Abwurf/Schuss
kommandiert wurde — gerechnet wird die neue Einheit erst im NÄCHSTEN.

**Warum das kein Komfort ist, sondern Determinismus:** die STEP-Phase läuft einen Job je Akteursindex
(§9); ein Akteur, der MITTEN in der Phase auftaucht, würde das Laufergebnis davon abhängig machen, WANN
in der Phase er erschien. Anhängen an der Barriere hält den ganzen Tick zu einem Snapshot, genau wie jede
Pose.

**Die Ordnung ist bis unten deterministisch:** Akteure werden in Listenreihenfolge geleert, die
Freigabe-Queue jedes Akteurs ist FIFO, und jede neue Einheit wird in dieser Reihenfolge angehängt — also
sind Liste, Tick-Ordnung und Unit-Ids in einem 1-Thread- und einem N-Thread-Lauf identisch.

**Keine Allokation im Tick-Pfad:** die Kapazität wird vorreserviert — erst für die deklarierten Units,
dann exakt (`maxActors = Akteure + Σ LoadedCount()`), sobald jedes Modul sein Loadout angewandt hat: EINE
weitere Einheit je belegter Station und keine mehr, denn ein Store kann einmal freigegeben werden. Alles
indexparallele (Log-Puffer, `PrevPose`, Roster) wird auf DIESE Decke dimensioniert, damit ein
mittendrin erscheinender Store keinen Puffer resized, den ein Worker-Thread gerade referenziert.

---

## 7. Der Tick, Phase für Phase

Aus der Schleife in `FBMissionRunner.cpp` (`dt = 0.1 s`):

| # | Phase | Parallel? | Warum |
|---|---|---|---|
| 1 | Elevation je aktivem Akteur | **nein** | Der Provider ist das EINE geteilte Objekt des Clients (`FBTilesElevation` fährt den Tile-Streamer); eine Punktabfrage pro Tick ist viel zu billig, um die Frage überhaupt zu stellen. |
| 2 | **STEP** (`Module::Run` + eigene FDM-Substeps) | **ja** (`FBTickPool`) | Die Indizes sind unabhängig (§9). |
| 3 | Log-Puffer in Akteursreihenfolge drainieren | nein | Zeilenposition darf nicht am Scheduler hängen. |
| 4 | `PublishPose` | nein | **IST die Barriere.** |
| 5 | `simT += dt`, Roster bauen | nein | Roster = Callsign, Fraktion, das eine Bit aus dem Gesundheitsregister; Waffen bleiben draußen (eine Runde in der Luft ist niemandes Ziel). |
| 6 | `CheckEnvelope` + `RunMonitors` | nein | Damit das Urteil, das einen Lauf beendet, und die Zeilen, die es emittiert, in AKTEURS-Reihenfolge gelesen werden, nie in Fertigstellungs-Reihenfolge. |
| 7 | Telemetrie-Sampling | nein | Entscheidung. |
| 8 | Geschosse fliegen + auflösen | nein | Sie gehören dem Client, nicht einem Modul. |
| 9 | Stores: Zünder, Aufschlag, Ablauf | nein | dito |
| 10 | `hook->OnTick` | nein | der Renderer des nativen Orakels, einthreadig per Entscheidung |
| 11 | **Wachstum**: Feuerstöße + Freigaben leeren, neue Akteure anhängen | nein | §6 |
| 12 | `PrevPose` aller Akteure merken | nein | Letzter Akt, NACH dem Wachstum, damit auch eine in diesem Tick erschienene Einheit ab jetzt einen Eintrag hat. |

---

## 8. Was der Runner sonst noch auflöst (nicht in der Aufgabenliste, aber im File)

Diese Dinge stehen im Orchestrator, weil sie zwischen ZWEI Einheiten passieren und auf der **Wahrheit**
(den publizierten Posen) aufgelöst werden müssen — dieselbe Grenze wie bei den zwei Richtern: eine Waffe,
die sich auf ihrer eigenen Schätzung selbst bewertet, wäre die reinste Form von Cheaten.

- **`ClosestApproach` (CPA).** Warum keine Abstandsprüfung je Tick: der Tick ist 0,1 s, eine
  Head-on-Annäherung kann 1.500 m/s überschreiten, aufeinanderfolgende Samples liegen also **150 m**
  auseinander — eine reine Pro-Tick-Prüfung gegen einen 10-m-Zünderradius würde fast jeden echten
  Treffer verpassen. Also Minimum über das SEGMENT, Standard-CPA auf `p(t) = p0 + t(p1−p0)`, `t ∈ [0,1]`.
  Die Geradenannahme innerhalb eines Ticks ist bei 20 g etwa **einen Meter** Krümmung wert — im Banner
  ausgesprochen statt versteckt. `FracT` macht die Ereigniszeit sub-Tick.
- **Die Schärfverzögerung** (`Perf.ArmingS`) ist das, was einen Start davon abhält, auf dem eigenen
  Träger zu detonieren: eine Runde, die 3 m neben dem Jet den Pylon verlässt, zählt nicht als Treffer
  auf ihn.
- **`ResolveBurst`.** Der CPA-Vektor wird in den Körperrahmen des ZIELS gedreht (`FBEnuToBodyVec`, mit
  der publizierten Lage) und an `core/FBDamageModel` gereicht — über die Einheit, die das Register
  besitzt. Die WAFFE liefert eine Zahl (Sprengmasse), das MODUL des Ziels eine Tabelle (wo seine Systeme
  sitzen), keiner von beiden entscheidet etwas.
- **`ResolveGunHit`.** Strukturell identisch; anders ist, was ankommt: ein Gefechtskopf ist eine Masse,
  aus der das Modell eine Energie ableitet — ein Feuerstoß ist eine ANZAHL Geschosse in einem Muster,
  also wird die Energiedichte hier gerechnet (Fehlabstand, Musterbreite `σ = DispersionSigmaRad · PathM`
  mit Minimum 0,05 m, Relativgeschwindigkeit, präsentierte Fläche). Tore: `kMinReportedHits = 0.1`
  (ein Zehntel Geschoss ist bequem unter einem Treffer und bequem über dem Rauschen der stetigen
  Dichtefunktion), `kGunHitReachM = 8 m` (die halbe Spannweite eines Jägers, ADDIERT auf 3σ — kein
  Trefferradius, sondern der Punkt, ab dem das Dichtemodell nur noch Null liefern kann),
  `kGunNearMissM = 200 m` (ab wann ein Vorbeischuss überhaupt eine Zeile wert ist). Der Startende ist vom
  Beschuss ausgenommen (`LauncherId`).
- **`GroundCrossing` — der sub-Tick-Aufschlag.** Der Richter läuft auf 0,1 s, also ist ein Store, wenn er
  als „durchgeschlagen" beobachtet wird, schon bis zu einen Tick unter der Oberfläche: **gemessen an
  einer Mk-82 bei 216 m/s: 14 m Tiefe, also ~20 m Horizontalweg jenseits des wahren Aufschlagpunkts** —
  ein Fünftel des gesamten Abwurffehlers, den die Angriffsmissionen messen sollen, und ein reines
  Abtastartefakt. Rekonstruktion: Tiefe / Sinkrate = `backS`, Position linear zurückprojiziert (die
  Krümmung des Bogens ist über ~0,1 s Zentimeter wert). Möglich ist das nur, weil eine Waffe **absichtlich
  keinen Boden zum Kollidieren bekommt** (`kWeaponNoGroundElevM`), also bis zuletzt ballistisch fliegt.
  **Bewusst NICHT** zwischen den letzten zwei publizierten Posen interpoliert: bis der Physik-Richter
  schließt, ist auch die vorige Pose schon unter der Oberfläche, es gibt also kein einschließendes Paar.
- **Warum eine Waffe keinen Boden bekommt:** JSBSims Bodenreaktionen modellieren ein RUHENDES Objekt —
  die zwei STRUCTURE-Kontakte des Mk-82-Modells sind eine Feder mit 10.000 lbf/ft und 200.000
  lbf/ft/s Dämpfung. Bei 150 m/s ist das eine steife ODE, die **innerhalb eines Schritts divergiert**
  (gemessen: die Integration sprengt auf dem Kontaktschritt, es bleibt kein Aufschlagzustand zu melden).
  Ein Store springt nicht — er detoniert.
- **`ResolveGroundBurst`.** Das ungelenkte Gegenstück, dieselbe 1/r²-Splitterphysik. Das Nähe-Tor ist
  ABGELEITET statt gewählt: die niedrigste Schwelle, die das Layout des Ziels selbst deklariert, ist die
  geringste Energie, die ihm überhaupt etwas antun kann. Gegen **Flugzeuge** wird ein Bodenburst
  ausdrücklich NICHT aufgelöst — die Splittergeometrie gegen eine Zelle gibt es nicht, und ein erfundener
  Radius wäre eine Zahl, die sich als Physik ausgibt.
- **`stores DELIVERY`.** Die Vorhersage der Feuerleitung reist auf der Runde mit (`FBStoreTrack::Solution`)
  und wird neben dem gemessenen Aufschlag ausgegeben: `predErrM` = was der COMPUTER falsch hatte (grobe
  gespeicherte Tabelle gegen die Aerodynamik des Modells — der Fehler, den die CCIP/CCRP-Anordnung
  aufdecken soll), `aimErrM` = was die ABGABE falsch hatte, plus Längs-/Querfehler in der
  Anflugrichtung der Runde und die Flugzeitdifferenz.

---

## 9. Multi-Unit — was jede Etappe gebaut hat

| Etappe | Inhalt | Beweis |
|---|---|---|
| **1** | Der `fdm/`-Adapter ist instanzfähig: `FBFdm` ist ein Objekt je Zelle, keine globale Instanz, keine statischen mutablen Globals. | `make -C sim test-fdm` → `build/fb-test-two-fdm` (zwei divergierende Zellen + eine dritte, die die erste bit-für-bit reproduziert) |
| **2** | Der Akteur ist EIN Objekt: `units/FBSimUnit`. | — |
| **3** | Der Verband ist MISSIONSDATEN: `.fbm` trägt eine Liste von `unit`-Blöcken; jeder Client hält eine `FBActorList`; jede Einheit hat eigene `FBFdm`/`FBModule`/Monitore/Telemetriedatei; das Missions-Urteil fällt PRO EINHEIT. Die **Snapshot-Disziplin steht ab hier**, obwohl noch niemand cross-unit las. | — |
| **4** | **Thread je Einheit, aber NUR im Gym** (`fb-gym --threads N`, Default 1). Parallelisiert GENAU eine Phase: den STEP. | Fingerabdruck-Vergleich, s. u. |
| **5** | Einheiten sehen einander — aber nur über ein System: der kooperative Datalink. | — |
| **6** | Das FCR-Radar als aktiver Sensor daneben. | — |
| **8** | Das Avionik-Datenmodell (`FBState` als typisierter Block-Bus, Kommando-/Quittungspfad). | — |

### Etappe 4 im Detail — Pool und Barriere

`app/FBTickPool` ist C++17-Eigenbau (`std::barrier` ist C++20) und **GYM-ONLY per Entscheidung**: native
und wasm bleiben im Sim-Loop einthreadig (Echtzeit braucht keine Parallelphysik, und der Browser erspart
sich den pthreads/SharedArrayBuffer-Build). Der Header wird **ausschließlich** von
`app/FBMissionRunner.cpp` inkludiert, ist NICHT Teil der Core-Lib und erreicht den WASM-Build nie.

| Element | Verhalten |
|---|---|
| Threads | N−1 Worker, EINMAL für den Lauf erzeugt (bei 10 Hz über Tausende Ticks wäre ein Thread pro Tick reiner Spawn-Overhead), parken auf einer Condition-Variable |
| `RunTick(job, count)` | ruft `job.RunIndex(0..count−1)` **genau einmal je Index**, verteilt über die Worker PLUS den rufenden Thread |
| Zeitplan | **dynamisch**: ein atomarer Zähler (`Next_.fetch_add`) — wer frei ist, nimmt den nächsten Index. Für ungleiche Last gedacht (eine Einheit am Boden neben einer im Reiseflug). |
| Barriere | das RETURN von `RunTick` IST sie (`Done_.wait(Busy_ == 0)`) |
| `--threads 1` | erzeugt **gar keinen** Thread; `RunTick` degeneriert zur inline-Schleife — der sequenzielle Referenzpfad, strukturell derselbe Code statt eines zweiten |
| `Generation_` | pro Tick hochgezählt: das Aufweck-Prädikat, immun gegen Spurious Wakeups; im Destruktor ebenfalls erhöht, damit ein geparkter Worker die Änderung sieht und nicht nur `Stop_` |
| Rufender Thread | arbeitet mit (ein Thread weniger zu wecken, und er kann nicht an der Barriere idlen) |
| Sizing | vom Runner auf die Besetzungsgröße geklemmt — mehr Threads als Akteure sind Leerlauf, keine Geschwindigkeit |
| Deklarationsreihenfolge | Der Pool wird ZULETZT deklariert, also ZUERST zerstört: seine Threads werden gejoint, solange Puffer und Job noch leben |
| Logging über den Pool | **nichts.** Wie viele Threads die Besetzung stepten, ist eine Eigenschaft des Clients, kein Ereignis der Mission — eine Zeile darüber wäre der EINZIGE Unterschied zwischen einem sequenziellen und einem parallelen `events.log`. |

### Was sequenziell bleibt — und der jeweilige Grund

| Bleibt sequenziell | Grund |
|---|---|
| Laden/Spawnen der Modelle | JSBSims statische `Element::convert`-Einheitentabelle wird beim XML-Parsen per `operator[]` MUTIERT (→ [`fdm.md`](fdm.md) §3) |
| Elevation-Sampling | Der Provider ist das EINE geteilte Objekt des Clients (`FBTilesElevation` fährt den Tile-Streamer); und eine Punktabfrage pro Tick ist zu billig, um die Frage zu stellen |
| `PublishPose` | Das IST die Barriere |
| Beide Monitore + Hüllkurven-Checks | Das Urteil, das einen Lauf beendet, und die Zeilen dazu müssen in Akteursreihenfolge gelesen werden, nie in Fertigstellungsreihenfolge |
| Telemetrie-Sampling | Entscheidung; der Bus ist ohnehin pro Einheit |
| `FBMissionTickHook` | Der Renderer des nativen Orakels, einthreadig per Entscheidung |
| Wachstum der Akteursliste | §6 |

### Log und Telemetrie ohne Determinismusverlust

- **Telemetrie** ist längst pro Einheit (eigener Bus, eigene Datei) und wird in der SEQUENZIELLEN Phase
  gesampelt — also gar kein Problem.
- **`FBLog`** behält seine statische Fassade (Cross-Cutting-Infrastruktur; die Alternative wäre gewesen,
  ein Kontextobjekt durch jede `Run()`-Signatur zu fädeln — genau das, was die Fassade vermeidet), aber
  ihr **KONTEXT ist `thread_local`**: `TimeS_`, `Unit_[32]` und ein `ThreadSink_`. Die **KONFIGURATION**
  (`Sink_`, `Level_`) bleibt prozessweit — sie ist Boot-Konfiguration.
- **Kein Worker schreibt je direkt in einen gemeinsamen Sink.** `FBActorStepJob::RunIndex` legt per RAII
  (`FBLogThreadSinkScope`) den `FBBufferedLogSink` **der Einheit** an, die dieser Thread rechnet, plus
  `FBLogUnitScope` mit ihrem Callsign, und stempelt die Simulationszeit (`FBLog::SetTime`) — der Worker
  lernt den Tick aus dem Job, weil die Uhr thread-lokal ist.
- **An der Barriere** drainiert der Runner die Puffer **in Einheitenreihenfolge** in den echten Sink
  (`for (auto &l : actorLogs) l.Drain(logSink)`). Damit hängt nicht einmal die Position einer Zeile am
  Scheduler. `FBBufferedLogSink` kopiert `tag`/`event` als `std::string` (eine gepufferte Zeile überlebt
  den `Emit()`-Aufruf, und nur String-Literale hätten das zufällig überlebt) und behält beim Drainieren
  seine Kapazität — ein Steady-State-Tick ohne Logausgabe allokiert nichts.
- Der Job reicht `world = nullptr` durch, exakt wie zuvor.

### Determinismus-Beweise (aus CLAUDE.md)

- `payerne-pair`, `payerne-pair-fail`, `payerne-four`, `payerne-mixed` liefern über `--threads 1..4` und
  je **5 Wiederholungen** EINEN einzigen Fingerabdruck: SHA-256 aller `telemetry*.csv` + normalisierter
  `events.log` + Exit-Code, inklusive der `decisive=`-Attribution.
- Die 7 Einzel-Missionen × `const`/`swiss` sind mit dem Default **byte-identisch** zum Stand vor Etappe 4.

### Skalierung, ehrlich

| Messung | Wert |
|---|---|
| Kosten eines F-16-Steps | ~95–100 µs, praktisch **phasenunabhängig** (Bodenroll vs. Reiseflug ≤ 7 % Unterschied) — eine Mission kann über Flugphasen also kaum Ungleichlast erzeugen |
| 2 Einheiten, 2 Threads | **1,29–1,41x** |
| 4 Einheiten, 2 / 3 / 4 Threads | **1,49x / 1,53x / 1,77x** |
| Maschine | Apple A18 Pro, 2 P- + 4 E-Kerne |
| Zwei UNABHÄNGIGE `fb-gym`-Prozesse | 0,42 s allein → **0,58 s je**, also 1,45x aggregat — sie skalieren genauso schlecht |
| Spin-vor-Park-Variante der Barriere (gebaut, gemessen, verworfen) | 1,41x vs. 1,41x bei zwei Threads; 1,72x vs. 1,68x bei vier — innerhalb der Lauf-zu-Lauf-Streuung |

**Die Decke ist die MASCHINE, nicht die Barriere.** Threading lohnt ab ~4 Einheiten auf echten
Performance-Kernen; darunter ist es ein Faktor < 1,5.

---

## 10. Telemetrie bei N > 1

| Regel | Inhalt |
|---|---|
| Eine Datei je Einheit | Primärer Akteur (Index 0) behält den kanonischen Namen `telemetry.csv`; jeder weitere bekommt `telemetry_<callsign>.csv` |
| Callsign-Sicherheit | Der Parser beschränkt das Callsign auf `[A-Za-z0-9_-]`, damit es dateisicher ist |
| Feste Spaltenzahl | pro Datei; neue Quellen hängen hinten an (§3) |
| Ein Store bekommt seine Datei beim Erscheinen | Schlägt das Öffnen fehl, fliegt er trotzdem — nur seine Trace fehlt (`StartTelemetry(nullptr)`) |

**Warum keine breite Zeile mit Präfix-Spalten:** das Spaltenset eines Akteurs folgt SEINEM Modul. Eine
geteilte Zeile würde entweder alle Module in ein Schema zwingen oder den Header von der Besetzung der
Mission abhängig machen. Die Datei-je-Einheit braucht bei N=1 keinen Sonderfall — die Zeilen bleiben
byte-identisch.

---

## 11. `FBModelRoots` — die EINE Modellwurzel

**Warum eine.** Alles, was FlightBox fliegt, liegt unter `sim/assets/aircraft` — ein selbstständiges
Verzeichnis je Modell (`.xml` plus eigene `engine/`- und `Systems/`-Unterverzeichnisse, JSBSims eigenes
Pro-Flugzeug-Layout). Heute `f16`, `mk82` und `aim120`.

Das gepinnte Submodul ist **kein Ladepfad mehr, sondern die Basis**: der Upstream-Stand, gegen den
`make -C sim verify-models` jede Kopie diffed (siehe [architecture.md](architecture.md)s Delta-Regel,
Delta-Regel). Die frühere Zwei-Wurzel-Aufteilung („aus dem Submodul, weil read-only" gegen „bei uns,
weil das Submodul es nicht hat") trägt nicht mehr, sobald ein Modell korrigiert werden darf: aus dem
Submodul geladen kann es keine Korrektur tragen, aus einer Kopie unbekannter Herkunft geladen ist es
keine Referenz. Damit ist auch `FBModule::FdmModelVendored()` ersatzlos entfallen — eine Unterscheidung
ohne Wirkung.

| Client | Modellwurzel |
|---|---|
| native / gym (relativ zu `sim/`) | `assets/aircraft` |
| WASM (eingebettetes FS, s. `--embed-file` im `wasm`-Target) | `/fb/aircraft` |

`FBNativeModelRoots()` ist die EINE Definition der native/gym-Wurzel für jeden Client, der aus `sim/`
läuft (beide Apps und jeder Test-Harness) — statt Stringliteralen, die auseinanderdriften können.

**Liegt in `app/`**, weil nur `app/` ein Airframe bootet (das IC-Gate): nichts unter `systems/` oder
`modules/` erreicht einen Modellpfad, so wenig wie eine Initialbedingung.

---

## Offene Punkte

- **Erledigt (Kommentar-Runde):** die vier veralteten Banner-Aussagen in `FBUnit.h` („planned per-unit
  threading"), `FBUnitRegistry.h` („heute Datalink, morgen das Radar"), `FBSimUnit.h` (`GetSignature`
  „heute sein Datalink-Sender") und `FBModule.h` (`AttachFdm`, „`units/FBUnit` later") sind mit den
  Bannern selbst entfallen; ebenso `FBSimUnit.h`s Zählwiderspruch „exactly the six places", an denen ein
  fehlendes Airframe zählt.
- **`FBMissionRunner.h`s Docstring ist unvollständig:** „Returns 0/1/2/3 = Success/Fail/Crash/Timeout"
  nennt `LOC` nicht, das sich Code 2 mit `Crash` teilt (im `FBMissionResult`-Banner korrekt erklärt).
  Ebenso beschreibt derselbe Docstring `FBRunMission` noch als „Ground-spawns `missionPath`'s module" —
  Einzahl, obwohl es längst eine Besetzung ist.
- **Der Detonations-Banner ist überholt:** „What a hit DOES is deliberately not modelled yet — this is
  the event, not a damage verdict" steht unmittelbar vor dem `ResolveBurst`-Aufruf, der genau das tut.
- **`FBUnitRegistry` hat kein `Unregister`.** Eine retirierte Einheit bleibt registriert (sie verstummt
  nur). Jeder Sensor muss also selbst mit stillen Einträgen umgehen; ob jeder das tut, wurde in dieser
  Runde nicht geprüft.
- **Kein Cross-Check der WASM-Schleife.** `app/FBAppWasm.cpp` ruft nachweislich `PublishPose`,
  `RunMonitors`, `PrimeState` und `FBWorld::SetUnits`, aber ob sie die Phasenreihenfolge des Runners
  exakt spiegelt (insbesondere Elevation vor STEP, Roster-Aufbau), wurde nicht Zeile für Zeile
  verglichen.
- **`FirstFlightKo`/`ExpectedLoss`/`FirstJudged` sind O(N²) über die Akteursliste** und laufen in der
  Schleifenbedingung, also mehrfach pro Tick. Bei den heutigen Besetzungsgrößen (< 10) irrelevant,
  aber nicht dokumentiert als Grenze.
- **Etappe 7 fehlt in der Historie.** CLAUDE.md nennt Etappen 1–6 und 8; eine 7 wird nirgends erwähnt.
  Ob sie übersprungen oder aufgegangen ist, geht aus keiner Quelle hervor.
- **`--elev`-Defaults, Turnierläufer, `.fbm`-Syntax** sind hier bewusst nicht wiederholt — sie stehen in
  [`doc/mission-format.md`](../mission-format.md) bzw. gehören in die Nachbardateien dieser
  Wissensbasis (Elevation-Hook, Piloten-Varianten).
