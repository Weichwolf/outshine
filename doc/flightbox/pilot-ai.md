# Piloten-KI — FBPilot, FBBfmTrack, FBEngagement, FBPilotTuning

**Quellenstand:** Commit `9673e00` („Führung hält eine Bahn, wo eine Bahn deklariert ist").
Primärquellen sind die Kommentar-Banner des Quellcodes:

| Datei | Rolle |
|---|---|
| `sim/src/systems/FBPilot.h` / `.cpp` | die Entscheidungsebene: Phasenmaschine, Brief, BFM-Regelgesetz, Attack, Intercept |
| `sim/src/systems/FBBfmTrack.h` / `.cpp` | das Zielbild aus Radarkontakten + das BFM-Scoreboard + `FBTrackDatum` |
| `sim/src/systems/FBEngagement.h` / `.cpp` | die BVR-Zustandsmaschine als Daten + das Debriefing |
| `sim/src/systems/FBPilotTuning.h` / `.cpp` | die Piloten-VARIANTE als Missionsdaten |
| `sim/src/modules/f16/FBF16Pilot.h` | die F-16-Zahlen (alle virtuellen Hooks) |
| `sim/src/modules/f16/FBF16Module.cpp` | Takt-Verdrahtung, `set`-Schlüssel → Brief/Task/Variante |
| `sim/tools/fb_tournament.py` | der Turnierläufer (kein Build-Target) |
| `doc/mission-format.md` §§ Kampf-/Abfang-/Angriffsmissionen, Piloten-Varianten | die Missionsdaten-Seite |

Konvention in diesem Dokument: **[MESS]** = am gepinnten Modell oder im Missions-Regelkreis gemessen,
**[HERL]** = aus einer genannten Gleichung hergeleitet, **[SETZ]** = gesetzt (Konstruktions-Entscheidung
ohne Quellzahl), **[DOK]** = aus `doc/f16/`.

---

## 1. Die Schichtung

Drei Ebenen, drei Fragen, drei Takte. Keine Ebene greift an der nächsten vorbei.

| Ebene | Klasse | Frage | Takt | Ausgang |
|---|---|---|---|---|
| Mission | `systems/FBPilot` | WOHIN soll das Flugzeug | 10 Hz | `FBPilotCommands` |
| Führung | `systems/FBAutopilot` | WELCHES Manöver bringt es dahin | 100 Hz (im FDM-Substep) | `FBGuidance` (Querlage, VS, Speed) |
| Hände | `systems/FBFlightControl` | WELCHER Ruderausschlag | 100 Hz | `FBControls` → `FBFdm::SetControls` |

Der Takt entsteht im Modul, nicht im Piloten: `FBF16Module::Run` ruft `Due(PilotAccS, dt, 10.0)` und
darin `PilotSys->Run(...)`; die 100-Hz-Substeps laufen darunter in derselben `Run()`
(`AP->Run(st)` → `FC->Run(...)` → `fdm.Step(st)`, Spiralschutz ≤ 12 Substeps/Frame).
Eine Piloten-Entscheidung steht damit für ~10 FDM-Schritte. Unmittelbar nach dem Piloten-Tick
veröffentlicht das Modul die fusionierte BFM-Sicht auf den Bus (`SharedState.Bfm = PilotSys->BfmTrack().Block()`)
und ruft `NavSys->AdvanceWaypoint(...)` — die Wegpunkt-Sequenzierung ist Akteurs-Verhalten, keine
Runner-Buchhaltung.

### `FBPilotCommands` — die Ausgabe eines Entscheidungstakts

```
FBPilotGuidance Guidance;                        // None | Manual | Direct | Course
double TargetAltM, TargetSpeedKt;
double TargetLatDeg, TargetLonDeg;               // Direct-Zielpunkt / Course-Referenzpunkt
bool   HaveLeg; double LegLatDeg, LegLonDeg;     // der Bahnursprung, wenn eine Bahn EXISTIERT
double CourseDeg, GlidepathDeg;                  // nur Course (TargetAltM = Schwellenhöhe)
double ManualRoll, ManualPitch, ManualYaw, ManualThr;
std::optional<bool>   GearDown;
std::optional<double> Speedbrake, WheelBrakeLeft, WheelBrakeRight, NosewheelSteer;
std::optional<bool>   EngineStart;
```

Zwei Verträge stecken in den Typen:

- **`FBPilotGuidance::None` = „AP unangetastet lassen".** Das Modul ruft den passenden
  `FBAutopilot`-Setter (`SetManual`/`SetDirect`/`SetDirectLeg`/`SetCourse`) NUR bei einem konkreten Modus;
  ein `None`-Tick ändert nichts an der laufenden Führung. Ein neutraler `FBPilotCommands` (Phase `Idle`)
  ruft in `FBF16Module::ApplyPilotCommands` gar nichts auf.
- **Jede Zellen-Anforderung ist `std::optional`.** Nicht gesetzt = „der Pilot fasst diesen Hebel gerade
  nicht an" — das Abbild der Hände eines echten Piloten, und der Grund, warum die meisten Ticks keine
  einzige Zellen-Anforderung tragen.
- **`HaveLeg` ist KEIN Optional**, obwohl es eines sein könnte: es ist keine Bedienung, die der Pilot
  ausüben kann oder nicht, sondern das, was er WEISS. Kein Bein = keine Linie = das Peilungsgesetz.
  `SetLegFromPlan` setzt es erst ab dem ZWEITEN Wegpunkt eines Plans (`idx > 0`) — zwei deklarierte
  Fixes sind eine Bahn, ein erster Wegpunkt ist eine Peilung. Das ist tragend: würde man aus der
  Spawn-Position ein Bein erfinden, flöge ein Verteidiger, der einen Punkt IN seinem eigenen Kurvenkreis
  umkreisen soll, ihn einmal an und verließe ihn (`missions/bfm-basic.fbm`).

---

## 2. Die Regel, die alles trägt

### 2.1 Der Pilot hält KEINE Systemzeiger

Die gesamte Signatur ist der Vertrag:

```
virtual FBPilotCommands Run(const FBState &state, FBCommandBus &avionics,
                            const FBAirframeControls &airframe, const fb_fdm_state &st,
                            const FBFlightPlan &plan, const FBRunway *runway, double dt);
```

- `state` — der Avionik-Bus. Was Sensoren HINEINGESCHRIEBEN haben, mit Reichweite, Scanvolumen,
  Netzzyklus, Alter und Gültigkeitskopf. Nie Weltwahrheit.
- `avionics` — der Kommandobus. Der EINZIGE Weg von dieser Klasse zu einer Avionikbox.
- `airframe` — die Zelle, geborgt CONST: Instrumentenablesungen (WOW, Fahrwerksstellung, Startgewicht,
  Triebwerk läuft) über dieselbe Schnittstelle, über die die Kommandos wieder herausgehen. Der Pilot
  erreicht kein FDM und weiß nicht, welches er fliegt — das hält diese Schicht zellen- UND
  instanz-agnostisch.
- Ein `const FBWorld *` STAND früher in dieser Signatur (ungenutzt, `(void)world`). Er ist entfernt:
  ein Pilot, dem man Weltwahrheit gar nicht reichen kann, kann nicht versehentlich darauf fliegen.

### 2.2 Avionik nur über den Kommandobus

Jede Bedienung ist ein `avionics.Post(target, value, nowS)` mit Quittung. Das kostet Zeit und kann
abgelehnt werden (`core/FBCommandBus.h`):

| Klasse | Latenz | Beispiele |
|---|---|---|
| HOTAS | `kHotasLatencyS` = 0,5 s | `MasterArm`, `Designate`, `WeaponRelease`, `CmDispense`, `RadarMode`, `RadarSlewEl` |
| DED (Kopf nach unten) | `kDedLatencyS` = 4,0 s | `AlowFt`, `BingoLbs`, `CmdsMode` |
| Abzug (Sonderfall) | `kTriggerLatencyS` = 0,1 s | `GunTrigger` — der Wert IST die Dauer des Drucks; der ABSTAND zweier Drücke bleibt 0,5 s |

Zusätzlich sperrt der Bus Kopf-nach-unten-Eingaben, während der Jet manövriert (Lastvielfaches aus dem
AirData-Block; ohne gültigen Block liest er 1 g).

### 2.3 Der Brief — und ohne Brief bedient er nichts

Eine `set`-Zeile richtet das Flugzeug im Spawn-Fenster ein, VOR dem ersten Piloten-Tick. Eine
`brief_*`-Zeile richtet den PILOTEN ein: sie ist ein Wert, den er im Flug über den Bus EINGIBT.

| Brief | Kanal | Missionszeile |
|---|---|---|
| `BriefAlowFt` | DED | `set brief_alow_ft <ft>` |
| `BriefBingoLbs` | DED | `set brief_bingo_lbs <lb>` |
| `BriefMasterArm` | HOTAS | `set brief_master_arm arm\|sim` |
| `BriefWeapon` | HOTAS | `set brief_weapon gun\|aim9\|aim120` |
| `BriefRelease(atS)` | HOTAS | `set brief_release_s <t>` (wiederholbar, max. `kMaxBriefedReleases` = 8) |
| `BriefChaff(atS)` | HOTAS | `set brief_chaff_s <t>` (wiederholbar, max. `kMaxBriefedDispenses` = 8) |
| `BriefAttack(mode)` | — (keine Box) | `set attack_mode ccip\|ccrp` |

Ablauf (`EnterBriefedItems`, `ReleaseBriefedStores`, `DispenseBriefedCm`):

- **Eine Eingabe pro Entscheidungstakt**, feste Reihenfolge ALOW → BINGO → Master-Arm → Waffenwahl.
  Der Strom ist damit deterministisch, und der Pilot arbeitet einen Hebel nach dem anderen.
- **Wiederholversuch nur bei Bus-Ablehnung** (Kanal belegt, Manöver-Sperre), und zwar frühestens nach
  `kBriefRetryS` = 2,0 s [SETZ]. Begründung im Header: eine abgelehnte DED-Eingabe ist eine Hand vom
  Stick und ein Kopf nach unten; im 10-Hz-Takt zu wiederholen wäre ein Tastaturmakro und würde den
  Kommandostrom zumüllen. Was die zuständige Box ERREICHT hat, ist final — ein Pilot, dem der Jet „nein"
  gesagt hat, tippt dieselbe Zahl nicht ewig neu.
- **Abwürfe und Täuschkörper werden NIE wiederholt.** Eine abgelehnte Auslösung ist eine Entscheidung des
  Jets.
- **Cockpitarbeit nur im Flug**: `CurPhase != Idle && !GetWeightOnWheels()`. Am Boden stehen diese
  Eingaben in der Checkliste vor dem Anlassen, also außerhalb der Phasen dieser Klasse.

### 2.4 Er sieht nur über Sensoren

Alles, was der Pilot über andere Einheiten weiß, kommt aus `FBState`-Blöcken: Radar (ANONYME Kontakte +
Lock-Index), RWR (Peilungen und Emissionsklasse, KEINE Entfernung), FireControl (Startbereich/Abwurflösung),
Stores, Warnungen, Cmds, Datalink. Der Kopf jedes Blocks (`Invalid`/`Valid`/`Held`) wird ZUERST gefragt
— `Readable()` bzw. `IsValid()` stehen in jedem Entscheidungspfad vor dem ersten Zahlenzugriff.

---

## 3. Die Phasen-Zustandsmaschine

`FBPilot::Phase` (Reihenfolge im Enum ist telemetrie-sichtbar, nur ANHÄNGEN):
`Idle, Preflight, Takeoff, Climb, Route, Approach, Flare, Rollout, Shutdown, Bfm, Intercept, Attack`.

| Phase | Guidance | Was sie tut | Ende |
|---|---|---|---|
| `Idle` | `None` | nichts — neutraler Befehl, der AP bleibt unangetastet | nur durch `SetPhase` von außen |
| `Preflight` | `Manual` | Fahrwerk unten, beide Radbremsen 1.0, Leerlauf, Flügel waagerecht | Triebwerk läuft UND `PhaseElapsedS ≥ kPreflightHoldS` (2,0 s) → `Takeoff`; ohne Bodenkontakt bleibt sie neutral |
| `Takeoff` | `Manual` | Bremsen los, Schub `TakeoffThrottleNorm()`, Bugradsteuerung auf die Runway-Achse, Stick neutral bis `Vr − RotationLeadKt`, dann Pitch-PD auf `RotationPitchDeg` | WOW == 0 → `Climb` |
| `Climb` | `Direct` (+ Bein ab WP 2) | zum aktiven Wegpunkt auf DESSEN Höhe, Speed = `ClimbSpeedKt`; Fahrwerk ein bei positiver Rate + AGL-Marge + unter `GearUpLimitKt` | `GetGearPosition() ≤ 0,02` → `Route`; kein Wegpunkt → `Shutdown` |
| `Route` | `Direct` (+ Bein ab WP 2) | zum aktiven Wegpunkt auf DESSEN Höhe UND Speed | Wegpunkt vom Typ `Land` → `Approach`; kein Wegpunkt mehr → `Shutdown` |
| `Approach` | `Course` | verlängerte Runway-Mittellinie + `GlidepathAngleDeg`-Sinkflug zur Schwelle, Speed = `ApproachSpeedKt`, Bremsklappe `ApproachSpeedbrakeNorm`, Fahrwerk unten | Radarhöhe ≤ `FlareStartAglFt` → `Flare`; WOW vorher → `Rollout`; keine Runway → `Shutdown` |
| `Flare` | `Manual` | Schub Leerlauf, Pitch-PD auf `FlareTargetPitchDeg` mit gedämpfter Autorität (`kFlareStickMax` 0,6) | WOW → `Rollout` |
| `Rollout` | `Manual` | Bremsklappe voll, Zwei-Punkt-Aerobrake auf `AerobrakePitchDeg` bis `AerobrakeSpeedKt`, dann proportionales Absenken der Nase; Radbremsen `RolloutBrakeNorm` erst unterhalb; Bugradsteuerung wie im Start | keine — `core/FBMissionMonitor` urteilt „steht auf der Runway" |
| `Shutdown` | `None` | nichts | — |
| `Bfm` | `Manual` (eigenes Gesetz) | § 5 | keine: Kampfphase, per Missionsdeklaration betreten |
| `Intercept` | `Direct` | § 7 | keine (außer intern `Abort`) |
| `Attack` | `Direct` | § 4 | nach dem Egress zurück nach `Route` |

Gemeinsame Bausteine:

- **`PitchHoldStick(targetDeg, pitchDeg, qDegS, stickMax)`** — ein PD (`kRotateKp` 0,15, `kRotateKd` 0,02)
  auf eine Ziel-Längslage. Rotation, Flare und Aerobrake/Derotate sind DASSELBE Gesetz mit anderem Ziel
  und anderer Autoritätsgrenze (`kRotateStickMax` 1,0 bzw. `kFlareStickMax` 0,6).
- **`NosewheelSteerCmd`** — `−(0,01·Querablage[m] + 0,02·Kursfehler[°])`, gekappt auf ±0,6.
  Klein gehalten, weil die modelleigene `steer-cmd-norm`→Grad-Kennlinie bei Rollgeschwindigkeit selbst
  sehr steil ist (~80 °/Einheit bei ~6 kt, `f16.xml`). Achsenkonvention = die von
  `FBMissionMonitor::OnRunway` und `FBAutopilot::SetCourse` (along = 0 an der Schwelle, + in
  Runway-Richtung; + across = rechts) — Start, Rollout und Missions-Urteil sind sich damit einig, was
  „auf der Linie" heißt.
- **Fahrwerk-ein-Freigabe:** `st.vy > kPositiveRateMs` (0,5 m/s), Radarhöhe **lesbar** und
  > `kGearUpAglFt` (10 ft), CAS < `GearUpLimitKt`. Die kleine AGL-Marge ist eine JSBSim-Eigenschaft:
  `FGLGear` friert WOW ein, sobald `gear-pos-norm` erstmals ≤ 0,99 fällt — eine Einfahrt mitten im
  Aufsetzer würde ein veraltetes `WOW=true` einfrieren. Groß darf sie nicht sein, weil das Modell im
  vollen Nachbrenner sonst die 300-kt-Fahrwerksgrenze erreicht [DOK `procedures-takeoff-taxi.md`].
- **Jedes AGL-Gate fragt zuerst den Kopf des Radarhöhen-Blocks.** Ohne gültige Höhe handelt der Pilot
  NICHT: Fahrwerk bleibt unten, der Flare löst nicht aus, der BFM-Boden zieht nicht mehr
  [DOK `controls-commands.md` §6.4 — der Sensor sperrt die Wirkung, nicht das Kommando].

---

## 4. Phase `Attack` — die einzige Phase, deren Entscheidung ein MOMENT ist

Drei Teile; der mittlere dauert einen Tick.

### 4.1 Anflug (RUN-IN)

`FBAutopilot::Direct` auf den aktiven Wegpunkt, auf DESSEN deklarierter Höhe und Geschwindigkeit —
also ein **waagerechter Laydown**. Zwei Gründe, beide im Banner:

1. `Direct` HÄLT eine Höhe. Einen waagerechten, stabilen Anflug fliegt diese Guidance exakt; ein
   20–30°-Sturzflug wäre der Pilot im Dauerkampf gegen seine eigene Höhenhaltung.
2. So bleibt der AUSLÖSEMOMENT die einzige Variable des Versuchs: der Anflug ist auf den Meter
   wiederholbar, jeder Meter Fehlabstand gehört der Rechnung oder dem Moment, nicht dem Fliegen.

Der Anflug ist eine **BAHN, keine Peilung**: der Ursprung wird im Moment des Anflugbeginns EINMAL
verankert (`AtkHaveRunIn_`, `AtkRunInLatDeg_/LonDeg_`) und über `HaveLeg` an `SetDirectLeg` gereicht.
Die Mission deklariert keinen Initial Point, also IST der Punkt, an dem der Anflug begann, der Initial
Point. [MESS, `doc/mission-format.md`] Querfehler 31,6 m → 10,6 m auf dem 19-km-CCRP-Anflug, der
Längsanteil unverändert.

### 4.2 Der EINE Pickle

Die Torbedingungen in der Reihenfolge, in der ein Pilot sie prüft:

| # | Bedingung | Quelle |
|---|---|---|
| 1 | `fc.H.Readable() && fc.AgValid` | kein Rechner, kein Abwurf |
| 2 | `fc.AgArmMarginS > 0` | ein Abwurf von hier käme als Blindgänger an (PUAC) |
| 3 | `fc.AgInRange` mindestens EINMAL gesehen (`AtkInRangeSeen_` gelatcht) | ein Countdown zählt DURCH null; ohne Latch feuert man auch auf eine nie positive Lösung (kein Bodenspeed eingegeben, Ziel längst hinter dem Jet) |
| 4a | **CCRP**: `fc.AgTimeToReleaseS ≤ leadS − bias` | der Cue |
| 4b | **CCIP**: dasselbe UND `\|fc.AgCrossErrM\| ≤ AttackCcipTolM` | die Beurteilung, die ein Countdown nicht treffen kann |

Nur der QUER-Anteil geht in die CCIP-Bedingung. Der Längsanteil ist genau das, worum es beim Cue geht —
er ist im Moment des Drucks absichtlich ungleich null (die Bombe muss noch geworfen werden). Den
kombinierten Fehlabstand zu prüfen würde jeden richtigen Abwurf ablehnen und dann einen späten annehmen.

**Die Vorhaltung um die eigene Betätigungslatenz.**
`leadS = FBCommandBus::LatencyS(FBCommandTarget::WeaponRelease)` = 0,5 s. Genau auf dem Cue zu drücken
ließe den Store eine halbe Sekunde zu spät von der Schiene: bei 230 m/s sind das 115 m — mehr, als die
ganze Rechnung wert ist. **Der echte Jet löst dasselbe Problem andersherum:** im CCRP HÄLT der Pilot den
Knopf und das FLUGZEUG löst aus, wenn der Cue durchläuft [DOK `weapons.md` §2.5]. Die Absicht wird also
früh geäußert und der Moment gehört dem Rechner. Auf einem Bus, auf dem ein Kommando ein diskretes
Ereignis ist, ist „um exakt die Kanal-Latenz vorhalten" genau dieselbe Aussage — und sie ist das Wissen
des Piloten um seine eigenen Hände, kein Blick in etwas, das er nicht sehen darf.

**Der Pilot rechnet keine Ballistik.** Er hält keine Zielposition und löst keine Bahn: er liest den
FireControl-Block wie den Radarhöhenmesser vor einem Flare. Wer rechnet: `core/FBBallistics` (geteilte
Vorwärtsintegration) über `modules/f16/FBF16FireControl`.

Der Pickle wird EINMAL abgesetzt und bei Ablehnung nie wiederholt; die Zeile
`pilot ATTACK_RELEASE` protokolliert Modus, Annahme, `ttrS`, `leadS`, `biasS`, Längs-/Querfehler,
Fehlabstand, Wurfweite, Flugzeit, Schärfreserve, Höhe und Bodenspeed.

### 4.3 Abdrehen (EGRESS)

Nach dem Abwurf: Zielpunkt EINMAL rechnen (`AttackEgressTurnDeg` gegen den aktuellen BODENKURS aus dem
AirData-Block, `AttackEgressRangeM` voraus, `AttackEgressClimbM` höher), dann `Direct` dorthin mit der
Geschwindigkeit, die der Anflug hinterlassen hat, für `AttackEgressS` Sekunden — danach zurück nach
`Route`. Immer nach RECHTS: eine Ausweichkurve muss sich für eine Seite entscheiden, und die Seite aus
der Geometrie zu wählen wäre eine Entscheidung, für die es hier keine Quelle gibt [SETZ].
Kein Schmuck: ein waagerechter Abwurf fliegt das Flugzeug über die eigene Detonation.

### 4.4 Die Vorspannung als Messinstrument

`AttackReleaseBiasS` (Variante `pilot_attack_bias_s`, Band −10…+10 s) verschiebt AUSSCHLIESSLICH
Bedingung 4a. Ein um `bias` Sekunden verspäteter Abwurf landet pro Sekunde eine Bodengeschwindigkeit
weiter — damit beantwortet die Mission die Frage „tut die Rechnung überhaupt etwas".
[MESS] `attack-late.fbm` mit `set pilot_attack_bias_s 2.0`: 482 m Fehlabstand statt 22 m.

---

## 5. Phase `Bfm` — die einzige Phase mit EIGENEM Regelgesetz

Kein Autopilot-Modus: `Guidance = Manual`, wie Takeoff/Flare/Rollout. Grund: `Direct`/`Course` sind
NAVIGATIONS-Modi, deren 60°-Querlagendeckel und bewusst sanfter Rolleinsatz (`FBFlightControl::F16`s
`RollStickMax` = 0,15, eine Reiseflugzahl) für einen Kampf strukturell falsch sind — und sie
umzustimmen würde die Zahlen jeder bestehenden Mission verschieben.

Ein Tick sind vier Schritte: Bild lesen → Verfolgungsart wählen → Zielpunkt bilden → mit der noch
vorhandenen Energie hinfliegen.

### 5.1 Das Gesetz: EIN Auftriebsvektor, EIN Lastvielfaches

Ein Flugzeug kann nur entlang seiner Auftriebsachse (Bauch → Haube) beschleunigen. Eine Kurve ist also
erst ein ROLLEN, das diese Achse dorthin legt, wo Beschleunigung gebraucht wird, und dann ein ZIEHEN.
Gewollt sind zwei Dinge, beide Vektoren in der Ebene senkrecht zur Geschwindigkeit — also werden sie
addiert:

```
L = a_turn·(sin φ, cos φ) + (−g·sin(roll)·cos(pitch), +g·cos(roll)·cos(pitch))      [Körper: rechts, oben]
a_turn = min( V · err_rad / kBfmTurnTimeS , g_avail · g0 )
roll_cmd = clamp( atan2(L_rechts, L_oben) / kBfmRollFullDeg , −1, +1 )
n_cmd    = clamp( |L| / g0 , 0 , g_avail )
```

φ ist die Richtung des Lenkfehlers in denselben Körperachsen; `kBfmTurnTimeS` = 2,0 s [SETZ] ist die
Zeit, in der der Pilot den Lenkfehler weghaben will; `kBfmRollFullDeg` = 60° [SETZ] ist der Rollfehler,
der vollen Querausschlag verdient.

**Drei Verhalten fallen aus diesem einen Ausdruck heraus statt als Sonderfälle codiert zu sein:**

1. Fehler null bei beliebiger Querlage → **L** zeigt in der WELT senkrecht nach oben → der Jet rollt
   flügelgleich und hält 1 g. Das ist die Nachführlösung, ohne eigenen Nachführmodus.
2. Reiner Azimutfehler im Horizontalflug → `roll = atan(a_turn/g)` und `n = 1/cos(roll)` — das IST die
   Kurvenflugbeziehung, hier erreicht statt angenommen.
3. Harte Kurve bei 90° Querlage → die Schwerkraft hat ENTLANG der Auftriebsachse gar keine Komponente,
   die Kurve kostet also nur ihr eigenes g und die Nase fällt.

**Warum `cos(roll)·cos(pitch)` und NICHT `1/cos(roll)`.** Der Term ist die Projektion der Schwerkraft
auf die Auftriebsachse, nicht die Forderung, Höhe zu halten. `1/cos(roll)` wäre das
Horizontalkurven-Lastvielfache — ein Gesetz, das bei 90° Querlage gegen unendlich geht und schon bei 80°
still 5,7 g dafür ausgibt, eine Höhe zu halten, die niemand verlangt hat. Die gewählte Form lässt die
Nase in der Schräglage fallen, und **genau das macht den Energiekampf erst möglich**: Höhe wird gegen
Geschwindigkeit getauscht, ohne dass irgendwo ein „Energie-Modus" steht.

**Die Achsen einzeln:**

- **Rolle** braucht keinen Dämpfungsanteil: der Querstick einer FLCS-Zelle IST ein RATEN-Kommando
  (`f16.xml` differenziert `fcs/aileron-cmd-norm` gegen die gemessene Rollrate), ein Proportionalgesetz
  auf den WINKELfehler ist damit bereits eines auf die Rate.
- **Nick** ist eine PI-Regelung auf den Lastvielfachen-Fehler (`kBfmGKp` 0,25, `kBfmGKi` 0,5,
  I-Grenze ±0,6), weil die Stick-zu-g-Autorität einer FLCS geschwindigkeitsabhängig ist. Bewusst
  ASYMMETRISCH gekappt: voller Zug, aber nur `kBfmPushMax` = 0,3 Druck — ein Jäger zieht und entlastet,
  er drückt nicht.
- **Verfügbares g**: `g_avail = clamp(CornerG·(V/Vcorner)², 1, MaxG)` — Auftrieb wächst mit dem
  Staudruck. Damit fordert die Schleife nie eine Kurve, die es nicht gibt, und der Integrator läuft
  nicht auf.

### 5.2 Verfolgungsart aus der Geometrie

Der Überschuss ist ein **FAHRPLAN, keine Schwelle**: die gewünschte Annäherung ist proportional zur
Restentfernung, damit die Annäherungsrate beim Erreichen der Kontrollentfernung bereits abgebaut ist.

```
ctrlMid = ½·(ctrlMin + ctrlMax)                          // Kontrollposition, über die Variante lesbar
schedKt = clamp( (R_nm − ctrlMid) · BfmClosureGainKtPerNm , ±BfmMaxClosureKt )
overtaking = validTrack && closKt > schedKt + kBfmClosureDeadKt      // Totband 40 kt
```

| Art | Bedingung | Zielpunkt |
|---|---|---|
| `Search` | kein gültiger Track | Richtung + Höhe (§ 5.4), nie ein Punkt |
| `Lag` | `R < ctrlMin` ODER `overtaking` | HINTER ihm entlang seiner Bahn (`BfmLagTimeS`) **und** ÜBER ihm (`BfmYoYoHeightM · excess`) |
| `Lead` | `Aspekt > BfmLeadAspectDeg` ODER `R > BfmLeadRangeNm` | Kollisionsvorhalt: `t_lead = clamp(R/V, 0, BfmLeadMaxS)`, Ziel + `v_tgt·t_lead` |
| `Pure` | sonst | auf ihn |

- **Lag sind ZWEI Verschiebungen, nicht eine.** Hinter ihm stoppt die Nase davor, nach vorn zu
  schießen; ÜBER ihm baut den Überschuss tatsächlich ab — aus der Kurvenebene nach oben ziehen tauscht
  die überschüssige Geschwindigkeit in Höhe, statt sie mit Widerstand zu verbrennen, den der Jet nicht
  hat, und gibt sie im Herunterkommen zurück. Das IST der **High Yo-Yo**. Die Höhe skaliert mit
  `excess = clamp((closKt − schedKt)/BfmMaxClosureKt, 0, 1)`, wickelt sich also von selbst ab.
- **Lead ist eine ZEIT, kein fester Winkel** — der Vorhalt schrumpft mit der Entfernung und verlangt nie
  eine Kurve in leeren Himmel.

### 5.3 Der Gashebel als zweite Hälfte des Annäherungsproblems

Hinter ihn zu zielen hält die Nase drin; es hält keinen Jet auf, der schlicht 100 kt schneller ist. Also
fliegt der Pilot die GESCHWINDIGKEIT, die die Geometrie will:

```
mit Track:   speedErrKt = (v_tgt − v_own)·kt + schedKt      // schedKt ist INNERHALB der Zone negativ
ohne Track:  speedErrKt = BfmCornerSpeedKt − casKt          // Ecke, wo jeder Kampf am besten beginnt
thr = clamp( kBfmThrTrim(0,6) + kBfmThrKpPerKt(0,006)·speedErrKt , 0, 1 )    // ±67 kt = Leerlauf…voll
lowEnergy → thr = 1,0
cas > BfmCornerSpeedKt·kBfmOverspeedFrac(1,15) → thr = min(thr, 0,6)
Speedbrake = speedErrKt < −kBfmSpeedbrakeKt(40) ? 1 : 0
```

**„Ohne Energie" ist relativ, nicht absolut:**
`lowEnergy = casKt < BfmMinSpeedKt && (!validTrack || v_tgt > v_own)`. Ein Verfolger in der
Kontrollposition hinter einem hart und verzögernd kurvenden Verteidiger IST unterhalb seines eigenen
Eckbands, und das mit vollem Nachbrenner zu „korrigieren" wirft ihn nach vorn heraus.
[MESS] die absolute Regel kostete 250 von 268 Sekunden Kontrollposition.

`lowEnergy` wirkt außerdem im Zielpunkt: `if (lowEnergy && aimU > 0) aimU = 0` — ein **Deckel**, keine
Nase-runter-Vorspannung. Ein negativer Höhenwunsch würde den Auftriebsvektor INVERTIEREN (das Gesetz
zeigt die Auftriebsachse auf den Zielpunkt, „unter mir" heißt 180° Rolle), also einen Split-S fliegen.
[MESS] genau das warf den Jet 2.900 m tief, während der Pilot nur 50 kt zurückgewinnen wollte.

### 5.4 Die Suche: das Datum + die Unsicherheitsbreite

Die Suche fliegt eine **RICHTUNG und eine HÖHE**, niemals einen Punkt, und sie fliegt das **DATUM**
(§ 6.2), nicht die zuletzt gemessene Position.

```
if (datum.Valid && datum.RangeM > datum.RadiusM) { brg = datum.BearingDeg; aimU = datum.UpM; }
else                                             { brg = anker(BfmSearchHdgDeg_); aimU = anker(BfmSearchAltM_) − st.elev; }
aim = kBfmSearchRangeM(3 nm) · (sin brg, cos brg)
aimU = clamp(aimU, −tan(kBfmSearchDownMaxDeg 5°)·R, +tan(kBfmSearchUpMaxDeg 20°)·R)
```

Vier Einzelentscheidungen, jede mit gemessener Begründung:

1. **Datum statt gefrorenem Messpunkt.** Der Block fällt jenseits des Extrapolationsfensters auf die
   zuletzt GEMESSENE Position zurück; ein Verteidiger im Break hat davon bis zum Suchbeginn ein Viertel
   seines Kreises entfernt. [MESS] Zielen auf den alten Punkt erzeugte einen 1.500-m-Sturz auf ein
   Datum, dessen Besitzer längst weg war.
2. **Innerhalb des Gebiets ist die Peilung KEINE Information mehr.** Ist der Jet innerhalb
   `datum.RadiusM`, ist der Gegner genauso wahrscheinlich hinten wie vorn; auf den Mittelpunkt zu
   steuern, auf dem man sitzt, lässt die Peilung durch 180° schwingen, das Gesetz antwortet mit einer
   Maximalraten-Umkehr, und die Suche wird ein Orbit. Dann ist die ehrliche Suche die kalte: Kurs und
   Höhe halten und das Scan-Muster arbeiten lassen.
3. **Die kalte Suche wird VERANKERT** (`BfmSearchAnchored_`): auf „wohin meine Nase gerade zeigt" zu
   zielen ist ein Regelkreis ohne Referenz — [MESS] das Weben startet eine Rolle, die Rolle dreht den
   Jet, der Zielpunkt folgt der Drehung, und die Suche setzt sich in einen stabilen 80°-Schräglagen-Orbit,
   der nichts absucht. Sobald wieder ein Track existiert, wird die Verankerung gelöst.
4. **Die Suche wird AUFRECHT geflogen**, mit asymmetrischen Grenzen (20° hoch, 5° runter). Ein steiler
   Abwärtswunsch lässt das Auftriebsvektor-Gesetz invertiert rollen (siehe oben) — [MESS] 2.000 m Zoom
   und Rolle, während das Ziel auf Datum-Höhe in aller Ruhe unbeobachtet flog. Höhe, die in einer
   Schräglage verlorengeht, wird durch STEIGEN zurückgeholt, nicht durch Nachstürzen.

**Das Webmuster** (`SearchWeaveDeg`) — der Zielpunkt wird IN DER WELT um die Vertikale rotiert (nicht
Grad auf den Körper-Azimut addiert), damit der Lenkfehler bei jeder Querlage eine kohärente Richtung
bleibt. Zwei Muster:

| Fall | Amplitude | Periode | Phasenbezug |
|---|---|---|---|
| kein Datum (nie etwas gesehen) | `BfmScanAmplitudeDeg` | `BfmScanPeriodS` | Missionsuhr |
| Datum vorhanden | `clamp(datum.HalfWidthDeg, base, kBfmScanMaxAmpDeg 45°)` | `BfmScanPeriodS · amp/base` | **Suchbeginn** (`ScanSinceS_`) |

- **Die Breite ist das, was der Pilot NICHT weiß.** Eine feste Amplitude sucht dieselben paar Grad ab,
  ob er vor einer Sekunde oder vor einer Minute verlorenging.
- **Die Periode wächst mit**, damit die Eigen-Kursrate des Webens (`2π·A/T`) die sanfte Zahl bleibt, die
  die Zellen-Hooks nennen: eine breitere Suche dauert länger, sie kurvt nicht härter.
  [MESS] ein 20°/10-s-Weben setzte den Jet in einen permanenten 77°-Schräglagen-Orbit und erfasste nichts.
- **Die Phase ist am SUCHBEGINN verankert**, damit der Sweep AUF der Datum-Peilung beginnt — der
  wahrscheinlichsten Peilung, die es gibt — und symmetrisch nach außen wandert. Auf der Missionsuhr
  phasiert, betritt der Pilot jede Suche an einem zufälligen Punkt der Sinuskurve.
  [MESS, 16 Merges einer Geometrie] vorher 6 von 11 Kontaktverlusten wieder erfasst (34/80/81/86/170/240 s,
  fünfmal nie), nachher 11 von 11 (10…141 s, Median 39 s).

Das Weben läuft, sobald `!validTrack || trackAgeS > BfmScanAfterS` — bis dahin folgt die Nase allein
der Extrapolation.

### 5.5 Der Bodendeckel

`if (BfmFloorFt() > 0 && ra.H.Readable() && ra.AglFt < BfmFloorFt())`
→ `elErr += kBfmFloorPullDeg(30°) · clamp(1 − AglFt/FloorFt, 0, 1)`.
Er steht ÜBER allem darüber: ein in den Boden geflogener Kampf ist kein gewonnener Kampf. Ohne lesbaren
Radarhöhen-Block zieht er nicht (§ 3).

### 5.6 Die Kanonen-Nachführung (Abschnitt 3b/3c) — ein GESETZ, keine Zahl

**Eintrittstor.** Drei Bedingungen; die letzten zwei verhindern, dass dies ein schlechteres
Verfolgungsgesetz wird als das obige:

```
gunTrack = Gun.H.Readable() && Gun.Ready && fc.H.Readable() && fc.GunValid
        && fc.GunSpanMr >= fc.GunFunnelBottomMr        // die Reichweitenprüfung des Guides selbst
        && |g.AzDeg| <= BfmControlAtaDeg               // er ist VORN
        && fc.GunAimErrorDeg <= BfmGunTrackMaxErrDeg   // es ist ein NACHFÜHR- und kein Kurvenproblem
```

Eine Kanonenlösung existiert für ein Ziel ÜBERALL, auch für eines, das gerade an der Fläche
vorbeigezogen ist — deren geforderte Rohrrichtung liegt dann 170° neben der Nase. Diese als Lenkfehler
in das Auftriebsvektor-Gesetz zu geben erzeugt eine gewaltsame, energiezerstörende Umkehr:
[MESS] flog den Jet in 158 s in den Boden.

**Warum ein Ratenanteil überhaupt sein muss.** Das Gesetz regelt einen FEHLER und kommandiert eine
Drehrate `err/T`. Gegen ein kurvendes Ziel ist die geforderte Rohrrichtung aber keine Konstante, sondern
eine **RAMPE** — sie wandert durch den Trichter —, und ein Regelkreis, der eine Rampe nur mit
Proportionalanteil beantwortet, bleibt konstant um (Rampenrate × eigene Zeitkonstante) zurück.
[MESS] gegen einen Verteidiger im Maximalraten-Break: Lösung wandert ~1 °/s, T = 2 s, Zielfehler nie
unter 4,6° bei ~1° Trichtertoleranz — zwei Feuerstöße, 70 Schuss, kein Treffer. Das ist kein
Schießproblem, es ist ein **Regelkreis-TYP-Problem**.

**Welche Rate — und hier ist die naheliegende Implementierung die falsche.** Die publizierte Vorhaltung
ist KÖRPERbezogen. Sie zu differenzieren misst also die eigene Zugbewegung genauso wie die Zielbewegung,
und im eingeschwungenen Zustand (genau dem, dessen Nachlauf man entfernen will) ist diese Ableitung fast
null, während die Lösung so schnell wie eh und je durch den Himmel wandert. Eigene Kreiselraten wieder
herauszurechnen wäre Arithmetik auf einer Größe, die sie bereits vermischt hat. Also:

```
(be,bn,bu) = FBBodyLosToEnu(roll,pitch,yaw, fc.GunLeadAzDeg, fc.GunLeadElDeg)   // in die WELT
rate  = α-Filter( d(be,bn,bu)/dt ),  α = kBfmLeadRateAlpha = 0,4
k     = kBfmTurnTimeS · min(1, rateMax/|rate|),  rateMax = CornerTurnRateDegS()
(azErr,elErr) = FBEnuToBodyLos( (be,bn,bu) + k·rate )                            // zurück in den Körper
```

Nichts von der eigenen Rolle, Nick oder Gierbewegung überlebt diesen Rundweg — das ist der Punkt.
Dem bestehenden Gesetz übergeben ergibt das ein Drehratenkommando `err/T + ω`: der P-Anteil nimmt den
Fehler heraus, der Ratenanteil hält die Nase auf einer Lösung, die nicht stillsteht. Die Begrenzung auf
`CornerTurnRateDegS` ist keine Vorsicht: eine schneller wandernde Lösung ist kein Nachführproblem mehr,
und sie zu fordern würfe den Auftriebsvektor auf einen Punkt, den die Zelle nicht erreicht.
Der Filter existiert, weil die Schätzung eine Differenz EINES publizierten Floats über zwei 10-Hz-Ticks
ist — schnell genug für eine Umkehr, langsam genug, dass ein verrauschter Look nicht die Nase schwenkt.

**Der Integrator und seine Herleitung.** Der Vorwärtsanteil lässt genau das stehen, was die
RATENSCHÄTZUNG selbst falsch hat: ein Filter hat Nachlauf, und die Bewegung der geforderten Rohrrichtung
hängt schwach von der Eigengeschwindigkeit ab, die Schätzung ist also etwas zu klein und der Kreis
schwingt etwas hinterher ein. [MESS] stationär 1,45° bei einer Lösungstoleranz nahe 0,4°. Ein konstanter
Versatz in einem Kreis, der bereits P- und Ratenanteil hat, ist exakt der Fall für einen Integrator —
und der konvergiert hier auf eine bestimmte Zahl: im Gleichgewicht ist der Fehler null und der
Integrator hält genau den Fehlbetrag des Vorwärtsanteils.

**Sein Gewinn ist keine Geschmacksfrage** [HERL]: das Gesetz kommandiert eine Drehrate `(e + I)/T` gegen
einen Winkel, der Kreis schließt sich also als

```
s² + s/T + Ki/T = 0        →    ζ = 1 / (2·√(Ki·T))
Ki = 1/(2T)                →    ζ = 1/(2·√(0,5)) = 0,7071
```

also `kBfmTrackKi = 0.5 / kBfmTurnTimeS` = 0,25 s⁻¹ bei T = 2 s — die Lehrbuchwurzel „schwingt sich ohne
Überschwingen ein", und nichts weiter muss gewählt werden. [MESS] das Doppelte (ζ = 0,5) wurde zuerst
probiert: gegen einen Verteidiger im Break ein klarer Gewinn, gegen einen geradeaus fliegenden schwang
der Kreis und die Trichterzeit brach ein. Wind-up-Grenze `kBfmTrackIMaxDeg` = 10°; der Trichterabriss
setzt beide Integratoren hart zurück.

**Die Begrenzung auf das Eintrittstor.** Die drei Anteile (P, Rate, I — hier alle drei WINKEL) können
sich zu einer Forderung addieren, die weit größer als der Zielfehler selbst ist. Dieses Gesetz ist aber
nur für das NACHFÜHR-Problem zuständig, also begrenzt genau die Schwelle, die es hereingelassen hat,
auch was es fordern darf:

```
mag = √(azErr² + elErr²);  lim = BfmGunTrackMaxErrDeg;  if (mag > lim) skaliere beide auf lim
```

[MESS] ohne sie erreichte die kombinierte Forderung auf kürzeste Entfernung sechzig Grad, der Jet
antwortete wie auf jede Sechzig-Grad-Forderung — voller Ausschlag in beiden Achsen — und departierte
(LOC-K.O. bei 150 °/s Rollrate).

Der Zustandswechsel wird protokolliert: `pilot GUN_TRACK` / `pilot GUN_BREAK` mit Entfernung, Flugzeit,
Zielfehler, Vorhaltewinkeln, Trichtergeometrie und Trommelinhalt. Im Trichter meldet das Scoreboard
`Lead` — es IST Vorhalteverfolgung.

**Gesamtwirkung** [MESS, je acht Anflüge pro Verteidiger, vorher/nachher]: Trichterzeit 3,2 → 20,7 s
(geradeaus) bzw. 0,0 → 21,6 s (kurvend); Schuss auf dem Ziel 11,9 → 111,2 bzw. 0,0 → 120,4 Patronen;
Abschüsse 0 → 5 bzw. 0 → 7 von je acht Läufen; mittlerer Nachführfehler 10,5° → 6,9° bzw. 11,9° → 4,1°.

### 5.7 Der Rollraten-Regler — das dritte Gesetz

Das Auftriebsvektor-Kommando ist ein WINKELfehler, der F-16-Querstick ein RATEN-Kommando: ein großer
Fehler bedeutet vollen Ausschlag, solange er dauert — und der größte Fehler, den dieses Gesetz erzeugen
kann, ist 180° (es nimmt immer den kurzen Weg). [MESS] eine Wiedererfassung auf 3,7 nm verlangte 230°
Rolle, der Jet antwortete drei Sekunden lang mit 150 °/s, und der Flugmonitor nannte es, wonach es von
außen aussieht: ein Departure.

```
kBfmRollRateMaxDegS = 120 / kBfmTurnTimeS = 60 °/s
if (|p| > cap && p·rollCmd > 0 && BfmRollCmdPrev_ != 0)
    lim = |BfmRollCmdPrev_| · cap / |p|;   if (|rollCmd| > lim) rollCmd = ±lim
```

**Warum das VORHERIGE Kommando skaliert wird und nicht das rohe.** Auf einem Raten-Stick sind Kommando
und Rate proportional, also IST `cmd_prev · cap/rate` das Kommando, das die Deckelrate erzeugt hätte —
und der Fixpunkt dieser Rekursion ist die Deckelrate selbst. Skaliert man stattdessen das rohe
(gesättigte) Kommando, konvergiert das Ganze auf das geometrische Mittel aus Deckel und
Vollausschlagsrate der Zelle: [MESS] ein 45-°/s-Regler hielt 85 °/s und der Jet rollte glatt durch 360°.

**Die GRÖSSENORDNUNG folgt aus dem Gesetz, das er bedient:** die Rolle existiert, um eine Kurve zu
etablieren, deren eigene Zeitkonstante `kBfmTurnTimeS` ist — auch der schlimmste Fall muss also in etwa
dieser Zeit geflogen werden, das sind Zehner von Grad pro Sekunde, keine Hunderter. Der **WERT** ist
gemessen [MESS, 16 Kanonengefechte gegen zwei Verteidiger]: ungeregelt und bei 90 °/s departierte der
Kampf in sechs davon, bei 60 °/s in keinem — und die Schießleistung war dort ebenfalls am besten.
Der Regler REDUZIERT nur, er kann nie Stick hinzufügen.

### 5.8 Der Abzug (`BfmGunfire`)

Kein zweites Zielen — ein Finger. Er tut drei Dinge:

1. Prüft, ob die Kanone scharf und geladen ist (`Gun.H.Readable() && Gun.Ready`) und eine gültige
   Lösung existiert.
2. **Sagt den Zielfehler voraus**: `pred = err + (dErr/dt)·(LatencyS(GunTrigger) + fc.GunTofS)`.
   Jede HOTAS-Handlung trifft eine Latenz später ein, und bei Nachführraten eines Jägers bewegt sich der
   Zielfehler ~2 °/s — auf 300 m sind das zehn Meter Fehlabstand pro Sekunde Verzug.
   [MESS] Feuerstöße, die auf einer 0,35°-Lösung befohlen wurden, kamen auf einer 1,7°-Lösung an und
   gingen 8 m vorbei.
3. **Hält den Pipper enger als der Trichter.** Die Trichterwände sind die SPANNWEITE des Ziels; eine
   Lösung knapp innerhalb setzt das Trefferbild eine halbe Spannweite neben seine Mitte.
   `pred > Tuned(GunFireTolFrac, BfmGunFireTolFrac()) · fc.GunTolDeg` → nicht schießen.
   0,35 [SETZ] legt das Muster in den Rumpf statt irgendwohin über die Spannweite.
4. Squeeze über den Bus (`GunTrigger`, Wert = `BfmGunBurstS`), und nicht wieder vor Ablauf dieser Dauer
   (`GunNextS_`).

**Er prüft NIE, auf wen er schießt** — und kann es nicht: der Pilot sieht einen Radarkontakt, keine
Besetzungsliste. Die Mission deklariert die Besetzung, der Abzug beantwortet den Trichter.

### 5.9 Die Kontrollposition und das Scoreboard

```
inControl = validTrack && Locked && ctrlMin ≤ R ≤ ctrlMax
         && AspectDeg ≤ BfmControlAspectDeg && |AzDeg| ≤ BfmControlAtaDeg
```

Nur `ctrlMin`/`ctrlMax` gehen über die Variantentabelle (`pilot_bfm_ctrl_min_nm`/`_max_nm`) — es ist die
eine BFM-Zahl, die eine Mission wirklich ändern muss: eine Raketen-Halteposition liegt AUSSERHALB des
Kanonentrichters [DOK `weapons.md` §2.5: 600–3.000 ft], ein Kanonen-Brief IST also eine andere
Kontrollposition und sonst nichts.

---

## 6. `systems/FBBfmTrack` — das Bild und das Gedächtnis

### 6.1 Woraus es gebaut wird — und woraus ausdrücklich NICHT

**Nur** aus `FBState.Radar` (Kontakte + Lock-Index) und dem eigenen `fb_fdm_state`. Im Include-Baum
dieser Datei gibt es **kein `FBWorld`, keine `FBUnitRegistry`, keinen Datalink-Track** — das ist die
Anti-Cheat-Eigenschaft, auf der der ganze BFM-Beweis ruht: die gemeldete Position ist eine berechnete
Schätzung, die in der Missionsanalyse gegen eine Wahrheit gelegt werden kann, die sie nie gesehen hat.

**Kopf zuerst**: ein `Invalid` Radar-Block ist ein Gerät, das nicht schaut, und sein Kontaktarray bedeutet
nichts. `Update()` liest nur den GELOCKTEN Kontakt — ein ungelockter Suchrückstrahler ist eine Detektion,
kein Ziel, auf das sich der Pilot festgelegt hat; beides zu mischen ließe die Verfolgung zwischen Jets
springen.

**Ein Look, kein Re-Read.** `lookS = nowS − c->LookAgeS`; nur wenn `lookS > LastLookS_ + kMinLookDtS`
(0,05 s) wird gefaltet. Ein zwischen zwei Looks erneut gelesener Kontakt trägt DIESELBE eingefrorene
Geometrie — ihn zu differenzieren würde eine Null in den Geschwindigkeitsfilter schieben.

**Der Echo-Ort ist WELT-referenziert** (Peilung + Elevationswinkel), nicht Körper-Az/El: letztere wurden
gegen die Lage ZUM LOOKZEITPUNKT gemessen, auf die Lage JETZT angewandt schmierten sie die Eigenbewegung
eines rollenden Jets in die geschätzte Zielgeschwindigkeit.

**Der Alpha-Filter.** `Vel += kVelAlpha · (Δpos/Δt − Vel)` mit `kVelAlpha` = 0,25. Bei ~0,1 s
STT-Frame ist das eine Zeitkonstante von ~0,4 s.
[MESS, gegen die aus beiden Unit-Logs rekonstruierte Wahrheit] aufeinanderfolgende Looks mit diesem
Filter treffen die RICHTUNG der geschätzten Geschwindigkeit auf 1,8° (Median, p90 1,9°); eine
Halbsekunden-Basislinie — fünfmal weniger Differenzierungsfehler, eine halbe Sekunde mehr Nachlauf —
lag 5,4° daneben.

**Extrapolation und Einfrieren** (`Predict`, jeder Tick, ob frischer Look oder nicht):

| Alter | `Blk_.H.Status` | Position |
|---|---|---|
| ≤ `kMaxExtrapolateS` (8,0 s) | `Valid` | letzter Look + `Vel·Alter` — jung genug, um darauf VORZUHALTEN |
| > 8,0 s | `Held` | zurück auf die zuletzt GEMESSENE Position, eingefroren |

Der Stempel ist immer der LOOK, nicht `now` — Alter am Kopf ist Alter seit der Sensor ihn sah.
Begründung für die 8 s: ein Konstantgeschwindigkeits-Modell gegen einen kurvenden Jäger geht schnell
falsch; bei ~5 °/s Dauerkurvenrate liegt eine Geradeausvorhersage nach acht Sekunden bereits einen
Großteil eines Kurvendurchmessers daneben, und dann ist „wo er WAR" ehrlicher als „wo er wäre, hätte er
aufgehört zu manövrieren".

**Die Annäherungsrate** kommt vom Radar, SOLANGE es hinsieht; im Coast aus der Schätzung selbst
(Relativgeschwindigkeit auf die Sichtlinie). Die eingefrorene letzte Messung wäre schlimmer als nutzlos:
ein Merge endet mit mehreren hundert Knoten Annäherung im Protokoll, und ein Pilot, der diese Zahl noch
liest, fliegt einen längst beendeten Overshoot weiter, statt zurückzudrehen.

**Aspekt** [HERL]: der Winkel AM ZIEL zwischen seinem Heck und der Sichtlinie zu uns. Mit **L** =
Einheitsvektor eigen→Ziel und **T** = seine Einheits-Geschwindigkeit zeigt Ziel→uns entlang −**L** und
sein Heck entlang −**T**, also `cos(aspect) = (−T)·(−L) = T·L`. Undefiniert unterhalb
`kMinTrackSpeedMs` (20 m/s) — dann bleibt der letzte Wert stehen.

**Energiehöhe** `Es = (h + v²/2g)` in ft: die einzige Energiezahl, die ein Pilot von EIGENEN
Instrumenten ablesen kann.

### 6.2 `FBTrackDatum` — das Gedächtnis, vollständig hergeleitet

Der Block beantwortet „wo ist er" und hört jenseits des Fensters ehrlich auf. Dieses Einfrieren ist für
die VERFOLGUNG richtig — man zieht keine Vorhaltung auf eine Vermutung — und für die SUCHE nutzlos.
Suchen braucht zwei Zahlen, die der Block nicht trägt: einen PUNKT und die BREITE des Gebiets darum.

Beide kommen aus derselben Schätzung, mit EINER zusätzlichen Annahme: der Gegner kurve etwa so hart wie
man selbst.

**Die Verschiebung** zwischen Geradeaus-Vorhersage und einer Kurve mit konstanter Rate ω, nach t
Sekunden bei Geschwindigkeit V:

```
d(t) = (V/ω) · √( (ωt − sin ωt)² + (1 − cos ωt)² )
```

Reihenentwicklung für die Zeiten, die zählen (ωt klein): `ωt − sin ωt ≈ (ωt)³/6`,
`1 − cos ωt ≈ (ωt)²/2` — der zweite Term dominiert, also

```
d(t) ≈ (V/ω)·(ωt)²/2 = 0,5 · V · ω · t²
```

**Die harte Schranke:** weiter als `V·t` von der zuletzt tatsächlich gesehenen Position kann er nicht
sein, was immer er getan hat. Also

```
RadiusM = min( 0,5·V·ω·t² , V·t )
```

**Der Kreuzungspunkt** der beiden Hälften [HERL]: `0,5·V·ω·t² = V·t ⟺ t = 2/ω`.
VOR ihm ist die Vorhersage mehr wert als der letzte Look, NACH ihm könnte die Kurve überall hingegangen
sein und der ehrliche Mittelpunkt hört auf, sich zu bewegen — deshalb `tProp = min(AgeS, 2/ω)`.
Diese Grenze ist HERGELEITET, nicht gewählt, und für die F-16 dieses Simulators landet sie bei ~7,3 s —
dort, wo `kMaxExtrapolateS` UNABHÄNGIG davon auf 8 s gesetzt worden war. Zwei verschiedene Fragen, eine
Antwort.

**ω = „er kurvt wie ich"** — `FBPilot::CornerTurnRateDegS()`:

```
ω = g·√(n² − 1) / V        (n, V = die EIGENEN Corner-Hooks der Zelle)
F-16: n = 5,6, V = 380 kt = 195,5 m/s → 9,80665·√30,36/195,5 = 0,2764 rad/s = 15,8 °/s
```

[MESS] `make -C sim test-corner` misst am selben Modell direkt 16,2 °/s — die Herleitung geht gegen das
Modell auf. Dieselbe Methode wird ZWEIMAL benutzt und ist deshalb eine Methode und keine zwei
Konstanten: sie ist die schnellste Nasenbewegung, die dieser Jet einer wandernden Kanonenlösung
nachführen kann, UND die Annahme über den anderen.

**`HalfWidthDeg`** = `atan2(RadiusM, horizontale Entfernung)` — der Radius, von HIER aus als Winkel
gesehen, also genau die Halbbreite, die eine Suche abdecken muss. Damit ist das Scanmuster eine FOLGE
dessen, was der Pilot nicht weiß, statt ein festes Weben.

`Datum()` ist bewusst NICHT aus `Block()` abgeleitet: der Block fällt jenseits des Fensters auf den
Messpunkt zurück und friert ein (richtig für die Verfolgung, falsch für die Suche), also rechnet diese
Methode aus derselben gespeicherten Schätzung mit ihrer EIGENEN Fortschreibungsregel. Const,
allokationsfrei, einmal pro Entscheidungstakt.

### 6.3 Die `bfm_*`-Kanäle (Quelle `bfm`, 15 Spalten, hinten angehängt)

| Spalte | Bedeutung |
|---|---|
| `bfm_pursuit` | `none`/`search`/`lead`/`pure`/`lag` |
| `bfm_valid` / `bfm_locked` | Schätzung jung genug zum Vorhalten / Radar hält ihn JETZT |
| `bfm_age` | s seit dem letzten echten Look (−1 wenn ungültig) |
| `bfm_rng` / `bfm_ata` / `bfm_aspect` / `bfm_hca` / `bfm_clos` | nm / ° off nose (+ = rechts) / ° AM ZIEL (0 = im Rücken) / Kursdifferenz / kt (+ = annähernd) |
| `bfm_es` | eigene Energiehöhe (ft) |
| `bfm_gcmd` / `bfm_ctrl` | kommandiertes g / Kontrollposition JETZT |
| `bfm_engaged` / `bfm_lock_s` / `bfm_ctrl_s` | die drei Integrale (s) — Lock-Haltequote und Zeit in der Kontrollposition aus der LETZTEN Zeile ablesbar |

Jede Größe ist aus EIGENER Perspektive berechenbar. Alles, was Weltwahrheit braucht (z.B. der WAHRE
Aspekt), gehört in die Auswertung, nicht in den Piloten.

---

## 7. Phase `Intercept` — geflogen mit dem SENSOR

Der Gegenpol zu BFM: BFM wird mit der NASE geflogen und der Lock geht nie weg; ein Abfang wird mit dem
SENSOR geflogen, und die ganze Kunst ist, wann man ihn worauf richtet. Guidance ist `Direct` auf einen
Punkt `kInterceptAimM` (60 nm) entlang des gewünschten Kurses — weit genug, dass die Peilung dorthin
über einen ganzen Tick auf Bruchteile eines Grades der gewünschte Kurs ist.

Reihenfolge eines Ticks = Reihenfolge der Aufmerksamkeit eines Piloten: was sehe ich → wer sieht mich →
in welchem Zustand bin ich → wohin zeige ich den Jet → und erst dann welchen Schalter fasse ich an.

### 7.1 Das Bild

- Gelockt → der Kontakt bei `LockIndex`. Sonst der NÄCHSTE Rückstrahler, der sich nicht als Freund
  ausgewiesen hat: eine gültige Mode-4-Antwort BEWEIST freundlich und nimmt ihn von der Liste, Schweigen
  beweist nichts und bleibt Kandidat (`core/FBRadarContact.h` kennt keinen Wert „hostile") — das IST das
  Identifikationsproblem, keine Abkürzung darum herum.
- `haveTgt = tgt && LookAgeS < kInterceptLostS` (10,0 s) — zwei CRM-Frames plus Marge: ein verpasster
  Sweep ist ein verpasster Sweep, drei sind ein Ziel, das nicht mehr da ist.
- `Bfm_.Update()` läuft AUCH hier: die Fusion liefert, was ein einzelnes Echo nicht kann — die
  Zielgeschwindigkeit und daraus den Aspekt, auf dem die Schussentscheidung beruht.

### 7.2 Wer mich sieht

Aus dem RWR-Block: die stärkste Nicht-Such-Warnung, wobei ein **Raketen**-Symbol ein **Track**-Symbol
IMMER schlägt (das eine ist ein Radar, das schießen könnte, das andere ein Suchkopf, der es bereits hat).

**Die Kernregel — wann eine Warnung eine Antwort verlangt:**

```
shotSelfSufficient = Eng_.HaveShot() && (Eng_.Pitbull() || !locked)
mustDefend = threatMissile || (threatTrack && (!weapons || shotSelfSufficient))
defendDue  = mustDefend && (now − IntDefendCueS_) ≥ Tuned(ReactionS, kInterceptReactionS)
```

Ein Suchkopf auf dem eigenen Flugzeug ist **nie** verhandelbar. Ein bloß VERFOLGENDES Radar ist es:
vor dem eigenen Schuss von einem Lock-Spike wegzudrehen verliert das Gefecht — der ganze Grund, warum
ein Jäger es hinnimmt, verfolgt zu werden, ist, dass er gleich zurückschießt. Ein Track-Spike verlangt
also erst eine Antwort, wenn der eigene Angriff nichts mehr zu gewinnen hat: der Schuss ist weg und
braucht keine Führung mehr, oder es gab nie einen.

`IntDefendCueS_` ist die Null der Reaktionszeit — der Moment, in dem die Warnung eine Antwort VERLANGTE,
nicht das erste je gesehene Symbol.

### 7.3 Die Zustandsmaschine (`FBEngageState`)

| Zustand | Was der Pilot tut | Verlassen wenn |
|---|---|---|
| `Idle` | wie `Search` behandelt | s.u. |
| `Search` | gebrieften Vektor fliegen (aktiver Wegpunkt = Peilung + Höhe), Suchmodus wählen, Antenne auf das erwartete Höhenband, **NICHT locken** (ein Lock ist eine persönliche Warnung an den, auf den er zeigt) | `haveTgt` → `Attack` wenn `R ≤ LockRangeNm`, sonst `Closing` |
| `Closing` | Verfolgungskurs auf den Kontakt, co-altitude, Antenne auf dem Rückstrahler zentriert — weiter OHNE Lock | `R ≤ LockRangeNm` → `Attack`; Kontakt weg → `Search`; keine Waffen → `Abort`; `R < AbortRangeNm` und nie geschossen → `Abort` |
| `Attack` | designieren (TMS vorwärts) wenn noch nicht gelockt; Startbereich lesen; schießen wenn `inParams` UND Lock ≥ `kInterceptTrackSettleS` alt UND Schussabstände eingehalten | Schuss registriert (Stores-Zähler) → `Support`; sonst wie oben |
| `Support` | **Lock HALTEN** (die AIM-120 fliegt ihre Mittelphase am Uplink) und dabei **CRANKEN**: wegdrehen, bis das Ziel am Rand dessen sitzt, was die Antenne noch führen kann | `now − ShotS ≥ holdS` ODER (`!locked && !Pitbull`) → `weapons && haveTgt ? Attack : Abort` |
| `Defend` | **BEAMEN**: quer zur Bedrohungspeilung drehen (kürzerer Weg), Täuschkörper alle `ChaffIntervalS` | `now − IntThreatLastS_ ≥ DefendHoldS` → `CanPressOn(state) ? Search : Abort` |
| `Abort` | kalt abdrehen: 180° weg vom Letzten, was auf dieses Flugzeug gezeigt hat | terminal |

`defendDue` schlägt alles andere und setzt sofort `Defend`.

**Der Schuss-Torschluss:**

```
inParams = fc.DlzValid && fc.InZone
        && fc.TargetRangeM ≤ fc.RtrM · Tuned(ShotRtrFactor, InterceptShotRtrFactor())
        && |tgtAzDeg| ≤ Tuned(ShotAtaDeg, InterceptShotAtaDeg())
wantShot = locked && inParams
        && (now − IntLockSinceS_) ≥ kInterceptTrackSettleS
        && (now − IntLastShotS_) ≥ Tuned(ShotSpacingS, …) && now ≥ IntNextShotS_
```

Geschossen wird bei **Rtr**, nicht bei Raero: Raero ist die kinematische Maximalreichweite, also ein
Schuss, den der Gegner durch Umdrehen und Weglaufen besiegt; Rtr ist die Entfernung, aus der die Runde
auch dann ankommt, wenn er genau das tut. `RtrFactor` ist ein ANTEIL einer Zahl, die die Feuerleitung je
Schuss rechnet — kein eigener Entfernungswert —, bleibt also richtig, wenn sich die Geometrie ändert.

**`kInterceptTrackSettleS` = 2,0 s** [HERL/MESS]: ein Single-Target-Track ist nicht in dem Moment eine
Feuerlösung, in dem die Antenne stehenbleibt. Die Runde wird mit der geschätzten BEWEGUNG des Ziels
programmiert (`core/FBWeaponUplink`), und diese Schätzung wird aus aufeinanderfolgenden Looks
differenziert und gefiltert (`kVelAlpha`, ~0,4 s Zeitkonstante auf einem 0,1-s-STT-Frame). Innerhalb
einer Sekunde nach dem Designieren zu schießen heißt, eine Runde mit einer Geschwindigkeit von nahezu
null zu starten — [MESS] der Aspekt, unter dem der Schuss fiel, ließ sich nicht einmal berechnen. Zwei
Sekunden sind mehrere Filter-Zeitkonstanten und zugleich das, was eine echte Schusssequenz an
Schalterarbeit kostet.

**Nach dem Schuss:** `IntNextShotS_ = now + max(ShotSpacingS, fc.TimeToImpactS)` — eine zweite Runde ist
erst dann überhaupt eine Frage, wenn die erste ihre Chance hatte. Der Pilot erfährt über den Treffer
weiterhin nichts außer über die eigenen Sensoren: ein zerstörter Jet hört auf, ein Radarkontakt zu sein,
weil er fällt, nicht weil es ihm jemand sagt.

**Die Crank-Dauer.** NICHT „bis der Suchkopf übernimmt": das ist der Moment, ab dem die Runde den UPLINK
nicht mehr braucht, nicht das Ende des Schusses. Zwischen Suchkopf-Aktivierung und Einschlag kann dieser
Jet nichts mehr für sie tun und hat jeden Grund, dem Ziel derweil nicht entgegenzufliegen:

```
holdS = min( kInterceptSupportMaxS(60 s) , max( Eng_.ShotTtiS() , max(Eng_.ShotTtaS(), 0) ) )
```

Der Deckel fängt einen Startbereich ab, der nie einen Countdown produziert hat (`TimeToImpactS < 0`:
die Runde stirbt, bevor sie ankommt). Das andere Ende ist das Versagen: Lock weg UND der Suchkopf hat
NICHT übernommen — die Runde fliegt eine Schätzung, die niemand mehr auffrischt; dafür lohnt die
Rückkehr, denn Neu-Designieren nimmt den Uplink wieder auf.

**Die Crank-Seite wird einmal pro Schuss festgelegt** (`IntCrankSign_`): ein Crank, der seine Richtung
jeden Tick neu wählt, ist ein Jet, der eine S fliegt, während seine Runde ungestützt bleibt.
Kommandiert wird `aimHdg = st.yaw + wrap180(tgtAz − sign·CrankAtaDeg)`.

**Der Beam.** 90° zur Bedrohungspeilung (`InterceptBeamOffsetDeg`): dort hat die Eigengeschwindigkeit
keine Komponente auf seine Sichtlinie, was genau das Clutter-Filter eines Puls-Doppler-Geräts ist — und
es ist die EINZIGE Geometrie, in der Düppel überhaupt etwas wert sind (`systems/FBRadarSystem`s Notch).
Von den beiden Wegen der kürzere, weil die Drehung selbst Zeit in seinem besten Fall ist.

### 7.4 `CanPressOn` — die drei Instrumente der Wiederaufnahme

```
weapons = Stores.H.Readable() && Stores.LoadedCount > 0
bingo   = Warnings.H.Readable() && (Warnings.Active & FBWarnBingo)
sensor  = Radar.H.Readable() && Radar.Radiating
return weapons && !bingo && sensor;
```

Drei Instrumente, drei Gründe heimzufliegen, jeder vom BUS abgelesen statt gewusst: ein Jet mit leeren
Trägern kann ihn nicht töten, einer bei BINGO kommt danach nicht heim, und einer, dessen Gerät nicht
strahlt, findet ihn nicht einmal. Die Fuel-Beurteilung trägt der WARN-Block, weil BINGO eine Zahl ist,
zu der sich der PILOT verpflichtet hat (`systems/FBWarningSystem` gegen die gebriefte Schwelle) — kein
Bruchteil, den diese Klasse erfinden darf.

**Die Wiederaufnahme sucht das DATUM, nicht den gebrieften Vektor.** Ein Jäger, der zur Verteidigung
abgebrochen und überlebt hat, nimmt nicht die Vektorierung eines Fluglotsen wieder auf, als wäre nichts
gewesen — er geht dorthin zurück, wo er ihn zuletzt wusste. Kurs, Höhenband, Antennenelevation UND
Webbreite kommen alle aus `FBBfmTrack::Datum`. Hat er nie etwas gesehen, ist das Datum ungültig und jede
Zahl bleibt Byte für Byte die gebriefte — deshalb ist ein Abfang ohne je einen Kontakt von alldem
unberührt.
[MESS, `bvr-duel.fbm`] vorher flogen beide nach dem Abwehrmanöver ihren Vektor weiter und trennten sich
**474 s** lang bis 70,7 km, jeder mit einer Rakete an Bord. Danach kehren beide um (größte Trennung
55,6 km bei t≈280 s), gehen bei t≈355 s wieder in `closing`, bei t≈365 s in `attack`, zweiter Schuss bei
t = 527 s (43,6 m Fehlabstand, im Notch abgewehrt wie der erste). Timeout deshalb 320 → 700 s.

### 7.5 Die Antennenführung

- `Search`: `wantEl = atan2(bandAltM − st.elev, distM)·rad2deg − st.pitch`. Der **eigene Nick** ist
  das, was daraus ein Kommando statt einer Konstante macht: das Muster ist an die Nase geschraubt, ein
  steigender Jet schaut also aus dem Band heraus, das er absuchen soll, wenn die Antenne nicht um genau
  diesen Winkel zurückgedrückt wird.
- `Closing`/`Attack`/`Support`: `wantEl = tgtElDeg` — den Rückstrahler **zentrieren**, nicht relativ
  nachführen. Sowohl die gemeldete Kontaktelevation als auch die Mitte des Volumens sind
  körperbezogen, die gewünschte Antennenstellung IST also der Winkel, unter dem der Kontakt zurückkam.
  [MESS] auf das aktuelle Kommando zu ADDIEREN wanderte die Keule Look für Look vom Ziel weg und verlor
  einen frontalen Kontakt zwanzig Sekunden nach der Erfassung.

### 7.6 Die Hände (`InterceptCockpit`)

**Höchstens EINE Bedienhandlung pro `Tuned(ActionSpacingS, kInterceptActionS)`** (Default 0,5 s), in
fester Prioritätsreihenfolge:

1. **Düppel** (`CmDispense`, Wert 0 = das vom PRGM-Knopf gewählte Programm) — das Flugzeug, auf das
   geschossen wird, editiert keinen Radarmodus.
2. **Schuss** (`WeaponRelease`).
3. **Designation** (`Designate`, Wert = Tracknummer; 0 = lösen) — nur wenn das Gerät nicht schon den
   verlangten Lock hält: eine Designation ist eine Entscheidung, keine wiederholte Forderung.
4. **Antennenelevation** (`RadarSlewEl`), mit Totband `kInterceptElDeadDeg` = 2,0° — ein Knopf, der alle
   zehntel Sekunden angetippt wird, wird nicht geflogen; ein Suchmuster ist ±10,5° hoch
   (`modules/f16/FBF16Fcr`), 2° liegen also gut innerhalb der Keule.
5. **Suchmodus**, EINMAL (`RadarMode`, `SearchRadarModeOrdinal()`). Ordinal < 0 = „dieses Modul hat
   keinen eigenen nicht-lockenden Suchmodus, das Gerät bleibt, wie die Mission es gesetzt hat".

---

## 8. `systems/FBEngagement` — die Zustandsmaschine als Daten und das Debriefing

**Scoreboard, nicht Gehirn.** `FBBfmTrack` trägt beide Rollen (Bild + Metrik), weil ein Tracker EIN
Objekt sein muss; hier steht das Bild bereits auf dem Bus, und was fehlt, ist ein Ort, sich zu MERKEN,
was passiert ist. `FBPilot::Run` entscheidet, diese Klasse protokolliert.

**Ereignisse, je genau einmal (erstes Vorkommen gewinnt):**

| Methode | Was sie festhält |
|---|---|
| `NoteContact(now)` | erster fester Radarkontakt auf dem bearbeiteten Ziel |
| `NoteLock(now)` | erster Single-Target-Track |
| `NoteShot(now, R, ata, aspect, raero, rtr, rmin, tta, tti)` | der Abzug MIT dem ganzen Startbereich, wie ihn die Feuerleitung in DIESEM Augenblick meldete — die Vorhersage, gegen die das geflogene Ergebnis später gemessen wird. NUR der erste Schuss beschreibt die Metrik (ein zweiter ist eine andere Entscheidung mit eigener Geometrie; Mitteln beschriebe keine von beiden), der Zähler sagt, wie viele fielen |
| `NoteThreat(now)` | erste Track-Klassen-Warnung |
| `NoteDefensiveAction(now, cueS)` | erstes Kommando als ANTWORT, gemessen ab dem CUE |
| `NoteChaff(n)` | tatsächlich ausgestoßene PATRONEN (die Zählung der Anlage, nicht die der Schalterwürfe) |
| `NoteSupport(locked, now, dt)` | ein Tick im Führungsfenster `[Start, Start + TTA]` |

**Führung wird in SEKUNDEN GEMESSEN, IN DENEN DER UPLINK TATSÄCHLICH GESPEIST WURDE**, nicht in
Sekunden seit dem Start: wer die Nase drin behält, aber den Track durch den Kardanwinkel verliert, stützt
den Schuss nicht mehr. `Pitbull` ist das abgeleitete Urteil, genau einmal am ENDE des Fensters gefällt.
Das Fenster schließt VOR der Zählung dieses Ticks, damit die Summe die Fensterlänge nie übersteigt —
`eng_support_f` ist ein ANTEIL und wird zusätzlich bei 1 gekappt (Tick 0,1 s gegen ein auf den
Integrationsschritt quantisiertes Fenster).

**Die `eng_*`-Kanäle (Quelle `eng`, 27 Spalten, ganz hinten angehängt).** Jeder ist (a) aus den EIGENEN
Instrumenten berechenbar und (b) eine Größe, über die ein echtes Debriefing streiten würde:

| Spalte(n) | Misst |
|---|---|
| `eng_state` | der Zustand aus § 7.3 |
| `eng_tgt_nm`, `eng_ata`, `eng_aspect`, `eng_clos`, `eng_locked` | die aktuelle Geometrie des bearbeiteten Kontakts (−1 = keiner) |
| `eng_detect_s`, `eng_lock_s` | **Zeit bis zur Erfassung** — wer die Antenne auf die falsche Höhe stellt, findet ihn spät oder nie |
| `eng_shot_s`, `eng_shot_nm`, `eng_shot_ata`, `eng_shot_aspect` | Schussmoment, -entfernung, -geometrie |
| `eng_shot_rtr_nm`, `eng_shot_raero_nm`, `eng_shot_rmin_nm` | der Startbereich IM MOMENT des Schusses — ein Schuss ist nur so gut wie die Geometrie, in der er fiel |
| `eng_tta_s`, `eng_tti_s` | die beiden Vorhersagen des Feuerleitrechners (bis Eigenlenkung / bis Einschlag) |
| `eng_support_s`, `eng_support_f`, `eng_pitbull` | **der Unterschied zwischen einem Start und einem Abschuss** |
| `eng_threat_s`, `eng_react_s` | **Reaktionszeit** — der eine Kanal, der rein den PILOTEN und nicht die Geometrie beschreibt |
| `eng_defend_s`, `eng_shots`, `eng_chaff` | Sekunden in der Verteidigung, Schüsse, ausgestoßene Patronen |
| `eng_es`, `eng_es_min` | Energiehöhe jetzt und ihr Minimum SEIT BEGINN DES GEFECHTS (nicht seit Laufbeginn: wer noch nicht kämpft, hat noch nichts ausgegeben) |

Alle überleben das Gefecht, deshalb IST die letzte Zeile eines Laufs das ganze Debriefing.
**Das ist die Bedeutung für die evolutionäre Runde:** weil jeder Kanal aus eigener Perspektive
berechenbar ist, bewertet die Fitness kein Wissen, das der Pilot nicht hatte — und weil sie in der
Telemetrie stehen und nicht in der Auswertung neu gerechnet werden, urteilt die Auswertung nicht über
ihre eigene Kopie der Geometrie.

---

## 9. `systems/FBPilotTuning` — die Variante als Missionsdaten

**Die Tabelle.** Ein festes Array `{Have_[], Value_[]}` über das Parameter-Ordinal: keine Allokation,
kein Map-Lookup im Entscheidungspfad. Gelesen ausschließlich über

```
double Tuned(FBPilotParam p, double own) const { return Tune_.Or(p, own); }
```

`own` ist IMMER der eigene Hook dieses Piloten — die Zahlen der Zelle bleiben damit in der Klasse der
Zelle, und die Überschreibung bleibt dünn und an der Verwendungsstelle sichtbar.

**„Nicht gesetzt ist keine Null."** Ein leerer Eintrag bedeutet „die eigene Zahl dieses Piloten". Eine
Mission ohne `pilot_*`-Zeile fliegt deshalb byte-identisch zu einer, die diese Klasse nie gesehen hat
(nachgemessen).

**Bandprüfung.** `Set(key, value)` lehnt einen unbekannten Schlüssel oder einen Wert außerhalb des
Bandes ab; `FBF16Module::ApplySetup` macht daraus einen Missions-FAIL. Eine vertippte Turnier-Zahl fliegt
also nicht still den Default. Die Bänder sind bewusst WEIT — sie fangen einen Tippfehler oder eine
Einheitenverwechslung, sie kodieren keinen Geschmack: ein Turnier darf eine schlechte Idee versuchen, es
darf nicht 6.000 nm versuchen.

| Schlüssel | Band | Entscheidet | Anmerkung |
|---|---|---|---|
| `pilot_speed_kt` | 150…900 | Abfanggeschwindigkeit (kt TAS) | |
| `pilot_lock_nm` | 1…40 | wo der Lock (und seine Warnung) ausgegeben wird | Obergrenze = das Tor des APG-68 |
| `pilot_shot_rtr` | 0,1…3,0 | Auslösen bei diesem Vielfachen von Rtr | > 1 = jenseits von Rtr |
| `pilot_shot_ata_deg` | 1…60 | wie weit off-nose noch geschossen wird | Obergrenze = Kardanwinkel |
| `pilot_shot_spacing_s` | 0…120 | Abstand zweier Schüsse auf dasselbe Ziel | |
| `pilot_crank_deg` | 0…60 | wie weit der gestützte Schuss weggedreht wird | |
| `pilot_abort_nm` | 0…40 | darunter ist der Abfang vorbei | |
| `pilot_beam_deg` | 0…180 | Verteidigungsdrehung gegen die Bedrohungspeilung | |
| `pilot_chaff_s` | 0,2…60 | Wurfintervall in der Verteidigung | |
| `pilot_defend_hold_s` | 0…120 | Haltezeit nach der letzten Warnung | |
| `pilot_react_s` | 0…30 | menschliche Reaktionszeit | **PILOTEN-Eigenschaft** |
| `pilot_action_s` | 0,1…30 | eine Bedienhandlung pro dieser Zeit | **PILOTEN-Eigenschaft** |
| `pilot_gun_burst_s` | 0,1…1,0 | Länge EINES Abzugsdrucks | Obergrenze = `core/FBGun.h`s `MaxBurstS`; die Dauer ist NICHT der Bus-Abstand |
| `pilot_gun_tol_frac` | 0,05…1,0 | wie eng der Pipper gehalten wird (Anteil der Trichtertoleranz) | |
| `pilot_bfm_ctrl_min_nm` | 0,05…5,0 | Nahkante der Kontrollposition | |
| `pilot_bfm_ctrl_max_nm` | 0,05…10,0 | Fernkante | Kanonenposition IM Trichter, Raketenposition außerhalb |
| `pilot_attack_bias_s` | −10…+10 | Abwurf um s Sekunden nach dem Cue | bewusst WEIT und VORZEICHENBEHAFTET: der Parameter für einen ABSICHTLICH falschen Abwurf |
| `pilot_attack_ccip_m` | 1…2000 | CCIP-Pipper-Toleranz | |

**Warum eine Population eine Menge von TEXTZEILEN ist.** Eine Variante ist damit eine ZEILE in einer
Missionsdatei statt einer Klasse — zwischen zwei Kandidaten wird nichts kompiliert, und im Simulator
steht kein Turnier-Code. Reaktions- und Handlungszeit liegen weiterhin ZUSÄTZLICH zur Bus-Latenz der
jeweiligen Klasse: keine Variante kann schneller antworten, als der Jet es zulässt.

### 9.1 Der Turnierläufer (`sim/tools/fb_tournament.py`)

Stdlib-Python, kein Build-Target, keine Abhängigkeit unter `sim/build` außer der `fb-gym`-Binärdatei.

- **Was er fliegt:** jedes ungeordnete Paar von Varianten, in BEIDEN Seitenzuordnungen (A west/B east und
  B west/A east), auf einer Startgeometrie. Beide Sitze zu fliegen entfernt den Positionsvorteil aus dem
  Ergebnis — die zwei Läufe eines Paares sind Spiegelbilder, die SUMME misst also die Variante und nicht
  den Sitz. Läufe gehen über `fb-gym --threads N` und sind byte-reproduzierbar (`--check-determinism`
  fliegt jede Paarung zusätzlich mit `--threads 1` und vergleicht die Telemetrie byteweise).
- **Geometrien:** `mirror` (frontal, co-altitude, co-speed, beide außerhalb des 40-nm-Suchtors bei t=0 —
  beide Läufe beginnen mit einer echten Suchphase, keiner bekommt eine Detektion geschenkt) und `split`
  (der Energie-Unterschied von `bvr-duel-decided.fbm`: 6.000 m und 150 kt — nicht „wer gewinnt einen
  gleichen Kampf", sondern „wer macht mehr aus beiden Enden eines ungleichen").
- **Auswertung** ausschließlich aus `telemetry*.csv` (die `eng_*`- und `dmg_*`-Spalten der letzten Zeile)
  plus den `UNIT_RESULT`-Zeilen aus `events.log`.

**Die Auswertungsregel: Ergebnis dominiert, Handwerk ordnet nur innerhalb gleicher Ergebnisse.**

| Posten | Gewicht | Begründung im Skript |
|---|---|---|
| `kill` | +1000 | der Sinn des Einsatzes |
| `lost` | −1200 | etwas MEHR als ein Abschuss wert — ohne die Asymmetrie wäre ein gegenseitiger Abschuss so viel wert wie ein Patt, und „immer tauschen" wäre eine stabile Strategie |
| `hits landed` | +150 je Treffer | der Anti-Schießwütig-Posten und das Einzige, wofür ein Schuss überhaupt etwas bekommt; nicht fälschbar (ein Treffer heißt, eine Runde auf zehn Meter geführt zu haben) und ein Sechstel eines Abschusses |
| `no shot` | −250 | der Anti-Weglaufen-Posten. Wer nie feuert, kann auch nicht abgeschossen werden — eine reine Überlebenswertung hat ihr Optimum im Verlassen des Gebiets. Größer als alle Handwerksposten zusammen |
| `rounds` | −25 je Schuss | eine AIM-120 ist nicht umsonst |
| `shot geometry` | ×100 | `q = zone · cos(ata)`, `zone = clamp((Raero − R)/(Raero − Rtr), 0, 1)`. 1,0 = bei oder innerhalb Rtr, direkt voraus. **Plateau** jenseits Rtr: „näher ist besser" ohne Grenze würde für das Hineindrücken bezahlen, was die Ergebnisposten längst teurer bewerten |
| `support` | ×80 | `eng_support_f` — der Unterschied zwischen Start und Abschuss |
| `shot lead` | ×40 | `tanh((t_foe − t_me)/15)`, RELATIV zum Gegner im SELBEN Lauf — ein Duell ist eine relative Sache; die tanh verhindert, dass ein 15-s-Vorsprung zehnmal so viel wert ist wie ein 5-s-Vorsprung |
| `defence` | ×40 | nur wo es etwas abzuwehren GAB und nur wenn es funktionierte: `max(0, 1 − react/8)` und überlebt. Nicht farmbar — beschossen zu werden ist die Entscheidung des Gegners |
| `energy` | ×40 | `eng_es_min / es_start`. Bewusst klein: geradeaus und waagerecht zu fliegen ist auf dieser Achse perfekt und auf jeder anderen hoffnungslos |

Die Rangliste ist der MITTELWERT über die Läufe einer Variante (wächst also nicht mit der Feldgröße) und
trennt `outcome` (kill/lost/hits) von `craft` (der Rest).

---

## 10. Der Missions-Regelkreis als Arbeitsweise

```
Mission definieren (.fbm)  →  headless simulieren (fb-gym)  →  Telemetrie maschinell analysieren
      ↑                                                                       │
      └───────────────────────── Korrektur ←──────────────────────────────────┘
```

- **Definieren:** `sim/missions/*.fbm`. Für einen Kampf zwingend `set datalink off` (sonst wäre die
  Sensorbeschränkung nur behauptet) und ein Auto-Lock-Modus (`set fcr_mode acm_hud` o.ä. — CRM lockt
  nicht von selbst). Für einen Abfang `set fcr_mode crm` und Startgeometrien außerhalb des Suchtors.
- **Simulieren:** `build/fb-gym --mission FILE --out DIR [--threads N] [--elev const|swiss]`.
  Kampf- und Abfangmissionen enden per **TIMEOUT (Exit 3)** — ein Gefecht hat kein Wegpunkt-Ziel; das
  Urteil steht in der Telemetrie, nicht im Exit-Code. Seit den Kampfzielen (`objective kill unit …`)
  kann ein Duell stattdessen SUCCESS/FAIL liefern.
- **Analysieren:** die LETZTE ZEILE trägt alle Integrale. Kanäle nach Frage:

| Frage | Kanäle / Ereignisse |
|---|---|
| Wo war der Pilot? | `phase`, `activeWp`, `distToWpM` |
| Wie lief der Kurvenkampf? | `bfm_*` (§ 6.3), `pilot GUN_TRACK`/`GUN_BREAK` |
| Wie lief der Abfang? | `eng_*` (§ 8) |
| Was hat er bedient? | `cmd_*`-Spalten, `cmd CMD_ISSUE`/`CMD_ACK`/`CMD_REJECT` |
| Was hat er gesehen? | `blk_*` (Blockgültigkeit — ein gehaltener Wert sieht sonst aus wie ein frischer), `warn_*` |
| Was hat er getroffen? | `dmg_hits`/`dmg_failed`/`dmg_degraded`/`dmg_effective`, `damage DAMAGE`/`SYSTEM`/`KILL`, `gun HIT`/`MISS`/`DRY` |
| Was hat er abgeworfen? | `pilot ATTACK_RELEASE`, `stores DELIVERY` |
| Wie ging es aus? | `UNIT_RESULT unit=… result=… reason="…" decisive=…`, `RESULT`, `SUMMARY` |

- **Regel für neue Kanäle:** IMMER hinten anhängen (`units/FBSimUnit::StartTelemetry`), damit keine je
  gemessene Spalte ihre Position verliert. `FBBfmTrack` und `FBEngagement` sind aus genau diesem Grund
  EIGENE Telemetriequellen und nicht in die drei `pilot`-Spalten hineingefaltet, die mitten in jeder
  bestehenden `telemetry.csv` stehen.

---

## 11. Piloten-Eigenschaft vs. Flugzeug-Eigenschaft

Die Trennung ist strukturell: Flugzeug-Zahlen sind **virtuelle Hooks** auf `FBPilot`, die das Modul
überschreibt; Piloten-Zahlen sind **Konstanten in `FBPilot.cpp`**, weil ein Mensch in jedem Cockpit
derselbe ist — und beide sind über `FBPilotTuning` missionsseitig überschreibbar, wo eine Variante
sinnvoll ist.

**PILOTEN-Eigenschaften** (Konstanten, keine Hooks):

| Konstante | Wert | Begründung |
|---|---|---|
| `kInterceptReactionS` | 1,0 s | die eine Zahl in dieser Datei, die einen MENSCHEN modelliert: wahrnehmen, erkennen, entscheiden, bewegen. Veröffentlichte Werte für einen geübten Piloten auf einen eindeutigen, erwarteten Reiz liegen bei 0,5–1,5 s; 1,0 s ist die Mitte. Sie liegt OBEN AUF der 0,5-s-HOTAS-Latenz — das früheste, was ein Verteidigungskommando wirken kann, ist also anderthalb Sekunden nach dem Aufleuchten. Genau dafür existiert sie: eine KI, die im selben Tick antwortet, gewinnt mit Reflexen, die sie nicht hat |
| `kInterceptActionS` | 0,5 s | dasselbe für die HÄNDE: ein Pilot bedient einen Hebel nach dem anderen, und die Avionik braucht selbst 0,5 s, um zwei Handlungen an einem Schalter zu unterscheiden |
| `kInterceptElDeadDeg` | 2,0° | Totband der Antennenhöhe, innerhalb der Keulenbreite |
| `kInterceptLostS` | 10,0 s | zwei CRM-Frames plus Marge |
| `kInterceptTrackSettleS` | 2,0 s | mehrere Filter-Zeitkonstanten + die Schalterarbeit einer echten Schusssequenz |
| `kBriefRetryS` | 2,0 s | ein abgelehnter DED-Eintrag ist eine Hand und ein Kopf, keine Schleife |
| `kBfmTurnTimeS`, `kBfmRollFullDeg`, `kBfmRollRateMaxDegS`, die PI-Gewinne | s. § 5 | Regelgesetz-Parameter, teils hergeleitet, teils gemessen |

**FLUGZEUG-Eigenschaften** (virtuelle Hooks; F-16-Werte aus `modules/f16/FBF16Pilot.h`):

| Hook | Generischer Default | F-16 | Quelle |
|---|---|---|---|
| `RotationSpeedKt(w)` | 65 | Tabelle 128…198 KIAS über 20.000…44.000 lb, interpoliert | [DOK] `procedures-takeoff-taxi.md` |
| `RotationLeadKt` / `RotationPitchDeg` | 10 / 8° | 15 kt / 10° | [DOK] „~15 kt unter Vr im AB", „8–12° Rotationslage" |
| `GearUpLimitKt` / `ClimbSpeedKt` / `TakeoffThrottleNorm` | 150 / 100 / 1,0 | 300 / 350 / 1,0 | [DOK] / Missionsprofil / [DOK] „Full Afterburner" |
| `ApproachSpeedKt` | 90 | **165** | [MESS] getrimmt waagerecht, Fahrwerk unten, ~40 % Sprit: 11,0° AoA bei 164,9 KCAS — ein offener Ersatz für eine geschlossene AoA-Schleife, treu zur Trimmkurve des Modells statt zu einer abgeschriebenen Zahl |
| `GlidepathAngleDeg` | 3° | 3° | [DOK] `navigation-ils.md` |
| `FlareTargetPitchDeg` / `AerobrakePitchDeg` / `AerobrakeSpeedKt` | 8 / 10 / 100 | 12,5 / 12,0 / 100 | [DOK] Short-Final/Roll-Out (≤13° AoA, ~13° halten bis ~100 kt) mit Marge gegen den 15°-Lagen-K.O. des Flugmonitors |
| `BfmCornerSpeedKt` / `BfmCornerG` | 300 / 4,0 | **380 / 5,6** | [MESS] `make -C sim test-corner`: 280→12,8 °/s @3,2 g \| 340→14,9 @4,6 \| **380→16,2 @5,6 (Spitze)** \| 420→14,7 @5,8 \| 500→11,7 @5,6 \| 620→12,9 @7,5 |
| `BfmMinSpeedKt` | 220 | 300 | [MESS] dort ist die Rate ~17 % unter der Spitze |
| `BfmMaxG` / `BfmUnloadG` | 6,0 / 3,0 | 9,0 / 3,0 | Strukturgrenze / [SETZ] |
| `BfmControlMinNm`…`BfmControlAtaDeg` | 0,5 / 1,5 / 30° / 30° | identisch | [SETZ] |
| `BfmClosureGainKtPerNm` / `BfmMaxClosureKt` | 120 / 200 | identisch | [SETZ] |
| `BfmLead*`, `BfmLagTimeS`, `BfmYoYoHeightM` | 45°/3 nm/4 s, 2,5 s, 400 m | dito, Yo-Yo 600 m | [SETZ] |
| `BfmScanAmplitudeDeg` / `BfmScanPeriodS` | 8° / 30 s | identisch (~1,7 °/s Weben: ein Scan, keine Kurve) | [HERL] aus `2π·A/T` |
| `BfmFloorFt` | 2000 | 2000 | [SETZ] |
| `BfmGunBurstS` / `BfmGunTrackMaxErrDeg` / `BfmGunFireTolFrac` | 0,5 s / 20° / 0,35 | keine Überschreibung → generische Werte | [SETZ], Burst-Länge = der Bus-Mindestabstand, also Dauerfeuer solange der Trichter hält |
| `SearchRadarModeOrdinal` | −1 | **1** (`FBF16FcrMode::Crm`) | der einzige F-16-Modus, der groß sucht und NICHT selbst lockt |
| `InterceptSpeedKt` | 300 | **550 kt TAS** | auf 8.000 m ≈ 375 KCAS = die gemessene Eckgeschwindigkeit; zugleich die Startgeschwindigkeit, die die Runde erbt |
| `InterceptLockRangeNm` | 20 | **16** | außerhalb jedes frontal gemessenen Rtr (~11 nm auf 8.000 m [MESS]) und innerhalb des 40-nm-Suchtors |
| `InterceptCrankAtaDeg` | 45 | **45** | [HERL] STT-Kardanwinkel ±60° minus 15° Reserve für ein manövrierendes Ziel |
| `InterceptShotAtaDeg` | 30 | 30 | eine schienengestartete Runde, der mehr als ~30° aufzuholen gegeben wird, verbraucht ihren Motor dafür |
| `InterceptAbortRangeNm` | 5 | 5 | darunter sind Rmin und der Merge dasselbe Problem |
| `InterceptShotRtrFactor` / `ShotSpacingS` / `BeamOffsetDeg` / `ChaffIntervalS` / `DefendHoldS` | 1,0 / 12 s / 90° / 3 s / 12 s | identisch | [SETZ] bzw. Notch-Geometrie |
| `AttackReleaseBiasS` / `AttackCcipTolM` | 0 / 60 m | 0 / **45 m** | [HERL] das Splittermuster einer Mk-82 nimmt eine weiche Anlage bis ~25 m aus und degradiert sie bis ~45 m (`modules/ground/FBGroundTarget.h`) — weiter daneben erreicht der Anflug nichts, und genau dann pickelt ein Pilot nicht |
| `AttackEgressTurnDeg` / `ClimbM` / `RangeM` / `S` | 120° / 500 m / 12 km / 25 s | **135°** / 600 m / 12 km / 30 s | eine Ausweichkurve deutlich hinter den Beam; 135° am Querlagenlimit sind bequem in 30 s |

---

## 12. Offene Punkte

**Bekannte Schwächen (Stand `9673e00`, jede gemessen, keine davon versteckt):**

1. **Kanone gegen den kurvenden Verteidiger: ~1 von 8 Anflügen verfehlt weiterhin.** Der Verfolger setzt
   sich innerhalb der MINDESTENTFERNUNG des Trichters fest — dort liefert die EEGS-Lösung keine
   brauchbare Feuerfreigabe mehr, während die Verfolgungsgeometrie den Jet dort hält. Fehlt: eine
   Regel, die aus „zu nah" wieder Abstand macht (die Kontrollposition beschreibt eine Bandmitte, keine
   Untergrenze mit Ausweichverhalten).
2. **Das BVR-Duell bleibt ein Patt, weil jeder Fernschuss GENOTCHT wird.** Beide Seiten verteidigen
   erfolgreich mit Beam + Düppel (`bvr-duel.fbm`: erster Schuss abgewehrt, zweiter bei t=527 s mit 43,6 m
   Fehlabstand ebenfalls). Solange keine Seite einen Schuss aus einer Geometrie beibringt, die den Notch
   ausschließt (oder eine Waffe, die ihn übersteht), ist die Paarung symmetrisch und das Ergebnis
   strukturell unentschieden. `bvr-duel-decided.fbm` zeigt die Gegenprobe: ein Energie-Unterschied
   entscheidet, das Handwerk allein nicht.
3. **Der enge Frontalpass wird nicht in eine Kurve KONVERTIERT.** Das Gesetz regelt eine PEILUNG und
   nimmt immer den kurzen Weg; genau am Heckdurchgang sind beide Wege gleich lang, das kommandierte
   Auftriebsvektor-Vorzeichen kippt, und der Regelkreis sieht keinen sich verschlechternden Fehler,
   sondern einen NEUEN Fehler auf der anderen Seite — er antwortet mit einer Umkehr. [MESS] am 385-m-Merge
   von `bfm-blind`: die Sichtlinie wanderte von +102° auf +177° und wickelte auf −177°, das Rollkommando
   klapperte über die Vertikale (94/109/97/100/107/93/105° Querlage in aufeinanderfolgenden
   Halbsekunden), statt eine Kurve zu halten. Ein Pass wird damit nicht in eine Position umgesetzt.
4. **Der Annäherungsfahrplan hat bei `9673e00` einen FLACHEN Deckel** (`BfmMaxClosureKt` = 200 kt), der
   nicht an der Bremsautorität der Zelle hängt. Der Fahrplan `c = k·(R − Rctrl)` verlangt eine
   Verzögerung `k·c`; jenseits von `a/k` schreibt der Pilot einen Scheck, den die Zelle nicht deckt.
   [MESS, `gun-bfm`] Beschleunigung auf 190 kt Annäherung bei 2 nm, danach 35 s Leerlauf + Bremsklappe
   ohne sie loszuwerden, Ankunft im Kontrollband mit 98 kt statt der gefahrplanten 5, Durchflug auf 61 m.
5. **Der Gashebel regelt eine Geschwindigkeits-DIFFERENZ, nicht die Annäherungsrate.** Beide sind nur in
   einer koaltitudinalen Heckverfolgung dieselbe Zahl. [MESS, `gun-bfm` dritter Anflug] 74 kt
   TAS-Differenz gegen 157 kt tatsächliche Annäherung, weil der Verfolger 700 m höher war und Höhe in
   Annäherung umsetzte. Die Alternative (Regelung auf die Radarmessung der Annäherung) ist gemessen und
   verworfen: Kontrollband minimal besser (21,4 % → 23,2 % der verfolgten Zeit), Trichterzeit gegen den
   geradeaus fliegenden Verteidiger brach ein (21,2 → 12,7 s) — die Annäherungsrate trägt die ganze
   Verfolgungsgeometrie, sie mit dem Gashebel zu regeln lässt den Gashebel die Kurve bekämpfen.
6. **Keine Geländemaskierung in der Sensorkette** (bewusst, `systems/FBRadarSystem`): der Pilot kann
   deshalb keine Terrain-Taktik lernen, weder aktiv noch defensiv.
7. **Fackeln wirken nicht** (kein IR-Sucher im Baum) — die Verteidigung des Piloten ist heute rein
   Chaff + Beam.

**Nachführung fällig — nebenläufige Änderung.**
Zum Zeitpunkt dieser Datei ändert ein anderer Agent `sim/src/systems/FBPilot.{h,cpp}` und
`sim/src/modules/f16/FBF16Pilot.h` (Nahkampf-Runde). Bereits in der Arbeitskopie sichtbar und hier NOCH
NICHT dokumentiert:

- ein neuer Zellen-Hook `BfmBrakeMs2()` (F-16 2,4 m/s², [MESS] 238 Proben bei 4.000 m zwischen 325 und
  400 KCAS, Median 2,39 / p10 1,64 / p90 3,80) und ein daraus **hergeleiteter** Annäherungsdeckel
  `a/k` statt des flachen 200-kt-Deckels → betrifft Punkt 4 oben;
- eine **Konversions-Regel** mit committetem Drehsinn (`BfmTurnSense_`, `kBfmConvertErrDeg` = 90°,
  Zonenbreite aus `180° − LOS-Rate · kBfmReverseS`) → betrifft Punkt 3 oben;
- der Abzug liest die vorhergesagte Ziellösung als **BETRAG** statt sie bei 0 zu klemmen.

Nach Abschluss dieser Runde sind § 5.2 (Fahrplan), § 5.7/§ 5.8 (Rolle/Abzug), § 11 (Hook-Tabelle) und
§ 12 (Punkte 3–5) gegen den dann gültigen Commit nachzuziehen. `doc/mission-format.md` trägt Teile
dieser Runde bereits — bei Abweichungen zwischen beiden Dateien gilt der QUELLCODE des jeweils
zitierten Commits.
