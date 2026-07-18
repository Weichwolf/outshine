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

## 7. Autopilot — Command Center kommandiert, iNav fliegt

**Position: Das Command Center gibt iNav nur High-Level-Missions-Kommandos; iNav führt sie NATIV
aus.** LAUNCH, RTH, Loiter, FW-Autoland, Waypoints laufen nativ in iNav 9.1.0. Der Companion-Nav-Loop
(Vector-Field-Loiter, Alt-Hold-PID, Climbout in `autopilot.c`) wird **gelöscht** — die Fluglogik darf
nicht in unserem Companion statt in iNav leben, sonst ist „im Sim bewiesen" wertlos. Auf der echten
Hardware fliegt iNavs native NAV, nicht `autopilot.c`.

*Ist (dev/inav):* der Vector-Field-Loiter ist raus; der Autonom-Zweig engagiert jetzt **natives
NAV RTH** (`aux 2 8 2` = AUX3, `nav_rth_climb_first=ON_FW_SPIRAL`, `nav_fw_loiter_radius`) als
„keine-Mission"-Default + RC-Loss-Failsafe. Der frühere Bug (LOITER flog geradeaus von Home weg)
war genau das Symptom des Nachbaus: das `gate` nullte die Heimwärts-Korrektur beim Radialflug.

| Nativ in iNav (Gehirn) | Dünner Bridge-Glue (sim-legitim) | Gelöscht |
|---|---|---|
| LAUNCH/RTH/Loiter/Autoland/Failsafe (via `inav.diff` + AUX) | Senderless Auto-Arm-Edge — kein Tx im Sim | Vector-Field-Loiter |
| Waypoints via **MSP_SET_WP über CRSF-Tunnel** (§4) | Control-Uplink → CRSF-RC + Mode-AUX; Live-Sticks direkt | Alt-Hold-PID, Climbout |

Die Band-Aids (Slew-Limiter, odir-Hysterese gegen ~200 deg/s Roll-Kicks) entfallen — sie waren nötig,
WEIL wir iNav diskontinuierliche ANGLE-Befehle fütterten. Erzeugt iNavs native FW-NAV in unserem FDM
Kicks, ist das ein **echter Bug, den wir finden wollen** (er träfe HW auch). MIL-STD-882-konform:
iNavs Failsafe ist das echte Sicherheitsnetz; kein verstecktes zweites Gehirn.

### 7.1 Kommando-Modell

Das CC spricht wenige High-Level-Befehle, alle → native iNav-NAV via MSP (§4). Nur **Live-Steuerung**
geht direkt (RC über denselben Funk, iNav in ANGLE/ACRO):

| Kommando | iNav-Mechanismus |
|---|---|
| `circle(x,y, radius, alt)` | POSHOLD / WP-HOLD-Loiter um x,y |
| `goto(gps1→gps2, alt)` | Waypoint-Mission (`MSP_SET_WP`), NAV WP |
| `land(airport, runway, heading)` | Anflug auf Bahnschwelle + FW-Autoland |
| `hold` / `rth` | POSHOLD an Ort / NAV RTH heim + Loiter |

### 7.2 AGL/ASL — Terrain lebt im Command Center

**Nur das CC kennt Höhe über Grund** (DEM); iNav hat über GPS ausschließlich Höhe-über-Null (ASL).
Jede AGL-Vorgabe rechnet das CC per **Bodenhöhe(Zielpunkt) → ASL** um, bevor sie als GPS-Höhe an iNav
geht. Terrain-Wissen bleibt oben, iNav bekommt reine GPS-Kommandos. Das ist auch das reale
HW-Analogon: der Companion-Computer trägt die Terrain-DB, der Flight-Controller nicht.

### 7.3 Missionen als Config

Eine **Mission** ist externalisierte Konfiguration (JSON), **identisch geladen von den E2E-Tests UND
dem Command Center** — dieselbe Datei fliegt im Test und in der Bodenstation:

```json
{ "aircraft": "c172p",
  "takeoff": { "airport": "<ICAO>", "runway": "<ident>" },
  "waypoints": [ { "lat": .., "lon": .., "alt_agl": .. }, … ],
  "land":     { "airport": "<ICAO>", "runway": "<ident>" } }
```

Das CC übersetzt sie in die §7.1-Kommandos (AGL→ASL je WP über §7.2) und lädt sie via MSP-over-CRSF
in iNav. **Auswahl im Command Center per URL-Parameter** — `http://localhost:8080/?mission=<name>`,
analog zum bestehenden `?ground=`; die E2E-Tests lesen dieselben Missions-Dateien direkt (ohne
Browser). Eine Mission wählt implizit auch das Flugzeug und den Start-Origin (Takeoff-Flughafen).

### 7.4 Flughafen-Datenbank

Für Start-/Lande-Bahn + **Ausrichtung** braucht es eine Flughafen-DB. **Quelle: OurAirports**
(Public Domain): `airports.csv` (ICAO/lat/lon/Elevation) + `runways.csv` (Schwellen-Koordinaten,
`le_heading`/`he_heading`, Länge). Gefilterter statischer Auszug (befestigte Bahnen), **ein Datensatz
für Tests und CC**. Liefert Bahn-Heading (Start-/Landerichtung) und Schwellen-GPS (Aufsetzpunkt).

## 8. Test & Verifikation

- **Headless End-to-End-Missionstests (das Autopilot-Orakel):** kein Rendering, keine Bodenstation-
  GUI. Jeder der drei Flieger (F-16, Cessna 172, Motorsegler) startet von **je einem eigenen
  Flughafen**, fliegt seine Missions-Wegpunkte (§7.3) und landet wieder. Assertions: Abheben
  (AGL > Schwelle), **jeder WP im Fangradius getroffen**, sauberer Touchdown nahe Ziel-Bahnschwelle
  (geringe Sinkrate, Bahn-Heading). „So schnell wie die Simulation zulässt", nicht Echtzeit — wir
  testen die Sim, nicht die Uhr. **Offene Machbarkeit (zuerst zu klären):** iNav-SITL läuft heute auf
  einem Realtime-Scheduler; schneller-als-Realtime braucht ggf. Zeitskalierung. Fallback: realtime
  (langsamer, aber vollautomatisch headless).
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

| # | Paket | Domäne | Status / Aufwand |
|---|---|---|---|
| A | Render ECEF Schritt 2 (camera-relative) | Renderer | M *(Renderer entflochten: present/render/codec-Split, native Present-Auflösung)* |
| B | Plugin-Layout + Loader + GPS-Fix-Dynamik | iNav/Config | S–M *(`profile.env`-Ansatz da; `manifest.json`/`inav.diff` offen)* |
| C | `radio_driver`-vtable (UDP) + MSP-over-CRSF Chunk/Reassembly | iNav/Funk | M |
| D | JSBSim linken + `jsbsim_adapter.cpp` | FDM | **✓ erledigt** — alle 3 Flieger fliegen via JSBSim (`FDM_ENGINE`-Switch) |
| E | EPP-Nurflügel-`physics.xml` authoren, custom-FDM als A/B-Orakel | FDM | **L** |
| F | Atmosphäre/Wetter/Thermik auf JSBSim-Winds umrouten | FDM | S |
| G | eval.py gegen JSBSim neu verankern (Struktur behalten) | Verifikation | M |
| H | NAV nativ in iNav (Companion ausdünnen) **+** truth-attitude→`--useimu` | iNav/FDM | **in Arbeit** — Vector-Field raus, NAV RTH engagiert (dev/inav); `--useimu` offen |
| I | **SITL-Zeitfrage** klären (schneller-als-Realtime) | iNav/Test | S — **Gate für E2E** |
| J | Flughafen-DB (OurAirports → statischer Auszug + Loader) | Daten | S–M |
| K | Missions-Format + Loader (JSON, Tests **und** CC) | Command | S |
| L | Command-Layer: `MSP_SET_WP` + AGL→ASL + Takeoff/Land | Command/iNav | M–L |
| M | Headless E2E (3 Flieger/Flughäfen, Takeoff→WP→Land) | Test | M |

**Cross-cutting (kein Solo-Change):** H koppelt hart — `--useimu` verlangt korrekte body-spezifische
Kräfte aus dem 6-DOF (§3↔§5), und iNavs native NAV belastet das FDM realistisch (deckt Schwächen auf —
genau der Zweck). Zuletzt und gemeinsam von selig-fdm + inav-firmware + verify-measure.

**Gesamt: L–XL.** Dominante Kosten: EPP-Nurflügel-XML-Authoring (E) + Toleranz-Neueichung (G) —
nicht das Linken. Real-Time-Budget (100 Hz JSBSim) unkritisch (FG-erprobt), trotzdem messen.
