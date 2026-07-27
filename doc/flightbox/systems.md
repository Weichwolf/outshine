# FlightBox — Generische Systemslots (`sim/src/systems/`)

Quellen dieser Datei: die Kommentar-Banner der Quelldateien unter `sim/src/systems/` (Stand: Branch
`systems`, Commit `8cd3a74`) plus CLAUDE.mds `systems/`-Abschnitt. Jede Herleitung, jede Messzahl und
jede Setzung ist von dort übernommen; nichts ist extrapoliert. Wo Code und CLAUDE.md auseinandergehen,
steht es unter „Offene Punkte".

**Geltungsbereich:** die airframe-agnostischen Systemslots OHNE Sensorik, Pilot und Waffen.

| Nicht hier | Sondern |
|---|---|
| `FBDatalinkSystem`, `FBRadarSystem`, `FBRwrSystem`, `FBCountermeasureSystem` | `sensors.md` |
| `FBPilot`, `FBPilotTuning`, `FBBfmTrack`, `FBEngagement` | `pilot-ai.md` |
| `FBStoresSystem`, `FBGunSystem`, Schadensmodell | `weapons-and-damage.md` |

---

## 1. Das Slot-Muster

### 1.1 Die Regel

CLAUDE.md: **FBCore → Interface → Default-Implementation → modul-spezifischer Override.**
In `systems/` heißt das konkret:

1. **Interface und Default sind EINE Klasse.** Kein `IFoo`/`FooImpl`-Split. Die Klasse ist direkt
   instanziierbar und liefert das generische Verhalten.
2. **Genau EIN Override-Punkt je Slot** — fast immer `Run()`, bei `FBDisplaySystem` zusätzlich
   `BuildHud()`. Ein Modul, dessen System sich *im Verhalten* unterscheidet, leitet ab und
   überschreibt diesen einen Punkt.
3. **Zahlen-Tuning ist KEINE Ableitung.** Gains/Konfiguration sind public Datenfelder auf der
   Basisklasse oder eine statische Preset-Factory (`FBFlightControl::F16()`). Eine leere Ableitung,
   die nur Konstanten ändert, existiert nicht.
4. **Der Default ist entweder ECHT oder NoOp.** Beides ist zulässig; ein NoOp-Default kostet eine
   Throttle-Vergleichsoperation und einen leeren virtuellen Aufruf.
5. **Das Modul KOMPONIERT die Slots** (`std::unique_ptr<Base>`, nicht Wertmember — sonst Slicing beim
   Austausch), besitzt sie und taktet jeden im eigenen Rhythmus. Slots rufen sich nie gegenseitig.

### 1.2 Der Slot-Bestand

| Slot | Datei | Default | Override-Punkt | Zustand |
|---|---|---|---|---|
| Guidance | `systems/FBAutopilot.h/.cpp` | ECHT | `Run(const fb_fdm_state&) → FBGuidance` | voll |
| Flight Control | `systems/FBFlightControl.h/.cpp` | ECHT | `Run(const FBGuidance&, const fb_fdm_state&) → FBControls` | voll |
| Air Data | `systems/FBAirDataSystem.h/.cpp` | ECHT | `Run(FBState&, const fb_fdm_state&, dt)` | voll |
| Radarhöhenmesser | `systems/FBRadarAltimeter.h/.cpp` | ECHT | `Run(FBState&, elevAslM, groundAslM)` | voll |
| Warnungen | `systems/FBWarningSystem.h/.cpp` | ECHT | `Run(FBState&, dt)` | 3 Bits |
| Navigation | `systems/FBNavSystem.h/.cpp` | ECHT | `Run(FBState&, const fb_fdm_state&, dt)` | 1 Steerpoint + Bullseye |
| Displays | `systems/FBDisplaySystem.h/.cpp` | ECHT | `BuildHud()` (+ `Run()`) | generisches HUD |
| Airframe-Controls | `systems/FBAirframeControls.h/.cpp` | NoOp + reale Ableitung | jede virtuelle Methode | voll (`FBJsbsimAirframeControls`) |
| Input/HOTAS | `systems/FBSystemSlots.h` (`FBInputSystem`) | NoOp | `Run(FBMasterMode, dt)` | leer |
| Propulsion | `systems/FBSystemSlots.h` (`FBPropulsionSystem`) | NoOp | `Run(const fb_fdm_state&, dt)` | leer |
| Weapons (Alt-Stub) | `systems/FBSystemSlots.h` (`FBWeaponSystem`) | NoOp | `Run(FBMasterMode, const FBWorld*, dt)` | vestigial, s. Offene Punkte |

**Was aus `FBSystemSlots.h` HERAUSGEWACHSEN ist** — weil sein Default REAL statt NoOp wurde, bekam es
eine eigene Datei: Displays (`FBDisplaySystem.h`), Comms/Datalink (`FBDatalinkSystem.h`), Sensors
(`FBRadarSystem.h`), Defensiv (`FBRwrSystem.h` + `FBCountermeasureSystem.h`), Stores
(`FBStoresSystem.h`), Gun (`FBGunSystem.h`). Deshalb steht in `FBSystemSlots.h` weder ein
`FBCommsSystem`- noch ein `FBSensorSystem`-Stub.

### 1.3 Die Kommunikationsregeln (im Signaturtyp kodiert)

| Regel | Mechanik |
|---|---|
| Sensoren SCHREIBEN `FBState`, Displays LESEN | Sensor-Slots nehmen `FBState&`, `FBDisplaySystem::BuildHud` nimmt `const FBState&` und ist selbst `const` |
| Ein Block, ein Schreiber | jeder Block in `core/FBAvionicsBlocks.h` nennt sein Schreibersystem im Kommentar; Leser konsultieren erst den Gültigkeitskopf |
| Kein Peer-Aufruf | kein Slot hält einen Zeiger auf einen anderen; die Kopplung läuft ausschließlich über `FBState` |
| Welt nur geborgt | wer andere Einheiten/Terrain braucht, bekommt `const FBUnitRegistry*` / `const FBWorld*` pro Tick, nie global, nie als Member |
| Genau EINE Fusion-Ausnahme, dokumentiert | Feuerleitung liest Nav + Platform; `FBWarningSystem` liest RadarAlt + Ufc + Airframe — beides steht im jeweiligen Blockkommentar |

### 1.4 Takt (wie `FBF16Module` die Slots cycelt)

`modules/f16/FBF16Module.cpp::Run()` ist der einzige Ort, an dem diese Slots getaktet werden. `Due()`
ist ein Akkumulator-Throttle (`accS += dt; if (accS < 1/hz) return false; accS -= 1/hz;`).

| Gruppe | Rate | Inhalt (in Reihenfolge) |
|---|---|---|
| Input, Propulsion | pro `Run()` (gröbster Sim-Tick) | `Input->Run`, `Propulsion->Run` |
| „Sensor-Gruppe" | 10 Hz | `PublishPlatform` → `PublishAirframe` → Kommandos (Sensors/Avionics/Stores) → Radar → **AirData** → **RadarAlt** → Steerpoint setzen → **Nav** → FireControl → Ufc → Sms → **Warnings** (letzter: reiner Konsument) |
| Displays | 20 Hz | `Disp->Run` (die Anzeigenlogik, NICHT `BuildHud`) |
| Weapons (Stub) | 20 Hz | `Weapons->Run` |
| Gun | pro `Run()`, volles `dt` | Rundenzahl wird integriert — Throttling würde Schüsse erfinden/verschlucken |
| Defensiv | 10 Hz | RWR → Kommandos → CMDS |
| Comms | 5 Hz | Kommandos → Datalink |
| Pilot | 10 Hz | `PilotSys->Run` → `SharedState.Bfm` → `NavSys->AdvanceWaypoint` |
| **Guidance + FlightControl** | **100 Hz** | fester Substep-Loop, Spiralschutz ≤ 12 Substeps/Frame |

Der innere Loop, wörtlich die Regelkette:

```
for (k=0; AccS >= FBFdm::kStepS && k < 12; k++) {
  LastG = AP->Run(st);                 // Guidance
  FBControls c = FC->Run(LastG, st);   // FBW-Innenschleife
  fdm.SetControls(c.Roll, c.Pitch, c.Yaw, c.Thr);
  fdm.Step(st);                        // JSBSim, 0.01 s
  AccS -= FBFdm::kStepS; LastSub++;
}
```

`AP->Run()`/`FC->Run()` sind die EINZIGEN virtuellen Dispatches innerhalb dieses Loops (je einer pro
Substep). Alles andere ist außerhalb und höchstens einmal pro `Run()`.

**Reihenfolge-Begründungen (aus dem Modul-Banner):**
- Platform/Airframe werden ZUERST publiziert, weil die Systeme darunter sie lesen — „ein Schreiber pro
  Block" bleibt nur so wahr (früher las die Feuerleitung ein Höhenfeld, das niemand füllte).
- Kommandos einer Gruppe werden VOR den Boxen dieser Gruppe abgearbeitet: ein geworfener Schalter wirkt
  auf den nächsten Sweep, nicht auf den übernächsten.
- AirData/RadarAlt/Nav/FireControl/Ufc/Sms sind EINE Throttle-Gruppe, damit FireControl Navs
  Ausgabe DESSELBEN Ticks liest.
- Warnungen zuletzt: reiner Konsument, inklusive der Gültigkeitsköpfe.

### 1.5 Schadens-Gate (`core/FBSystemHealth`, nur lesend)

Das Modul entscheidet je Slot vor dem Takt:

| Zustand | Folge |
|---|---|
| `Intact` | Slot läuft normal |
| `Degraded` | Slot läuft, mit modellierter Einschränkung (nur wo ABLEITBAR — z. B. Radarreichweite ×0,707) |
| `Failed` | Slot wird **gar nicht getaktet**, sein Block wird `Invalid` |

Für die hier dokumentierten Slots: `FBSystemId::AirData` → AirData-Block, `RadarAlt` → RadarAlt-Block,
`Nav` → Nav- UND Cruise-Block (dieselbe Box publiziert beide Nachrichten). Guidance/FlightControl haben
KEIN Gate im Modul — Steuerungsschaden wirkt physisch über `FBFdm::SetControlAuthority`, d. h. die FLCS
kommandiert unverändert weiter und das Flugzeug antwortet nur nicht mehr.

---

## 2. `FBAutopilot` — die Guidance

`systems/FBAutopilot.h`, `systems/FBAutopilot.cpp`. Portiert aus `flightctl.h`s äußerer Schleife,
Numerik verbatim übernommen (äquivalenzgetestet); COURSE ist neu (Phase 3, die Landung) und sitzt auf
derselben Ebene wie DIRECT, nicht als Subklassen-Override.

### 2.1 Ausgabe: `FBGuidance`

| Feld | Bedeutung |
|---|---|
| `Mode` | `FBMode::Manual` / `Direct` / `Course` (`core/FBMode.h`) |
| `BankCmdDeg` | kommandierte Schräglage, deg |
| `AltErrM` | Ziel-Höhe − Ist-Höhe, m, UNGECLAMPT (Rohmaterial für die Innenschleife) |
| `TargetVsMs` | gewünschte Vertikalgeschwindigkeit aus der Höhenschleife, m/s |
| `TargetSpeedMs` | zu haltende Fahrt, m/s |
| `ManualRoll/Pitch/Yaw/Thr` | Durchgriff in MANUAL |
| `RingDistM` | Diagnose: Entfernung zum Direct-Zielpunkt (in COURSE: Distance-to-go) |

### 2.2 Modi und ihre Setter

| Modus | Setter | Was geregelt wird |
|---|---|---|
| MANUAL | `SetManual(roll,pitch,yaw,thr)` | nichts — Durchgriff bis in die Innenschleife |
| DIRECT (Punkt) | `SetDirect(lat,lon,altM,speedMs)` | Peilung zum Punkt + Höhenhaltung |
| DIRECT (Bahn) | `SetDirectLeg(fromLat,fromLon,lat,lon,altM,speedMs)` | die TRACK-Linie von→nach + Höhenhaltung |
| COURSE | `SetCourse(refLat,refLon,courseDeg,refElevM,glidepathDeg,speedMs)` | unendliche Linie durch den Referenzpunkt + gerader Gleitpfad |

`SetDirectLeg` ruft intern `SetDirect` und setzt danach `HaveLeg` — **derselbe Modus mit optionalem
Bahnursprung**, kein zweiter Modus.

### 2.3 „Ein Punkt ist keine Bahn" — warum EIN Modus mit optionalem Ursprung

Aus dem Klassen-Banner:

- Peilungsverfolgung (pure pursuit) ist das RICHTIGE Gesetz, wenn ein Punkt alles ist, was existiert:
  Abfang, Kurvenkampf, Suche — dort gibt es keine Linie, auf der man sein müsste.
- Sie ist das FALSCHE Gesetz, sobald eine Bahn existiert, und zwar aus einer Eigenschaft der pure
  pursuit, nicht aus Tuning: **ihre Positionssteifigkeit fällt mit 1/R**, auf Entfernung kann sie eine
  Linie also gar nicht halten.
- **Messung:** auf dem 19 km langen CCRP-Run-in trieb eine Schräglagen-Asymmetrie von 0,2° den Jet
  **31 m** von seiner eigenen Angriffsbahn ab, während die Schleife mit 0,2° Schräglage antwortete.
- Konsequenz: `SetDirectLeg` — gleicher Modus, gleiche Höhen-/Speed-Hälfte, die Bahn als DATEN vom
  Aufrufer. **Keine Bahn = keine Linie = das Peilungsgesetz, unverändert.**
- Genau deshalb umkreist ein Verteidiger, dem ein Punkt INNERHALB seines eigenen Kurvenradius befohlen
  wird, diesen Punkt für immer (`sim/missions/bfm-basic.fbm`) — gemessen: 58,9° Schräglage, 248 KCAS,
  4.000 m, 4,7 °/s, ~1.880 m Radius, konstant von t=20 s bis Missionsende.
- Ein zweiter Modus wäre falsch, weil er die Höhen-, Speed-, Capture- und Telemetriehälfte duplizieren
  und jeden Aufrufer, jeden Override und `FBMode` selbst ein drittes Wort für dasselbe Manöver lehren
  würde.

`kMinLegM = 100.0` m: kürzer als das ist eine „Bahn" ein Koordinatenpaar und keine Richtung — die
Peilung zwischen ihren Enden verliert ihre Bedeutung lange bevor das Flugzeug sie fliegen könnte.
`SetDirectLeg` fällt dann still auf das Peilungsgesetz zurück. Zwei Missionsfixes liegen Kilometer
auseinander; die Schranke fängt also nur eine degenerierte Deklaration.

### 2.4 Das Bahnfolge-Gesetz — vollständige Herleitung

**Struktur (`TrackBankCmd`, eine Definition, zwei Nutzer — Localizer und Route/Run-in-Leg):**

```
intercept = clamp(-k_xt * across, ±interceptMax)
dirErr    = wrap180(course + intercept - dir)
bank      = clamp(k_dir * dirErr, ±bankMax)
```

Kaskadiert statt summiert, mit Absicht: **der Cap begrenzt den Anfangswinkel, den ein großer Versatz
fordern darf**. Ein Jet zehn Kilometer neben seiner Bahn fliegt einen stetigen Winkel auf sie zu statt
einer momentanen Rechtwinkeldrehung. Für kleine Fehler IST die Kaskade die Zweizustands-Rückführung
`bank = −(k_xt·k_dir·y + k_dir·Δχ)` — und genau daraus sind die Wurzeln unten gerechnet.

**Die Strecke ist zweiter Ordnung.** Koordinierte Kurve, Kleinwinkel, `y` = Querablage, `Δχ` =
Bahnwinkel-Fehler, `φ` = Schräglage:

```
y'   = V · Δχ            (die Ablage wandert mit der Geschwindigkeit mal dem BAHN-Fehler)
Δχ'  = (g/V) · φ         (der Geschwindigkeitsvektor dreht mit der Schräglage)
```

Rückführung beider Zustände, `φ = −(k_y·y + k_χ·Δχ)`, schließt die Strecke als

```
y'' + (g·k_χ/V)·y' + (g·k_y)·y = 0
ωn = √(g·k_y)          ζ = k_χ·√(g/k_y) / (2V)
```

**Zwei Unbekannte → genau zwei Aussagen, nicht mehr:**

| # | Aussage | Begründung |
|---|---|---|
| (1) DÄMPFUNG | `ζ = 1/√2` | dieselbe Lehrbuchwurzel „einschwingen ohne Überschwingen", auf die auch die Kanonen-Nachführung geschlossen ist (`FBPilot::kBfmTrackKi`). Nichts an einem Streckenabschnitt spricht für eine andere. |
| (2) AUTORITÄT | der Querablage-Term allein erreicht die Schräglagen-GRENZE bei einem Versatz von genau EINEM Kurvenradius, `R = V²/(g·tan φmax)` | die einzige selbstbezügliche Aussage, die hier verfügbar ist: ein Versatz, den das Flugzeug innerhalb seiner engsten Kurve nicht schließen könnte, ist genau der Versatz, für den die engste Kurve die richtige Antwort ist — alles darüber ist gesättigt, weil es nichts mehr zu kommandieren gibt. |

**Auflösung.** Aus (2): `k_y = φmax / R(V) = φmax·g·tan(φmax) / V²`. Einsetzen in ζ liefert das
Bemerkenswerte — `k_χ` fällt UNABHÄNGIG von Geschwindigkeit UND Erdbeschleunigung heraus:

```
k_χ = 2·ζ·√(φmax · tan φmax)      [deg Schräglage je deg Bahnfehler]
k_y = φmax / R(V)                 [deg Schräglage je Meter]
```

Das ganze Gesetz ist damit durch `BankMaxDeg` festgelegt; **nur die Querablage-Hälfte ist gescheduled,
und zwar mit 1/V²** — genau das hält die Dämpfung bei 1/√2 über jede Geschwindigkeit, die derselbe Jet
fliegt. In der Kaskade ist die Querablage-Verstärkung `k_y/k_χ` und der Intercept-Cap `φmax/k_χ` (der
Winkel, bei dem der Querablage-Term allein bereits alles fordert, was die Schräglagengrenze hergibt) —
beide fallen aus denselben zwei Aussagen, statt gewählt zu werden.

**Zahlen bei `BankMaxDeg = 60°`** (φmax = 1,0472 rad; die Wurzel wird im BOGENMASS gebildet, weil sie
ein Winkelverhältnis ist, während `k_y` in deg/m ausgegeben wird):

| Größe | Wert |
|---|---|
| `k_χ` (= `kDir`) | 2·0,7071·√(1,0472·1,7321) = **1,905** deg/deg |
| Intercept-Cap | 60 / 1,905 = **31,5°** |
| `R` bei 85 m/s | 85² / (9,80665·1,7321) = **425 m** |
| `k_xt` bei 85 m/s | 60 / (1,905·425) = **0,074 deg/m** |
| `R` bei 231 m/s (450 kt) | **3.141 m** |

**Kreuzprobe — und der Grund, warum die Herleitung überhaupt geglaubt wird:** bei der geflogenen
F-16-Anfluggeschwindigkeit (165 KCAS ≈ 85 m/s) liefert der Schedule 0,074 deg/m gegen die 0,08 deg/m,
die der Localizer (`KXt`) seit Phase 3 von Hand fliegt, und 31,5° Intercept-Cap gegen dessen 45°. Das
sind **8 %** Abweichung von einer Verstärkung, die unabhängig gefunden wurde, auf einem Flugzeug, das
dreimal langsamer ist als das, an dem das Bahngesetz gemessen wurde — das ist die Aussage, dass der
Schedule das FLUGZEUG beschreibt und nicht einen Fall fittet. COURSE bleibt dennoch auf seinen eigenen
zwei Zahlen: das ist ein geflogener, gemessener Anflug, und diese Runde ging nicht um die Landung.

**Warum gegen den BODENKURS und nicht gegen die Nase** (`FBAutopilot.cpp`, Direct/Leg-Zweig):
die Querablage ist das Integral davon, wo das Flugzeug TATSÄCHLICH hinfliegt. Regelt man stattdessen
den Steuerkurs, bleibt jeder Schiebe- oder Vorhaltewinkel zwischen beiden als **permanente Driftrate**
stehen, die keine Querablage-Verstärkung je sehen kann. Der Bahnwinkel kommt daher aus dem
Geschwindigkeitsvektor: `trackDeg = atan2(vx, −vz)` (X-Plane-lokal: +x Ost, +z Süd, also Nord = −z).
Unterhalb `kLegMinSpeedMs = 30.0` m/s hat der Geschwindigkeitsvektor keine regelungswürdige Richtung
(Jet am Boden, Strömungsabriss) — dann liest das Gesetz die Nase (`s.yaw`). Diese Schwelle liegt weit
unter jeder Geschwindigkeit, bei der eine Bahn geflogen wird.

**Die statische Restablage — und warum kein Integrator.** Gegen eine KONSTANTE Querstörung (eine Zelle,
die 0,2° Schräglage braucht, um geradeaus zu fliegen — die des Vanilla-Modells) hält eine
Zweizustands-Rückführung eine bleibende Ablage von `φ_d / k_y`. Das ist Absicht:

| Argument | Zahl |
|---|---|
| Ein Integrator auf einer Bahn, die an jedem Wegpunkt geschaltet, neu verankert und verworfen wird, ist ein Wind-up-Problem | — |
| Restablage, die er entfernen würde, bei 450 kt | ~**10 m** (0,2° / 0,0191 deg/m) |
| Ablage, die das PEILUNGSGESETZ dort ohnehin stehen lässt | ~**30 m** |

Es ist also ein **begrenzter, geschwindigkeits-gescheduleter** Versatz statt eines unbegrenzten,
entfernungsgetriebenen.

`(void)along;` im Leg-Zweig ist eine Feststellung: die Längskoordinate ist Sache der SEQUENZIERUNG
(`FBNavSystem::AdvanceWaypoint`), nicht des Fliegens.

### 2.5 DIRECT — Höhen- und Peilungshälfte

| Element | Gesetz | Zahl |
|---|---|---|
| Peilung (ohne Bahn) | `bank = clamp(KHdg · wrap180(brg − yaw), ±BankMaxDeg)` | `KHdg` = 0,8 deg/deg |
| Höhe | `TargetVsMs = clamp(KAlt · AltErrM, ±25 m/s)` | `KAlt` = 0,08 (m/s)/m |
| `RingDistM` | `hypot(n,e)` — Diagnose, kein Regelanteil | — |

Der VS-Cap von 25 m/s ist enger, als eine Reiseflug-Höhenkorrektur bräuchte: DIRECT treibt auch den
Steigflug nach dem Abheben, wo der Fehler der GANZE Steigflug ist (Tausende Meter). Ungecappt hielte
die FLCS mit ihrem eigenen Alpha-Schedule das AoA zwar sicher, aber die resultierenden fast 30°
Anstellung sind ein unnötig aggressiver Steigwinkel für einen kontrollierten Climb-out.

### 2.6 COURSE — Localizer + Gleitpfad

Trackt die unendliche Linie durch `(refLat,refLon)` auf Rechtweisend `courseDeg` und sinkt dabei auf
einem geraden Gleitpfad `glidepathDeg` auf `refElevM` AM Referenzpunkt. Generisch darüber, was der
Referenzpunkt BEDEUTET — der Aufrufer (`FBPilot`) liefert Schwelle + Pistenrichtung.

| Element | Gesetz |
|---|---|
| Projektion | `FBTrackProjectM` → `along`/`across`; `distToGo = −along` (positiv VOR dem Referenzpunkt) |
| Lateral | `TrackBankCmd(CourseDeg, across, s.yaw, KXt, CourseInterceptMaxDeg, KHdg, BankMaxDeg)` |
| Zielhöhe | `RefElevM + tan(glidepath) · max(distToGo, 0)` |
| Vertikal | `TargetVsMs = clamp(−tan(gp)·gs + KAlt·AltErrM, ±ApproachVsCapMs)` |

**Das Feedforward und warum es existiert:** eine reine P-Korrektur auf den Höhenfehler hat gegen dieses
RAMPENDE Ziel einen bleibenden Schleppfehler (klassischer Typ-1-Servo-Lag, `e_ss = Zielrate / KAlt`).
**Gemessen: ~56 m zu hoch an der Schwelle auf einem 9-nm-Final** beim Default-`KAlt` — also Aufsetzen
tief in der Piste statt nahe der Schwelle. Das Feedforward der Zielsinkrate (`tan(glidepath) ×
Annäherungsgeschwindigkeit`) lässt dem P-Term nur noch die ABWEICHUNG vom Leitstrahl, nicht das
Nachfahren seiner Steigung.

Hinter dem Referenzpunkt (`distToGo ≤ 0`, z. B. wenn `FBPilot` längst an Flare übergeben hat und ein
`Run()` noch hier landet) hält das Ziel schlicht `RefElevM`, statt darunter zu tauchen.

Die Projektionskonvention (`along=0` am Referenzpunkt, `+across` = rechts vom Kurs) ist DIESELBE wie in
`FBPilot`s Centerline-Gesetz und `FBMissionMonitor::OnRunway` — alle drei sind sich darüber einig, was
„auf der Linie" heißt.

### 2.7 Gains (public Config-Block, Defaults = geflogenes F-16-Preset)

| Feld | Default | Bedeutung |
|---|---|---|
| `BankMaxDeg` | 60 | Schräglagengrenze; **legt in DIRECT-Leg das gesamte Gesetz fest** |
| `KHdg` | 0,8 | Richtungsfehler → Schräglage (DIRECT-Peilung und COURSE; in DIRECT-Leg ERSETZT durch das hergeleitete `k_χ`) |
| `KAlt` | 0,08 | Höhenfehler → Vertikalgeschwindigkeit |
| `KXt` | 0,08 | COURSE: Querablage (m) → Intercept-Winkel-Offset (deg) |
| `CourseInterceptMaxDeg` | 45 | COURSE: Cap dieses Winkels |
| `ApproachVsCapMs` | 8 | COURSE-VS-Cap (ein geflogener Gleitpfad, kein Sturz) |
| `kMinLegM` | 100 (constexpr) | Mindestlänge einer Bahn |
| `kLegZeta` | 1/√2 (Datei-Konstante) | Dämpfung des Bahngesetzes |
| `kLegG0` | 9,80665 | g |
| `kLegMinSpeedMs` | 30 | unterhalb: Nase statt Bahnwinkel |

**`Run()` ist der EINE Override-Punkt.** Ein Modul, dessen Guidance sich wirklich anders verhält (nicht
nur andere Gains), leitet ab und überschreibt `Run()`; `FBF16Module` komponiert diesen Default
UNVERÄNDERT. Konfigurationsunterschiede (F-16-Gains) bleiben Daten auf dieser Klasse, keine Subklasse.

---

## 3. `FBFlightControl` — die FBW-Innenschleife

`systems/FBFlightControl.h`, `systems/FBFlightControl.cpp`. Konsumiert `FBGuidance` + JSBSim-Zustand,
emittiert normierte Stick-/Throttle-Werte (`FBControls{Roll,Pitch,Yaw,Thr}`). Portiert aus
`flightctl.h`s innerer Schleife, Numerik verbatim.

### 3.1 Zwei innere Arten, EIN Flag

`Flcs` ist **Konfiguration, keine Subklasse** — es ist Tuning, kein Verhalten:

| `Flcs` | Art | Vorgehen |
|---|---|---|
| 1 | **FLCS** | eine echte FLCS wie ein Pilot kommandieren (F-16): Schräglagenfehler → Rollraten-Stick, g-Kommando-PI mit 1/cos(bank)-Kurven-Feedforward + VS-Fehler-Bias/Integral → Pitch-Stick, Quer-g-Nullung → Pedale, PI-Throttle. **Die FLCS der Zelle ist der Stabilisator; wir kommandieren sie.** |
| 0 | **Raw** | Attitude-Hold-PD direkt auf die Ruder (Modelle OHNE FLCS, der c172-Pfad) |

### 3.2 Verhältnis zur echten FLCS und `fcs/fbw-override`

- Die vanilla JSBSim-F-16 ist eine ECHTE FLCS: `fcs/*-cmd-norm` sind **Raten-Sollwerte**, keine
  Ruderausschläge.
- `FBFdmSpawn::FbwOverride` (`fdm/FBFdmBoot.h`) setzt beim Spawn `fcs/fbw-override = 1.0`
  (`fdm/FBFdm.cpp`): damit ist FlightBox' eigene FCS der Regler statt der modelleigenen
  Flugsteuerung — direktes Ruder, sonst zwei geschachtelte Ratenschleifen.
- Der Weg in die Physik ist ausschließlich `FBFdm::SetControls(roll,pitch,yaw,thr)`:
  - `roll → fcs/aileron-cmd-norm`
  - `pitch → fcs/elevator-cmd-norm` als **`−pitch + ElevTrim`** (JSBSim: +Elevator = Nase RUNTER;
    FlightBox: +Pitch = Nase HOCH; `ElevTrim` ist das vom Trimmlauf gefundene Höhenruder = die
    Trimmklappe, die bei neutralem Stick LEVEL hält statt die nasenhohe Ruhelage der Zelle)
  - `yaw → fcs/rudder-cmd-norm` (+yaw koordiniert die Kurve; −yaw schiebt sie — gemessen, starker
    negativer Wendemoment)
  - `thr → fcs/throttle-cmd-norm`, **slew-limitiert** (ein Sprung 0 → 0,95 sprengt die Drehzahl-ODE des
    Triebwerks und lässt die Zelle abkippen)
  - Kampfschaden greift GENAU HIER an: `roll/pitch/yaw *= Authority`, `thr` gedeckelt — die FCS
    kommandiert unverändert, das Flugzeug antwortet nicht mehr.

### 3.3 Das FLCS-Gesetz, Schritt für Schritt (alles bei festen 0,01 s)

```
bc      = min(|roll|, 80°) → rad
vsErr   = TargetVsMs − vy
VsIterm = clamp(VsIterm + KVsi·vsErr·0.01, ±0.5)
nzRaw   = 1/cos(bc) + KVs2g·vsErr + VsIterm
nzCmd   = clamp(nzRaw, NzPrev ± NzSlew·0.01)      // Slew-Limit auf das g-KOMMANDO
gErr    = nzCmd − nz
bankErr = |BankCmdDeg − roll|
blend   = clamp(1 − (bankErr − 5)/15, 0, 1)       // 1 bei ≤5°, 0 ab 20°
GIterm  = clamp(GIterm + blend·KGi·gErr·0.01, ±1)
Roll    = clamp(KRollRate·(BankCmdDeg − roll), ±RollStickMax)
Pitch   = blend · clamp(KG·gErr + GIterm, ±1)
NyIterm = clamp(NyIterm + KNyi·ny·0.01, ±0.6)
Yaw     = clamp(KNy·ny + NyIterm, ±1)
```

Die vier Konstruktionsentscheidungen, jede gegen das nackte Modell gemessen:

| Element | Begründung |
|---|---|
| Kurven-g-Feedforward aus der TATSÄCHLICHEN Schräglage | das g, das eine Kurve JETZT braucht, hängt an der Schräglage JETZT |
| VS-Fehler-INTEGRAL | bricht die falschen Höhe/g-Gleichgewichte auf |
| Slew-limitiertes g-Kommando | `NzSlew` = 1,5 g/s |
| g-Stick erst EINGEBLENDET, wenn die Schräglage nahezu steht | ein Pilot rollt mit neutralem Pitch ein; treibt man die g-Schleife mitten in der Rolle, stapelt sie sich auf die eigene g-PID der FLCS → Einflugspitzen |

**Gierpedale:** nullen den Querlastfaktor. Der Gierdämpfer des Modells (rohe r-Rückführung, kein
Washout) hält Ruder gegen eine stationäre Kurve; ein Rest-`ny` von etwa **−0,10 g** bleibt
**modell-intrinsisch** — akzeptierte Modell-Eigenschaft, kein Defekt (Prinzip 5).

**Throttle (in beiden Arten):**
`ThrIterm = clamp(ThrIterm + KTi·spdErr·0.01, −ThrTrim, 1−ThrTrim)` — der Integratorbereich ist der
PHYSIKALISCHE Weg von der Trimmstellung zu beiden Anschlägen. `Thr = clamp(ThrTrim + KpSpd·spdErr +
ThrIterm, 0, 1)`.

**Raw-Zweig (FLCS-lose Modelle, `flightctl.h`s `fc_update` verbatim):**
`pitchCmd = clamp(KAltRaw·AltErrM, ±PitchMaxDeg)`; `Roll = clamp(KpRoll·(BankCmd−roll) − KdRoll·p, ±1)`;
`Pitch = −clamp(KpPitch·(pitchCmd−pitch) − KdPitch·q, ±1)`; `Yaw = clamp(−KdYaw·r + KCoord·BankCmd, ±1)`.

**MANUAL** wird ganz vorn durchgereicht: `o = {ManualRoll, ManualPitch, ManualYaw, ManualThr}`, keine
Integratorbewegung, keine Schleife.

### 3.4 Gains: Default vs. `F16()`-Preset

| Feld | Default (Raw/c172-Ära) | `F16()` | Einheit/Bedeutung |
|---|---|---|---|
| `Flcs` | 0 | **1** | Innenschleifen-Art |
| `RollStickMax` | 1,0 | **0,15** | Cap des Roll-Sticks („sanftes Einrollen") |
| `KRollRate` | 0,06 | **0,05** | Schräglagenfehler → Roll-Stick |
| `KG` | 0,4 | **0,25** | g-Fehler → Pitch-Stick (P) |
| `KGi` | 2,0 | **0,8** | g-Fehler → Pitch-Stick (I) |
| `KVs2g` | 0,05 | 0,05 | VS-Fehler → g-Kommando |
| `KVsi` | 0,02 | 0,02 | VS-Fehler-Integral |
| `KNy`/`KNyi` | 1,5 / 2,0 | 1,5 / 2,0 | Quer-g → Pedale (P/I) |
| `KTi` | 0,002 | 0,002 | Speed-Integral |
| `KpSpd` | 0,03 | **0,02** | Speed-Fehler → Throttle |
| `ThrTrim` | 0,55 | **0,85** | Throttle-Trimmpunkt |
| `NzSlew` | 1,5 | 1,5 | max. g-Kommandoänderung je Sekunde |
| `KpRoll`/`KdRoll` | 0,022 / 0,004 | (ungenutzt) | Raw-Zweig |
| `KpPitch`/`KdPitch` | 0,06 / 0,010 | (ungenutzt) | Raw-Zweig |
| `KdYaw`/`KCoord` | 0,006 / 0,004 | (ungenutzt) | Raw-Zweig |
| `PitchMaxDeg`/`KAltRaw` | 15 / 0,05 | (ungenutzt) | Raw-Zweig |

**`RollStickMax = 0,15` ist eine Messung:** damit bleibt `nz` im Bereich **0,7…1,9 g**; bei 0,35 waren
es **−1,1…+3,0 g**.

### 3.5 Nebenpflichten

- **`FBTelemetrySource "flightcontrol"`** — Spalten `rollCmd`, `pitchCmd`, `yawCmd`, `throttleNorm`.
  `Run()` cached sein Ergebnis in `LastControls_`; bei 100 Hz liest der Telemetriebus (10 Hz) immer das
  jüngste.
- **`Reset()`** nullt alle Integratoren (`GIterm`, `VsIterm`, `NyIterm`, `ThrIterm`, `NzPrev`) — neuer
  Flug.
- **`GetGIterm()`/`GetVsIterm()`** sind Diagnose-Fenster in die Integratoren.

---

## 4. `FBAirDataSystem` — Luftdaten (ADC-Klasse)

`systems/FBAirDataSystem.h/.cpp`. Schreibt `FBState::AirData`. Airframe-agnostisch: jedes Modul mit
einem ADC und einem Geschwindigkeitsvektor bekommt dieselben Zahlen; eine Zelle, deren Luftdatenkette
sich wirklich unterscheidet, überschreibt `Run()`.

| Blockfeld | Quelle | Bemerkung |
|---|---|---|
| `CasKt` | `fdm.cas · kMsToKt` | kalibrierte Fahrt |
| `Mach` | `fdm.mach` | direkt |
| `GLoad` | `fdm.nz` | Körper-Normallastfaktor |
| `GLoadPeak` | laufendes Maximum seit Boot | wird nie zurückgesetzt (HUD-Peak-G) |
| `TrackDeg` | `atan2(vx, −vz)`, auf 0…360 gebracht | Bodenkurs aus dem Geschwindigkeitsvektor |
| `FpaDeg` | `atan2(vy, max(horiz, 0.01))` | Bahnneigungswinkel, + = steigend |

**FPM-Richtung als WELT-Azimut/Elevation, nicht als körperrelativer Versatz:** `FBF16Hud` projiziert
sie durch DIESELBE Kamerabasis (yaw/pitch/roll), die der konforme Horizont schon benutzt — die Lage
muss also nicht zweimal komponiert werden.

`vx/vy/vz` sind X-Plane-lokal (+x Ost, +y auf, +z Süd) — bereits das lokale ENU-Frame, keine Geodäsie
nötig. `horiz = √(vx²+vz²)`.

Telemetrie-Quelle `"airdata"`: `casKt`, `mach`, `nz`, `aoaDeg`. Die Klasse cached ihre eigenen letzten
Werte, weil `SampleTelemetry` kein `FBState` in der Signatur hat (Kern-Architekturregel: eine Quelle
sampelt ihr EIGENES letztes Ergebnis). `aoaDeg` steht in der Telemetrie, aber NICHT im Block.

`dt` wird ignoriert (`(void)dt`) — das System ist zustandsfrei bis auf `PeakG`.

---

## 5. `FBRadarAltimeter` — der Referenzfall für `Invalid`

`systems/FBRadarAltimeter.h/.cpp`. Schreibt `FBState::RadarAlt`.

### 5.1 Vertrag

- **Es fragt das Terrain NICHT selbst ab.** Es konvertiert das vom Client bereits aufgelöste Paar
  `(elevAslM, groundAslM)` in Metern ASL in die Radarhöhe in Fuß — dasselbe DEM-Sample, das die App
  ohnehin für `FBRenderer::SetAgl` holt. Keine zweite Terrainabfrage.
- `AglFt = (elevAslM − groundAslM) · 3,280839895`.
- Ein Schalter: `SetPowered(bool)`, Default `true`.

### 5.2 Was er lehrt

> Die Box ist eine BESTROMTE Box, und `doc/f16/controls-commands.md` §6.4 dokumentiert die Folge
> wörtlich: die CARA-ALOW-Warnung feuert **nur** bei bestromtem und sendendem Radarhöhenmesser — egal
> wie bereitwillig das DED die Schwelle angenommen hat.

Daraus die Regel, die für den ganzen Bus gilt:

| Fall | Verhalten |
|---|---|
| bestromt | `AglFt` neu gerechnet, `H.Publish(NowS)` → `Valid` |
| stromlos | `H.Invalidate()` → `Invalid`, **die letzte Zahl bleibt stehen** |

Stromlos publiziert die Box **nicht** „0 ft" und **nicht** einen alten Wert als frisch. Sie macht ihren
Block ungültig, und **jeder Konsument muss dann sagen, was er ohne sie tut** — der AGL-Boden des
Piloten, das R-Feld des HUD, das Warnsystem. Das ist der Gewinn des Dreizustandskopfs; eine Box, die
stattdessen 0 ft publiziert, flöge den Jet in den Boden.

Dass die letzte Zahl im Feld stehen BLEIBT, ist ebenfalls Absicht: ein Konsument, der den Kopf
ignoriert, darf nicht still eine frisch aussehende Null bekommen.

---

## 6. `FBWarningSystem` — der Warnsatz als Bitmaske

`systems/FBWarningSystem.h/.cpp`. Schreibt `FBState::Warnings` und **nichts sonst**; liest
Radarhöhe, die vom UFC committeten Schwellen und den Airframe-Block.

> Es kommandiert nichts — ein Warnsystem, das handeln könnte, wäre ein zweiter Pilot.

### 6.1 Warum es existiert: Gültigkeitsköpfe konsequent machen

Jede Bedingung hier ist eine Fusion von Blöcken, die anderswo geschrieben wurden. Jede kann daher
**UNAUSWERTBAR** sein — und das ist eine dritte Antwort, verschieden von „Warnung" und „keine Warnung".
Der Block trägt sie getrennt:

```
struct FBWarningBlock { FBBlockHeader H; uint32_t Active; uint32_t Inhibited; };
```

Ein `Invalid`er Radarhöhenblock heißt **nicht** „nicht tief". Er heißt: niemand kann es sagen — und der
Annunciator sagt genau das.

### 6.2 Die drei Bits (`core/FBAvionicsBlocks.h::FBWarningBit`)

| Bit | Wert | Aktiv wenn | Inhibiert wenn | Gar nicht ausgewertet wenn |
|---|---|---|---|---|
| `FBWarnAlow` | 1<<0 | `RadarAlt.AglFt < Ufc.AlowFt` | `Ufc` lesbar & `AlowFt > 0` & `RadarAlt` NICHT lesbar | `Ufc` unlesbar oder `AlowFt == 0` (keine Schwelle eingegeben = nichts zu warnen, **kein** Inhibit) |
| `FBWarnBingo` | 1<<1 | `Airframe.FuelLbs ≤ Ufc.BingoEffectiveLbs` | `Ufc` lesbar & `BingoEffectiveLbs > 0` & `Airframe` NICHT lesbar | `Ufc` unlesbar oder Schwelle 0 |
| `FBWarnGearUnsafe` | 1<<2 | `Airframe.WeightOnWheels && GearPosition < 0.99` | `Airframe` NICHT lesbar | — |

Feinheiten, die im Code begründet sind:

- **BINGO gegen die EFFEKTIVE Schwelle, nicht gegen die eingegebene.** Der Jet führt zwei Zahlen
  (`doc/f16/controls-commands.md` §6.8): das DED-Feld zeigt, was der Pilot TIPPTE (`BingoLbs`), die
  Warnung feuert an der Systemobergrenze (`BingoEffectiveLbs`). Anzeigen lesen die erste, das
  Warnsystem die zweite — Zusammenlegen hätte die dokumentierte Klemmung unsichtbar gemacht.
- **Gear ist der Kontrollfall.** Es ist die einzige Bedingung, deren Eingaben aus EINEM Block kommen —
  damit ist es die Referenz gegen die zwei Fusionen darüber.
- `Readable()` = `Valid` ODER `Held`. Ein absichtlich eingefrorener Block wird also gelesen; nur
  `Invalid` inhibiert.

Telemetrie-Quelle `"warn"`: `warn_active`, `warn_inhibited` (beide als Integer-Bitmasken).

---

## 7. `FBNavSystem` — Steerpoint + Bullseye + Sequenzierung

`systems/FBNavSystem.h/.cpp`. Schreibt `FBState::Nav` UND `FBState::Cruise` (ein Quellsystem darf mehr
als eine Nachricht publizieren).

### 7.1 Umfang und Setzung

Ein aktiver Steerpoint plus Bullseye-Referenz. Die echte Hardware kennt eine Steerpoint-Datenbank
(26…30 Markpoints plus die Missions-Steerpoints, `doc/f16/navigation-ils.md`) — **dies ist der
Ein-Punkt-Platzhalter, mit dem jedes Modul startet.**

| Setter | Bedeutung |
|---|---|
| `SetSteerpoint(lat, lon, elevFt)` | `elevFt` = die EIGENE Bodenhöhe des Steerpoints (Eingang für `FBF16FireControl`s Slant-Range) |
| `SetBullseye(lat, lon)` | im F-16-Modul: die Piste der Mission (`FBF16Module::SetRunway`) — eine `.fbm` deklariert kein Bullseye, und die Piste ist der eine gebriefte geografische Punkt, den alle Einheiten teilen |

### 7.2 Geodäsie

**Dieselbe planare ENU-Näherung wie überall sonst im Baum** (`core/FBGeodesy.h`, die EINE Definition):
`nord = Δlat · 111320 m/deg`, `ost = wrap180(Δlon) · 111320 · cos(ref_lat)`. Steerpoints liegen zehner
von nm entfernt, nicht interkontinental — der Flacherde-Fehler ist vernachlässigbar, und es bleibt
konsistent mit dem Rest des Codes statt eine zweite Geodäsie-Konvention einzuführen. Die Wrap-Behandlung
ist Teil des Primitivs: unwrapped las eine 360°-Differenz am Antimeridian ~38.000 km für einen Punkt
einen Meter weiter.

Konvention: **der Referenzpunkt kommt ZUERST und besitzt den Kosinus.**

### 7.3 Was publiziert wird

| Feld | Rechnung |
|---|---|
| `Nav.SteerBearingDeg` | Peilung Flugzeug → Steerpoint, 0…360 |
| `Nav.SteerElevAngleDeg` | `atan2(StElevFt·kFtToM − fdm.elev, max(dist,1))` |
| `Nav.SteerDistNm` | Horizontaldistanz |
| `Nav.SteerElevFt` | durchgereicht |
| `Nav.BullBearingDeg` / `BullDistNm` | Peilung/Distanz **VOM Bullseye ZUM Flugzeug** |
| `Nav.MagVarDeg` | **0,0 — Platzhalter, kein Deklinationsmodell** |
| `Cruise.SteerTtgS` | `dist / max(gs, 1)` |

`Nav.H.Publish` nur, wenn Steerpoint ODER Bullseye gesetzt ist.

### 7.4 Der `Held`-Fall: die CRUS-Seite bei ausgefahrenem Fahrwerk

Der echte Jet FRIERT die berechneten CRUS-Felder bei ausgefahrenem Fahrwerk EIN, statt sie zu leeren
(`doc/f16/controls-commands.md`, CRUS-Tabelle). Weil das eine Eigenschaft der NACHRICHT ist, hat sie
einen eigenen Kopf — deshalb ist `Cruise` ein eigener Block:

```
gearDown = Airframe.H.Readable() && Airframe.GearPosition > 0.5
gearDown ? Cruise.H.Hold() : (Cruise.SteerTtgS = …, Cruise.H.Publish(NowS))
```

`Hold()` bewegt den Zeitstempel NICHT — genau das macht „wie alt ist diese eingefrorene Zahl"
beantwortbar. Bearing/Distance im Nav-Block laufen dabei ungerührt weiter.

### 7.5 `AdvanceWaypoint` — Sequenzierung als AKTEURS-Verhalten

```
int AdvanceWaypoint(FBFlightPlan &plan, double lat, double lon, double captureM = 500.0)
```

**Wer ruft:** das MODUL selbst, im Piloten-Takt (10 Hz), direkt nach der Entscheidung
(`FBF16Module::Run`). **Nicht** der Missions-Orchestrator — dieses System kennt Steerpoints und
Distanzen bereits, also gehört die Sequenzierung hierher und nicht in die Runner-Buchhaltung. Der
Runner-Richter (`core/FBMissionMonitor`) urteilt unabhängig davon auf seiner eigenen, unveränderlichen
Plankopie.

**Zwei Erfüllungsgründe:**

| Grund | Test | Gilt ab |
|---|---|---|
| `capture` | `FBPlanarDistM(Flugzeug, wp) ≤ captureM` (Default 500 m) | jedem Wegpunkt |
| `passed` | Projektion auf die Bahn `wp[idx−1] → wp[idx]`: `alongM ≥ legM` | **erst ab `idx > 0`** |

**Warum ein Fangkreis allein nicht reicht:** ein Fangkreis kann einen Wegpunkt nicht beantworten, an dem
das Flugzeug PHYSISCH nicht ankommen kann — einen, der innerhalb seines eigenen Kurvenradius liegt und
den es außen für immer umkreist. (`bfm-basic.fbm` benutzt genau das absichtlich, um einen Verteidiger
zum Kurven zu bringen.) Die Antwort ist **nicht ein größerer Kreis**, sondern die Achse, die die Bahn
ohnehin definiert: **hinter der Senkrechten durch den Fix ist der Fix passiert**, wie groß der Fehlabstand
auch war.

**Warum das den absichtlichen Dauerkreis der BFM-Verteidiger nicht zerstört:** die Regel existiert
GENAU DORT, wo die BAHN existiert — dieselben zwei deklarierten Fixe, deren Track `FBPilot` fliegt. Ein
ERSTER Wegpunkt hat keine einlaufende Bahn, also gibt es kein „hinter" zu messen, und der Punkt wird
verfolgt wie zuvor. In `bfm-basic.fbm` hat der Verteidiger genau EINE `wp`-Zeile (Index 0): kein
Vorgänger, keine Bahn, keine `passed`-Prüfung, der Orbit bleibt.

**Damit stimmen Sequenzierung und Guidance konstruktiv überein:** das Fliegen hält nur dort eine Linie
(`SetDirectLeg`), wo die Buchführung erkennen kann, dass die Linie geendet hat.

Rückgabe: der gerade erreichte Planindex oder −1. Logzeile: `FBLog::Info("nav", "WP_REACHED", {idx, lat,
lon, by=capture|passed})`.

---

## 8. `FBDisplaySystem` — das generische Default-HUD

`systems/FBDisplaySystem.h`, `systems/FBDisplaySystem.cpp`. Erster Slot mit ECHTEM statt NoOp-Default —
deshalb aus `FBSystemSlots.h` herausgewachsen.

### 8.1 Zwei getrennte Einstiegspunkte

| Methode | Takt | Aufrufer | Zweck |
|---|---|---|---|
| `Run(const FBState&, FBMasterMode, dt)` | 20 Hz (Modul) | `FBF16Module::Run` | periodische Anzeigenlogik (MFD-Seiten, Warnlampen) — Default leer |
| `BuildHud(const FBState&, const FBHudEnv&, FBHudGeometry&) const` | 1× pro gerendertem Frame | `render/stages/FBHudStage::Encode` | die ganze Symbologie neu erzeugen |

`BuildHud` ist `const`: es LIEST Zustand, es besitzt keinen.

### 8.2 Arbeitsteilung mit `render/`

```
systems/FBDisplaySystem::BuildHud   → LOGIK/Symbologie, füllt …
render/FBHudGeometry                → den wiederverwendeten 2D-Geometriepuffer (Pixelkoordinaten)
render/stages/FBHudStage            → das reine WebGPU-Backend: lädt die Vertexströme VERBATIM hoch
```

- `FBHudStage` hält die Display-System-Referenz nur GEBORGT (`SetDisplaySystem`, `nullptr` = leeres
  HUD); der Client verdrahtet sie aus dem aktiven Modul (`R.SetHudDisplay(&module.Displays())`).
- `FBHudStage` cached seinerseits eine `FBState`-Kopie (`SetState`) plus `Agl`, baut daraus
  `FBHudEnv{Width, Height, Agl, Have}` und ruft `BuildHud` — pro Frame, nach `Geometry.Reset()`.
- `FBHudGeometry` kennt zwei Primitive: **Strokes** (`x,y,d,hw,r,g,b`, 6 Vertices je Segment, analytische
  Coverage-AA) und **Glyphs** (`x,y,u,v,r,g,b`, 6 je Zeichen, Bitmap-Font-Atlas). Vektoren statt fester
  Arrays: `Reset()` leert ohne Kapazität freizugeben, nach dem ersten Frame allokiert nichts mehr.
- `FBHudGeometry::SetClip/ClearClip` ist die Scissor-Klammer für konforme Symbologie (Liang-Barsky für
  Strokes, Ganz-oder-gar-nicht für Glyphs) — vom generischen Default NICHT benutzt, wohl aber von
  `FBF16Hud`s Combiner-Apertur.
- `FBHudEnv` trägt Viewport/AGL/Have-Telemetry, weil das Render-/Telemetrie-Verdrahtung ist und kein
  Sim-Zustand: es reist getrennt, statt `FBState` für einen Konsumenten aufzublähen.

### 8.3 Was das Default-HUD zeichnet (MIL-STD-1787-artig)

Geometrie/Positionen verbatim aus dem stillgelegten `FBHudSymbology.h::w3_build_hud` portiert
(Äquivalenzabsicht: gleiches Pixel-Layout für die behaltenen Elemente).

| Element | Details |
|---|---|
| Farbe | monochromes HUD-Grün `(0.30, 1.00, 0.40)` |
| Waterline/Boresight | feste Zellenreferenz, bildschirmfest, zwei Balken ±10…28 px + V-Spitze 7 px tief |
| `NO TELEMETRY` | wenn `env.Have == false`: nur Waterline + roter Text, `return` |
| Konformer Horizont | zwei Segmente neben der Waterline (Lücke ±36 px, Ende ±86 px), gekippt durch DIESELBE Kameraprojektion wie die Szene: `w3_cam_from(yaw,pitch,roll, FOV 80°)`, Mitte = Horizontpunkt geradeaus (az=yaw), zweiter Punkt bei +20° liefert die Neigung |
| Horizont-DIP | aus `Platform.AltM` (ASL/Krümmungsreferenz), **nicht** aus AGL — mit AGL atmete der Horizont bei Geländerelief im Level-Loiter |
| Heading-Tape (oben) | 5 px/deg, Ticks alle 5°, Labels alle 30° (`N/03/06/E/…`), Schiene ±200 px, Fenster ±45°, Kastenwert `%03.0f` + Up-Caret |
| Steerpoint-Marker | Dreieck + `SP` am Offset `Platform.HomeBearingDeg` (nasenrelativ), auf ±44° geklemmt |
| GS-Tape (links, x=70) | 5 px je Einheit **m/s**, Ticks alle 5, Labels alle 10, Schiene ±150 px, Kastenwert + Caret, Label `GS` |
| ALT-Tape (rechts, x=W−70) | 1,5 px je **Meter**, Ticks alle 10 m, Labels alle 20 m, Schiene ±150 px, Kastenwert + `<`-Caret, Label `ASL` |
| AGL/VS | unter dem ALT-Kasten: `AGL%4.0f` (aus `env.Agl`), `VS%+4.0f` (aus `Platform.VsMs`) |

Alle Werte kommen aus `state.Platform` — der Default liest KEINEN anderen Block (kein AirData, kein Nav,
kein RadarAlt). Er ist in SI beschriftet (m/s, m), nicht in kt/ft; die musterspezifischen Einheiten und
Formate sind Sache des Overrides.

### 8.4 Der Override

`FBF16Module` komponiert **nicht** diesen Default, sondern `modules/f16/displays/FBF16Hud` (die echte
F-16-Symbologie). Der generische Default bleibt der Fallback für Clients ohne lebendes Modul — z. B.
`FBAppNative.cpp`s No-Module-Screenshot-Modus (`static FBDisplaySystem hudDisplay;`).

---

## 9. `FBAirframeControls` — was die Hände des Piloten anfassen

`systems/FBAirframeControls.h/.cpp`. Interface + NoOp-Default in EINER Klasse (das
`FBSystemSlots.h`-Muster), plus die reale Ownship-Implementierung daneben.

### 9.1 Der Vertrag

Alles, was ein Pilot jenseits von Stick/Throttle (`FBFlightControl`) und Guidance-Zielen
(`FBAutopilot`) bedient:

| Kommando | Wertebereich | JSBSim-Property (via `FBFdm`) |
|---|---|---|
| `SetGear(bool down)` | — | `gear/gear-cmd-norm` (0/1; das Modell fährt kinematisch) |
| `SetSpeedbrake(double)` | 0…1 | `fcs/speedbrake-cmd-norm` |
| `SetWheelBrakes(l, r)` | je 0…1 | `fcs/left-brake-cmd-norm`, `fcs/right-brake-cmd-norm` |
| `SetNosewheelSteer(double)` | −1…1 | `fcs/steer-cmd-norm` |
| `EngineStart()` / `EngineCutoff()` | — | `FGPropulsion::SetStarter`/`SetCutoff` (alle Triebwerke) |

| Rückmeldung | Bedeutung |
|---|---|
| `GetWeightOnWheels()` | modellweites WOW — eine Ja/Nein-Frage, keine Aufschlüsselung je Fahrwerk (die gehört einem künftigen Fahrwerkssystem) |
| `GetGearPosition()` | 0 = ein … 1 = aus, kinematisch verzögert |
| `GetSpeedbrake()` | 0…1, verzögerter Readback |
| `GetGrossWeightLbs()` | Live-Abfluggewicht — Eingang für `FBPilot`s Rotationsgeschwindigkeits-Tabelle |
| `GetEngineRunning(int idx)` | läuft/läuft nicht |

NoOp-Default: alle Setter tun nichts, `GetWeightOnWheels()` liefert `false`, alle Zahlen 0.

### 9.2 `FBJsbsimAirframeControls` — die reale Ownship-Implementierung

- **Jeder Setter reicht direkt an EINE geborgte `FBFdm` weiter, jeder Getter liest DIESELBE Property
  zurück.** Kein Schattenzustand: eine kinematische Fahrwerksfahrt oder ein WOW-Umschlag ist in dem
  Moment sichtbar, in dem das FDM ihn meldet.
- Die `FBFdm&` ist **konstruktor-injiziert und nie null** — die Zuordnung dieses Objekts zu EINEM
  Airframe steht für seine ganze Lebensdauer fest. Genau deshalb ist jede Methode ein reiner Forward
  ohne „gibt es ein FDM"-Zweig.
- Es ist **airframe-agnostisch**: die benutzten `FBFdm`-Methoden sind generische FGFCS-/
  FGGroundReactions-/FGPropulsion-Anbindungen, also kann jedes JSBSim-geflogene Modul es
  wiederverwenden, nicht nur die F-16.
- Verdrahtet wird es in `FBModule::AttachFdm`: das Modul TAUSCHT den NoOp-Default gegen die reale
  Instanz aus (`AirframeCtrl = std::make_unique<FBJsbsimAirframeControls>(fdm);`).

### 9.3 Warum der Pilot die Zelle NUR hierüber liest

Aus `FBPilot.h`s Signatur-Banner:

> Der Pilot fasst nie ein FDM an — er kennt keines und kann keines erreichen. Genau das hält diese
> generische Schicht **airframe-agnostisch UND instanz-agnostisch** (Multi-Unit).

- Ein Pilot, der an dieser Schnittstelle vorbei ins FDM griffe, wäre an EINE konkrete
  FDM-Implementierung und an EINE Instanz davon gebunden.
- Das Handle reist **pro Tick** mit dem Rest der wahrgenommenen Welt (`const FBAirframeControls&`,
  `st`, `plan`, `runway`), statt bei der Konstruktion gebunden zu werden — ein Modul komponiert seinen
  Piloten lange bevor ein Airframe existiert.
- Abgrenzung, präzise: **Zellen-ZUSTAND** (Fahrwerk, WOW, Gewicht, Triebwerk) kommt ausschließlich über
  dieses Interface; **Pose/Fahrt** liest der Pilot aus dem `const fb_fdm_state& st` derselben Signatur;
  **Avionik** ausschließlich über den Kommandobus. Ein `const FBWorld*` stand früher in der Signatur,
  wurde nie gelesen und ist entfernt worden: ein Pfad zur Grundwahrheit, der darauf wartete, benutzt zu
  werden.
- Der Rückweg ist derselbe Kanal: `FBPilotCommands` trägt `std::optional`-Felder, und das Modul ruft
  den passenden Setter NUR, wenn ein Feld gesetzt ist — „nicht gesetzt" heißt „der Pilot fasst diese
  Bedienung gerade nicht an", was die meisten Ticks für die meisten Bedienungen zutrifft.

### 9.4 Telemetrie

`FBAirframeControls` ist selbst `FBTelemetrySource` (`"airframe"`) — über DIESELBEN virtuellen Getter,
die jeder Aufrufer benutzt. Deshalb funktioniert sie unverändert für den NoOp-Default UND für
`FBJsbsimAirframeControls`; keiner von beiden überschreibt Telemetrie. Spalten: `gearPos`, `wow`,
`speedbrake`.

---

## 10. Die noch leeren Slots

`systems/FBSystemSlots.h`. „NoOp-Default" heißt hier: die Klasse existiert, ist instanziierbar, wird
vom Modul besessen und im Takt aufgerufen — ihr `Run()` ist leer.

| Klasse | Signatur | Was hineingehört |
|---|---|---|
| `FBInputSystem` | `Run(FBMasterMode mode, double dt)` | HOTAS (SSC+TQS) + ICP: routet Stick-/Schalterereignisse nach dem aktiven Master-Mode. **Input-Routing ist Modul-Autorität, nicht global.** NoOp, bis ein Modul echten Input bindet. |
| `FBPropulsionSystem` | `Run(const fb_fdm_state &s, double dt)` | Triebwerks-SYSTEM-Logik ÜBER dem rohen FDM (F110+DEEC-Zustand, BINGO/JOKER-Rufe, EPU). **JSBSims eigenes Antriebsmodell treibt den Schub bereits** — hier legt sich das Triebwerksmanagement darüber. |
| `FBWeaponSystem` | `Run(FBMasterMode, const FBWorld*, double dt)` | historischer Stub für SMS/CCIP/CCRP/Gun; **überholt** durch `FBStoresSystem`/`FBGunSystem` (s. Offene Punkte) |

**Kosten:** ein nicht fälliger NoOp-Slot kostet einen Throttle-Vergleich, ein fälliger einen leeren
virtuellen Aufruf. Keine Heap-Allokation pro Frame, kein Dispatch in der inneren 100-Hz-Mathematik.

**Der geborgte `const FBWorld*`** ist bereits in der Signatur, obwohl niemand ihn liest: Sensoren/Waffen/
Defensiv bekommen die Welt read-only pro Tick. Ein Waffensystem, das echte Munition spawnt, bräuchte
einen MUTIERBAREN Weltpfad — der kommt mit der ersten realen Implementierung und wurde nicht
spekulativ vorgebaut. (Die reale Lösung ist inzwischen eine andere: das System legt einen
`FBStoreRelease` in eine Warteschlange, der BESITZER spawnt — s. `weapons-and-damage.md`.)

---

## 11. Blockbus-Verträge dieser Systeme

| Block (`core/FBAvionicsBlocks.h`) | Schreiber | Hier dokumentierte Leser |
|---|---|---|
| `Platform` | das Modul (aus dem `st`, das es bekommt) bzw. der Client (`FBSimUnit::HudState`) | `FBDisplaySystem::BuildHud` |
| `AirData` | `FBAirDataSystem` | HUD, Kommandobus-G-Sperre (`CmdBus_.SetLoadFactor`) |
| `RadarAlt` | `FBRadarAltimeter` | `FBWarningSystem`, HUD, Pilot |
| `Nav` | `FBNavSystem` | HUD, `FBF16FireControl` |
| `Cruise` | `FBNavSystem` (zweite Nachricht) | HUD |
| `Warnings` | `FBWarningSystem` | HUD, Pilot |
| `Airframe` | `FBAirframeControls` (über das Modul, das den FDM-Handle für die Tanksummen hält) | `FBNavSystem` (Gear→Cruise-Freeze), `FBWarningSystem` |
| `Ufc` | `modules/f16/FBF16Ufc` | `FBWarningSystem` |

`FBState::NowS` ist die EINE Bus-Zeitreferenz: das Modul stempelt sie einmal pro `Run()` aus seiner
eigenen Sim-Uhr, bevor es irgendeinen Slot taktet, und jeder Blockkopf-Zeitstempel kommt daher. Eine
Uhr für den ganzen Bus ist das, was das Alter eines Blocks beantwortbar macht, ohne dass jedes System
sein eigenes „jetzt" führt.

Die Blockstatus werden als eigene Telemetriespalten veröffentlicht (`blk_*`, `FBStateBusTelemetry`) —
weil ein gehaltener Wert sonst wie ein frischer aussieht.

---

## Offene Punkte

1. **`FBWeaponSystem` ist vestigial.** `systems/FBSystemSlots.h` deklariert ihn weiter als NoOp und
   `FBF16Module` taktet ihn mit 20 Hz (`Weapons->Run(Mode, world, dt)`), obwohl die reale Waffenarbeit
   längst in `FBStoresSystem` (10-Hz-Gruppe) und `FBGunSystem` (jeder Tick) liegt. Sein Banner-Text
   („SMS/Stores, CCIP/CCRP, gun: mode-gated … not built speculatively") beschreibt einen Zustand, der
   nicht mehr gilt. Zu klären: löschen oder mit Zweck füllen.
2. **`FBSystemSlots.h`s Herausgewachsen-Liste ist unvollständig.** Der Banner nennt drei
   herausgewachsene Slots (Displays, Comms, Sensors); real sind es sechs (zusätzlich Defensiv, Stores,
   Gun). Reine Dokumentationslücke im Code.
3. **`FBFlightControl::Run` hat `0.01` fest verdrahtet** statt `FBFdm::kStepS` oder eines `dt`-Arguments
   (Integratoren, Slew-Limit). Das bindet die Innenschleife stillschweigend an genau 100 Hz; ein
   anderer Substep-Takt würde alle Integratorverstärkungen still verschieben. Verhalten heute korrekt,
   weil das Modul ausschließlich mit `FBFdm::kStepS` taktet.
4. **Asymmetrie Bodenkurs vs. Nase.** Das Leg-Gesetz regelt gegen den BODENKURS (`atan2(vx,−vz)`) und
   begründet das ausführlich; COURSE regelt mit demselben `TrackBankCmd` gegen die NASE (`s.yaw`).
   Der Code sagt dazu nur, dass COURSE „auf seinen eigenen zwei Zahlen" bleibt, weil es ein geflogener,
   gemessener Anflug ist. Ob der Drift-Einwand für den Localizer nicht gleichermaßen gilt, ist offen.
5. **COURSE nutzt `KHdg` als `k_dir`.** Dieselbe Zahl (0,8) bedient in DIRECT die Peilungs-P-Verstärkung
   und in COURSE die Richtungsfehler-Verstärkung der Kaskade — im Header nicht als Doppelrolle
   ausgewiesen. Ein Modul, das `KHdg` für DIRECT tunt, verstellt damit unbemerkt den Localizer.
6. **`FBAirDataSystem` publiziert `aoaDeg` nur in die Telemetrie, nicht in den Block.** Ein Konsument
   auf dem Bus (Pilot, HUD) kommt an das AoA nicht heran, obwohl der ADC es hat; `FBF16Pilot`s
   11°-AoA-Anflug wird das brauchen.
7. **`GLoadPeak` wird nie zurückgesetzt** („running max since boot"). Für eine mehrphasige Mission
   (Start → Kampf → Rückflug) gibt es kein Peak-je-Abschnitt und keine Reset-API.
8. **`FBNavSystem::MagVarDeg` ist hart 0** — kein Deklinationsmodell. Jede Anzeige, die „magnetisch"
   beschriftet ist, zeigt derzeit rechtweisend.
9. **`FBRadarAltimeter` kennt kein `dt` und keine Sichtlinien-/Geländeneigungs-Modellierung** — es ist
   ein reiner Differenzrechner auf dem DEM-Sample unter dem Flugzeug (kein Kegelfußabdruck, keine
   Grenzhöhe, keine Verzögerung). Bei Rückenlage/steilen Lagen liefert die echte CARA nichts; hier
   liefert sie weiter.
10. **CLAUDE.md nennt bei `FBNavSystem` „planare ENU-Geodäsie wie `home_bearing`/`home_dist`"** — im
    Code ist `home_bearing`/`home_dist` inzwischen ein Feld des Platform-Blocks, das der Client bzw.
    das Modul schreibt, nicht `FBNavSystem`. Keine Verhaltensdifferenz, nur eine veraltete Verweiskette.
11. **`FBDisplaySystem`s Default-HUD nutzt `render/FBCamera.h` (`w3_cam_from`, `w3_horizon_dip_rad`)**
    — ein `systems/`-Slot inkludiert damit einen `render/`-Header. Das ist die einzige Stelle dieser
    Art in den hier dokumentierten Slots und passt nur deshalb in die Core-Lib, weil diese Mathematik
    CPU-seitig und WebGPU-frei ist (dieselbe Ausnahme, die CLAUDE.md für `render/FBHudGeometry.cpp`
    ausdrücklich einräumt — `FBCamera` nennt sie NICHT).
12. **Kein Guidance-/FCS-Schadens-Gate im Modul.** `FBSystemId::FlightControls` wirkt nur physisch über
    `FBFdm::SetControlAuthority`; die Slots selbst laufen weiter und publizieren weiter. Konsistent mit
    der Absicht („die FLCS kommandiert unverändert, das Flugzeug antwortet nicht"), aber die
    Block-Invalidierungsregel der anderen Slots gilt hier bewusst nicht — im Code nirgends notiert.
