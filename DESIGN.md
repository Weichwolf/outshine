# FlightBox — DESIGN (Soll-Architektur)

**Status:** Ziel-Definition. Nicht der Ist-Zustand. Beschreibt, wohin die Architektur konvergiert,
und warum. Ist-Berührungspunkte sind mit `datei:zeile` markiert, damit die Migration greifbar ist.

## 0. Nordstern & Prinzipien (nicht verhandelbar)

1. **Globaler Flugsimulator auf X-Plane/MSFS/DCS-Technologie, MIL-SPEC-Anspruch.** Abweichung vom
   Industriestandard ist ein **Fehler**, kein Feature — und braucht keinen Screenshot als Beweis.
2. **Reale-Hardware-Übertragbarkeit ist der Zweck.** **GPS, CRSF, MSP** sind die Interface-Constraints:
   der Sim spricht dieselben Schnittstellen wie die Hardware, damit „im Sim bewiesen" real gilt und
   der HW-Umstieg bodenseitig ein **Treiber-/Config-Change** ist, kein Code-Change.
3. **iNav ist das Gehirn, nicht das ganze Flugzeug.** Die *echte* iNav-9.1.0-Firmware läuft in der
   Schleife (SITL) und ist die Autorität für Steuerung, Fusion, NAV, Failsafe. Wir bauen *um* iNav
   herum, nie *statt* iNav.
4. **Flugzeuge sind Plugins:** ein Flugzeug = iNav-Config (Steuerung) **+** Physik-Config (Aerodynamik/
   Masse/Antrieb). Ein Verzeichnis pro Modell, F-16 bis EPP-Nurflügel.
5. **Physik nicht neu schreiben.** Aerodynamik, Thermodynamik, Wetter, Atmosphäre kommen aus
   **etablierten Open-Source-Libraries**. Eigener Code nur an den Nähten (Glue) und wo nachweislich
   kein Library-Äquivalent existiert (dann benannt, nicht versteckt).

## 1. Die drei Schnittstellen

Der ganze Aufbau organisiert sich um **einen Seam** und **zwei Grenzen**. Sie sauber zu halten ist die
Architektur.

```
        BODEN                          GRENZE A               AIRCRAFT-CONTAINER
  ┌─────────────────┐             (Funk: Boden↔Aircraft)  ┌──────────────────────────────────────┐
  │ Command Center  │◄───── control up / telem down ─────►│  fb_airframe (Physik+Glue)   iNav SITL│
  │ (Pilot, SVS/HUD)│         UDP  ⇄  ELRS/CRSF           │        └──── GRENZE B ────┘  (9.1.0)  │
  └─────────────────┘                                     └──────────────────────────────────────┘
                                                             SEAM = state_t S (sim_state.h)
```

| | **Seam: `state_t S`** | **Grenze A — Funk** | **Grenze B — Sensor/Aktor-Bus** |
|---|---|---|---|
| Was | der Flugzustand (Lage/Rate/Pos/Speed/Control) | Pilot-Control hoch, Telemetrie runter | iNav liest Sensoren, schreibt Servos |
| Sim | von der Physik-Engine gefüllt | UDP `ctrl_packet_t`/`telem_packet_t` | X-Plane DREF :49000 + MSP :5760 |
| HW | — (nur im Sim) | ELRS 2,4 / CRSF + Analog-Video | lokale IMU/GPS/Baro + PWM/DShot |
| Swap | — | **`radio_driver`-vtable** (§4) | **nichts auf iNav-Core-Seite** (§5); im Sim verschwindet `fb_airframe` ganz |

**Kernaussage:** Alles stromabwärts von `S` (`bridge/xp_link.c`, `msp.c`, `telemetry.c`, `autopilot.c`)
ist agnostisch, *wer* `S` füllt. Deshalb ist der Physik-Wechsel (§3) ein Füller-Tausch, kein Umbau.
Grenze A swappt per Treiber; Grenze B swappt auf iNav-Seite **gar nicht** — der SITL↔HW-Wechsel ist
iNavs eigenes Build-Target, unsere Aufgabe ist nur, die Sensoren so einzuspeisen, dass iNavs *echter*
Code-Pfad läuft.

## 2. Rendering — ECEF + camera-relative *(in Umsetzung)*

Der globale Standard (X-Plane/MSFS/DCS): Planet auf **WGS84-Ellipsoid (ECEF)**, **camera-relative**
gezeichnet (Kamera-ECEF in double abgezogen, float-Offsets an die GPU → Präzision überall am besten an
der Kamera). Der heutige flache Home-Tangentialebenen-Renderer (fixer Origin, keine Krümmung, Kamera
nicht im Ursprung) ist ein **bekannter Defekt** in Ablösung.

- **Schritt 1 (fertig, Commit `5f01c1e`):** osmmesh liefert Terrain in per-Tile lokalem ECEF
  (`double origin_ecef` + float-Offsets + ECEF-Normalen); WGS84 geo↔ECEF unit-getestet.
- **Schritt 2 (in Arbeit):** camera-relative MVP; Attitude über die lokale-ENU→ECEF-Rotation an
  Kamera-lat/lon; Horizont-Dip fällt aus ECEF. ENU-Pfad bleibt umschaltbar bis verifiziert.

Details: `command_center/*` (renderer-gfx). GPS-Position (Grenze B) und ECEF-Welt teilen dieselbe
WGS84-Geodätik.

## 3. Physik-Engine — JSBSim

**Position: JSBSim** (LGPL 2.1), nativ im Aircraft-Container. Erfüllt alle harten Constraints
gleichzeitig: etablierte OSS (NASA/FlightGear/ArduPilot-erprobt), XML-Aircraft-Verzeichnis **ist**
das „Flugzeug = Physik-Plugin"-Modell, **F-16 ships fertig**, **Dryden/von-Kármán-Turbulenz nach
MIL-STD-1797** eingebaut. Verworfen: YASim (FG-gebunden; bleibt Fallback-Autorenmethode für
Nurflügel ohne Koeffizienten), Gazebo (overkill), ArduPilot-SITL (nicht einbettbar), RealFlight
(proprietär → verletzt OSS-Constraint).

```
 iNav SITL ─DREF ctrl─► xp_link.c ─► fdm/jsbsim_adapter.cpp ─► libJSBSim (6-DOF, FGAero,
 (--sim=xp)             (unveränd.)   S.in_* → fcs/*-cmd-norm     FGPropulsion, FGWinds/Dryden,
     ▲   sensor DREF                  props   → state_t S          FGStandardAtmosphere)
     └───────────────── xp_link.c ◄── (SIGN/UNIT/FRAME glue) ◄──── property tree
```

- **`fdm/jsbsim_adapter.cpp` ersetzt `physics_step`** (`xp_bridge.c:114`). Eingang `S.in_*` →
  `fcs/*-cmd-norm`; `FGFDMExec::Run()` bei `SetDt(0.01)` (100 Hz bleibt, JSBSim sub-steppt RK);
  Property-Tree → `S.*` inkl. `g_nz` (`accelerations/Nz`) und Rotations-Turbulenz.
- **Toolchain-Folge:** Aircraft-TU wird C++ (oder `extern "C"`-Shim), `Containerfile` + `g++`/
  `libJSBSim`. WASM bleibt N/A (nur der Renderer ist WASM).
- **Custom-FDM bleibt** als `FDM_ENGINE=selig|jsbsim`-Laufzeitschalter (beide schreiben `S`) — ein
  *unabhängiges* zweites Modell ist der beste Cross-Check gegen Adapter-Frame-Bugs. Erst nach
  dauerhafter Parität zum Referenz-Orakel degradiert, nie tot-committed.

**Atmosphäre/Wetter:** `FGStandardAtmosphere` + `FGWinds` (stationärer Wind aus `weather.c` geslewt in
`atmosphere/wind-*-fps`; Turbulenz `ttMilspec`/`ttTustin`). **Thermik ist der einzige custom-Rest**
(keine Soaring-Atmosphäre in JSBSim) → als vertikale Windkomponente injiziert, kein Aero-Rewrite. Der
Böen-α-Bump und der „nachhaltige-Steig-in-Bodengeschwindigkeit"-Split werden gratis korrekt, weil
JSBSim α luftrelativ aus dem echten Windfeld rechnet.

## 4. Grenze A — Funk-Treiber

**Position: `radio_driver`-vtable**, drei logische Kanäle, ein Header, zwei Implementierungen:

```c
struct radio_driver {
    int  (*rc_recv)(uint16_t ch[16]);          // RC hoch: 16 Kanäle, CRSF-11-bit-Range
    int  (*msp_xfer)(msp_frame*, msp_frame*);  // MSP-Tunnel bidirektional, gechunkt
    void (*telem_send)(const telem_frame*);    // Telem-Trickle runter, ratenlimitiert
};
radio_udp   // Sim  (UDP 6001/6002)
radio_crsf  // HW   (ELRS/CRSF UART)
```

**CRSF semantisch nachbilden, den Draht idealisieren** — Kriterium „was muss stimmen, damit ‚im Sim
bewiesen' real gilt":

| CRSF-Eigenschaft | Modellieren | Warum |
|---|---|---|
| RC-Kanal-11-bit-Quantisierung | **ja** | iNav sieht auf HW quantisierte Kanäle; echtes Verhalten |
| **MSP-over-CRSF Chunk/Sequence-Reassembly** | **ja, verpflichtend** | `MSP_SET_WP` (21 B/WP) ist IMMER mehr-Frame gechunkt; wer das im Sim fährt, lädt eine bewiesene Mission **1:1** auf HW. Sonst ist der HW-Pfad ungetestet. |
| Telem-Bandbreite | **ja, als Raten-Cap** | schmaler geteilter Rückkanal; als Scheduler modellieren, nicht bit-genau |
| CRSF-Bit-Packing des Draht-Frames | nein | reines Transportdetail, gehört dem Treiber |

**Der MSP-Tunnel ist die Config-/Missions-Konsole des Command Centers — nicht nur Sticks+Viewer.**
Über `msp_xfer` programmiert das Command Center iNav live über denselben Kanal wie Waypoints:
Waypoints (`MSP_SET_WP`), **Logic Conditions + Global Functions** (`MSP2_INAV_SET_LOGIC_CONDITIONS`
/`_GLOBAL_FUNCTIONS` — z.B. die Motorsegler-Automatik „steig auf X → Motor aus → gleite → Motor an"),
Einzel-Settings (`MSP2_COMMON_SET_SETTING`), Mode-Box-Zuweisung (`MSP_SET_MODE_RANGE`). **Defaults
leben im Plugin** (`inav.diff`, §6), **live-tunebar** über den Tunnel. Logic Conditions sind
mehr-Chunk → das gechunkte MSP-over-CRSF-Reassembly ist genau dafür load-bearing (lädt man
pre-flight/gelegentlich, nicht kontinuierlich).

**Zwei Downlink-Naturen — ehrlich trennen:** (a) **CRSF-Telem-Trickle** (bandbreitenbegrenzter
Sensor-Rückkanal — HW-Paritäts-Pfad); (b) **Full-State-SVS-Feed** (der fette Zustand, der die
synthetische Sicht im Command Center treibt — Sim-/Digital-Twin-Bequemlichkeit, **kein**
Hardware-Analog). Beide behalten, beide labeln: das Command Center ist eine **Synthetic-Vision-
Bodenstation**, keine FPV-Brille. Auf der Ziel-Hardware (Analog-NTSC + Caddx) brennt iNavs
**MAX7456-OSD** die HUD ins Video (Commit `5e9aa2f` emuliert diesen Font-Atlas bereits).

## 5. Grenze B — Sensor/Aktor-Bus

Die DREF-Schnittstelle **ist** die richtige SITL-Bus-Abstraktion — aber **truth-attitude ist die
falsche Fidelity-Stufe**.

| Stufe | Was | iNav-Estimator | Ziel? |
|---|---|---|---|
| (a) truth-attitude DREF *(heute)* | Lage geschenkt aus phi/theta/psi | **umgangen** | nein |
| **(b) `--useimu` / Sensor-Level-DREF** | rohe body-accel + P/Q/R (+ mag), iNav fusioniert selbst | **voll** | **ja** |
| (c) echte Sensor-Emulation (UBX/Register) | iNavs reale Bus-Treiber | + Parsing | nein (over-engineering; SITL stubbt Treiber bewusst) |

**Position: (b).** Der Wechsel (a)→(b) testet genau den Code, der Sim von HW unterscheidet
(Attitude-Fusion). Ehrliche Kopplung zu **§3/selig-fdm**: `g_axil`/`g_side` (heute hart 0,
`xp_link.c:27-28`) müssen dann korrekte **body-frame spezifische Kräfte** aus dem 6-DOF sein, sonst
driftet iNavs Lage. Kein Solo-Change.

**GPS hardware-nah = Fix-Dynamik, nicht Wire-Format.** Statt `numSats=16, fix=2` hart
(`xp_link.c:44-45`): ein simples GNSS-Modell mit **fixType/numSats/HDOP**, Cold-Start-Rampe
(0→3D über ~30 s), Degradation bei hohem Bank/Abschattung. Das feuert die realen Pfade
`nav_extra_arming_safety` und GPS-Loss→Failsafe, die heute nie greifen. Kein NMEA/UBX-Strom — der
Wert liegt in der Fix-**Dynamik**. (Vorsicht Enum: `GPS_FIX_3D == 2`, nicht 3.)

## 6. Flugzeug = Plugin

```
aircraft/<name>/
  manifest.json    # display_name, chanmap, arm-sequence, gnss/imu-model-params
  inav.diff        # CLI `diff` über iNav-Target-Defaults → baut eeprom (reviewbar, driftfrei)
  physics.xml      # JSBSim: metrics/mass_balance/propulsion/flight_control(ELEVON!)/aerodynamics
  engine/<motor>.xml
```

- **`inav.diff`, nicht Full-Dump:** reviewbar, HW-reproduzierbar; `eeprom.bin` wird im Image-Build
  regeneriert, **nie** von Hand editiert.
- **`chanmap` gehört ins manifest, nicht in `run.sh`** — sie ist per-Airframe. `run.sh` liest
  `AIRCRAFT=<name>` → manifest → eeprom+chanmap → startet SITL. **Bridge-Code konstant, nur Configs
  swappen.**
- **F-16 vs EPP-Nurflügel** sind nur verschiedene `(inav.diff, physics.xml, manifest)`-Tripel:
  Nurflügel ruderlos (Elevon-Mix, `in_yaw` tot), F-16 mit echtem Ruder — Mixer + `physics.xml`
  fangen es. **Die Nurflügel-Koordinations-Lektion wird zur Config-Assertion:** ein
  `physics.xml` ohne echte Richtungsstabilität (`Cnβ`/`Cnr`, Dihedral `Clβ`) fliegt unter JSBSim
  genauso unkoordiniert — `coordination(sign)` testet künftig „steht die Stabilität im XML", nicht
  „ist der Gier-Code richtig".
- **Modelle mit eigener FBW/Autopilot → deren Regler UMGEHEN, iNav ist der einzige Controller**
  (Prinzip 3: „iNav ist das Gehirn"). Die JSBSim-F-16 ist eine echte FLCS: `fcs/*-cmd-norm` sind
  Raten-/G-*Sollwerte* in ihre eigene innere Ratenschleife, kein Direkt-Ruder. iNav drauf = zwei
  genestete Rate-Loops (iNav außen, FLCS innen) → Kopplung/Limit-Cycle. Fix: das eingebaute
  `fcs/fbw-override=1` überbrückt die FLCS in Roll+Pitch → Ruder direkt aus `*-cmd-norm` (konventionelles
  RC-Verhalten). **Integrations-Default für solche Modelle = FBW aus.** (F-16-Kante: Yaw hat keinen
  Override — läuft immer durch die FLCS; für unseren Zweck unkritisch.)
- **Lizenz per Aircraft-Datei, nicht pauschal.** JSBSim (Library) = LGPL 2.1 (§3), aber jede
  Aircraft-XML trägt ihre EIGENE Lizenz: `minisgs`/die meisten = LGPL; **die F-16 (Erik Hofman) = GPL**.
  Getrennte Datendateien neben LGPL-Library sind ok (keine Ableitung), aber die Attribution muss die
  jeweilige Aircraft-Lizenz ausweisen (F-16: GPL; Daten NASA TP-1538 u.a.).
- **`<output>`-CSV in gezogenen Modellen entfernen** (die F-16-XML schreibt pro Lauf `f16_datalog.csv`)
  — für den eingebetteten Betrieb kein FS-Müll; bei der Integration (Paket D) raus/ignorieren.
- **Grenze:** iNav-SITL ist ein Binary mit einem eeprom → Airframe-Wechsel = Config-Swap +
  Container-Neustart. Kein Live-Multi-Airframe; passt zur Container-Architektur.

## 7. Autonomie/NAV — nativ in iNav

**Position: LAUNCH, RTH, Loiter, FW-Autoland, Waypoints laufen NATIV in iNav 9.1.0.** Der
Companion-Nav-Loop (`autopilot.c:73-118` Vector-Field-Loiter, `:68-72` Alt-Hold, `:42` Climbout)
wird **gelöscht**. Begründung: der Architekturzweck — echte Firmware, HW-übertragbar — ist
unterminiert, wenn die Fluglogik in unserem Companion statt in iNav lebt. Auf der echten AR-Wing
fliegt iNavs native NAV, nicht `autopilot.c`.

| Nativ in iNav (Gehirn) | Dünner Bridge-Glue (sim-legitim) | Gelöscht |
|---|---|---|
| LAUNCH/RTH/Loiter/Autoland/Failsafe (via `inav.diff` + AUX) | Senderless Auto-Arm-Edge (`autopilot.c:19-28`) — es gibt keinen Tx im Sim | Vector-Field-Loiter |
| Waypoints via **MSP_SET_WP über CRSF-Tunnel** (§4) | Control-Uplink → CRSF-RC + Mode-AUX | Alt-Hold-PID, Climbout |

Die Band-Aids (Slew-Limiter, odir-Hysterese, die die ~200 deg/s Roll-Kicks töteten) entfallen —
sie waren nötig, WEIL wir iNav diskontinuierliche ANGLE-Befehle fütterten. Erzeugt iNavs native
FW-NAV in unserem FDM Kicks, ist das ein **echter Bug, den wir finden wollen** (er träfe HW auch).
MIL-STD-882-konform: iNavs Failsafe ist das echte Sicherheitsnetz; kein verstecktes zweites Gehirn.

## 8. Test & Verifikation

- **Struktur-Invarianten bleiben und werden stärker als Wahrheit:** `coordination(sign)`,
  `coord-turn-rate ≈ g·tan(φ)/V`, `vs ≤ airspeed`, `gs = dPos/dt`, Nicht-Divergenz — Physikgesetze,
  denen JSBSim per Konstruktion gehorcht.
- **eval.py wechselt die Rolle:** vom Physik-Validator zum **Glue-/Frame-Regressionstest**. JSBSim
  ist selbst validiert; die neue dominante Fehlerklasse ist **Sign/Unit/Frame im Adapter** (ft↔m,
  fps↔m/s, NED↔ENU, das X-Plane-`theta`-Vorzeichen). Toleranzen gegen eine **JSBSim-Referenztrace
  neu verankern** (nicht die alten custom-Zahlen behalten), Sign/Unit-Guards ergänzen.
  **verify-measure ist Eigentümer** dieser Neuverankerung — kein Fix gilt grün, bevor die Suite neu
  verankert *und* bewiesen ist, dass sie noch beißt (Mutation).
- **Verifikation per Konstruktion** wo möglich (ECEF-Mathe, geo↔ECEF Round-Trip, Attitude-Frame) —
  kein Screenshot, um Bekannt-Falsches zu „beweisen".

## 9. Migration & Kopplung

Reihenfolge (Fallback bleibt lauffähig, bis der neue Pfad grün ist):

| # | Paket | Owner | Aufwand |
|---|---|---|---|
| A | Render ECEF Schritt 2 (camera-relative) | renderer-gfx | M *(läuft)* |
| B | Plugin-Layout + Loader (`aircraft/<name>/`, `run.sh`) + GPS-Fix-Dynamik | inav-firmware | S–M |
| C | `radio_driver`-vtable (UDP) + MSP-over-CRSF Chunk/Reassembly | inav-firmware | M |
| D | JSBSim linken + `jsbsim_adapter.cpp` (ein Modell, `FDM_ENGINE`-Switch) | selig-fdm | M |
| E | EPP-Nurflügel-`physics.xml` authoren (F-16 frei), custom-FDM als A/B-Orakel | selig-fdm | **L** |
| F | Atmosphäre/Wetter/Thermik auf JSBSim-Winds umrouten | selig-fdm | S |
| G | eval.py gegen JSBSim neu verankern (Struktur behalten) | verify-measure | M |
| H | NAV nativ in iNav (Companion ausdünnen) **+** truth-attitude→`--useimu` | inav-firmware **+** selig-fdm | M–L |

**Cross-cutting (kein Solo-Change):** H koppelt hart — `--useimu` verlangt korrekte body-spezifische
Kräfte aus dem 6-DOF (§3↔§5), und iNavs native NAV belastet das FDM realistisch (deckt Schwächen auf —
genau der Zweck). Zuletzt und gemeinsam von selig-fdm + inav-firmware + verify-measure.

**Gesamt: L–XL.** Dominante Kosten: EPP-Nurflügel-XML-Authoring (E) + Toleranz-Neueichung (G) —
nicht das Linken. Real-Time-Budget (100 Hz JSBSim) unkritisch (FG-erprobt), trotzdem messen.
