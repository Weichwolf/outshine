# FlightBox — Minimalistischer FPV-Nurflügel mit Hybrid-Steuerung

**Projekt-Spezifikation · Lernprojekt klassische Aviation · Stand 2026-07-13**

Ein billiger FPV-**Nurflügel** (Flying Wing) mit eigenem „Gehirn", ausgelegt als
**Long-Duration Slow Flyer**: autonomer Handstart ohne Sender (einschalten,
Start-Taste am Flieger, werfen), dann selbstständiger Steigflug und lange,
langsame Flüge. Der Pilot fliegt normal; **bei Signalverlust fliegt das Modell
selbst heim (RTH) und kreist wie ein Vogel über dem Piloten**, jederzeit
übernehmbar. Nachtflug ist der Auslegungsfall. Analog dort, wo es real gewinnt;
digital, wo es billiger oder besser ist.

---

## Phasen

**Phase 1 (aktueller Fokus) — fliegen mit Standardkomponenten:**
- **Flieger** aus Standard-FPV-Teilen (integrierter F405-FC + iNav, ELRS-RX, GPS,
  Analog-Kamera + VTX) — steckbar, **kein Custom-PCB**.
- **Flightbox** (Bodenstation): ELRS-Sender + 5,8-GHz-Kameraempfänger + WLAN —
  Pi-Software (Video-Stream + Control-Bridge + Telemetrie).
- **Command Center** (WASM/Browser): Video, Game-Controller, Telemetrie-HUD/Karte.

**Phase 2 (optional, später):** Video-Scrambler + Descrambler (§2.9) · analoge
Rückfallebene Rate-Damper + Watchdog (§2.3, A12).

Autonomie, RTH, Nachtflug und Navigation gehören zu **Phase 1** und laufen über
iNav-Konfiguration.

---

## 1. Anforderung

| # | Anforderung |
|---|---|
| A1 | **Airframe:** minimalistischer Nurflügel, 2 Elevon-Servos, Pusher-Motor, Kamera in der Nase |
| A2 | **Manueller Flug:** Pilot steuert über **2,4-GHz-ELRS** (FHSS); Steuerung normal und direkt reagierend |
| A3 | **Fly-by-Wire:** Knüppel liefert *Korrekturwünsche*, kein direkter Ruderausschlag; das Gehirn validiert und schützt den Flugbereich |
| A4 | **RTH-Failsafe:** bei Signalverlust fliegt das Modell autonom **per GPS heim** und kreist über Home; der Link kehrt beim Näherkommen meist von selbst zurück — jederzeit sofort übernehmbar |
| A5 | **Link-Sicherheit:** ELRS **FHSS + Binding** — ein Störer kann den Link *verweigern/stören*, aber ohne Binding nicht *übernehmen*; bei Verlust greift RTH |
| A6 | **Video:** analoge Low-Light-Kamera → 5,8 GHz → Monitor am Boden; niedrige Latenz, nachttauglich |
| A7 | **HUD:** künstlicher Horizont, Höhe, Kurs, Akku — **plus GPS-Home-Indikator (Richtung + Distanz) und Anflugwinkel**; bringt den Piloten **auch ohne Sicht** heim (Instrumenten-Heimflug). **Gerendert auf der Flightbox** (WASM-Overlay aus ELRS-Telemetrie) |
| A8 | **Landung:** manuell durch den Piloten, geführt von den HUD-Indikatoren (kein Autoland nötig) |
| A9 | **Video-Zugangsschutz:** Bild verwürfelt, sodass es **nicht sofort als Videosignal erkennbar** ist und **kein zufälliger FPV-Nutzer mitschauen** kann (Sync-Suppression + PRNG, wie klassisches Pay-TV) — kein Anspruch auf starke Krypto — **Phase 2** |
| A10 | **Nachtflug:** primärer Betriebsfall — Sensorik und Führung müssen nachts funktionieren |
| A11 | **Kosten:** Fluggerät ≤ 150 CHF (Bodenausrüstung separat) |
| A12 | **Robustheit:** eine analoge Rückfallebene hält das Flugzeug fliegend, auch wenn das digitale Gehirn ausfällt — **Phase 2** |
| A13 | **Autonomer Handstart ohne Fernsteuerung:** einschalten, Start-Taste am Flieger, werfen; der DSP erkennt den Wurf und steigt selbstständig auf ein paar hundert Meter, dann kreisen — der Sender ist erst zur Übernahme nötig |
| A14 | **Long-Duration Slow Flyer:** effizient und gering flächenbelastet, langsame Reise, lange Flugzeit (Li-ion-Antrieb, großer langsamer Prop) |
| A15 | **Wegpunkt-Navigation (Steerpoint-Prinzip wie F-16):** kleine Steerpoint-Liste im DSP; HUD-Steering-Cue zum aktiven Punkt (Bearing/Distanz/Time-to-go); Home = Steerpoint 1. Pilot fliegt den Cue, optional DSP-Autoroute |
| A16 | **Flightbox (Herzstück):** eigenständige Box — **ELRS-2,4-Sender** + **5,8-GHz-Video-RX** + **Webserver** (serviert die WASM-App) + WebRTC-Videostream + WLAN; rendert auch das **HUD** aus ELRS-Telemetrie |
| A17 | **Command Center (WASM):** von der Flightbox servierte Browser-App — Video + HUD/Moving-Map ansehen, mit **Game-Controller** steuern (jedes Gerät im WLAN wird zum Cockpit) |

---

## 2. Konzept

### 2.1 Leitprinzip

> **Analog nur, wo es wirklich gewinnt** — Latenz, Nacht-Low-Light,
> MCU-unabhängiges Überleben. **Digital überall, wo es billiger oder besser
> ist** — Steuerlink, Lagefusion, Validierung, Anzeige-Logik.

Ergebnis: ein **digitales Gehirn mit digitalem Störschutz-Funk, analogen Augen
und einem analogen Sicherheitsnetz.**

### 2.2 Airframe

Schaum-Nurflügel (Foamboard/EPP), als **Slow Flyer** ausgelegt: gering
flächenbelastet und effizient, eher große Spannweite (~900–1200 mm), Pusher-Prop
hinten, Kamera in der Nase. Zwei Ruder = **Elevons**; das Mischen
(`links = Pitch + Roll`, `rechts = Pitch − Roll`) macht das Gehirn digital.
Keine Seitenleitwerks-Mechanik, minimale Teile.

### 2.3 Steuerungs-Architektur (Fly-by-Wire)

Der Pilot äußert **Absicht**, das Gehirn hat **Autorität**:

```
 ELRS 2,4-RX ─► [ GEHIRN (F405) ] ─► Elevon-Servos
   (CRSF)         │  CRSF-Decode (+ ELRS-Telemetrie zurück)
                  │  Envelope-Schutz (kein Stall/Overbank/Overrate)
                  │  Sensorfusion (Lage/Höhe/Kurs)
                  │  Flugzustands-Logik (§2.4)
                  │  HUD-Rechnung (§2.6)
                  ▼
   Gyro (analog, µs) ─► ⊕ analoger Rate-Damper ─► Servo   ◄── A12: läuft weiter,
   Watchdog fällt aus ─► MUX friert Trim ein + Rate-Damping    wenn das Gehirn hängt
```

Der DSP liefert *validierte Sollwerte* (~50 Hz); die schnelle Dämpfung bleibt
analog und immer live. Fällt der DSP aus, hält die analoge Ebene Trim +
Ratendämpfung → das Flugzeug fliegt stabil weiter (Direct-Law-Prinzip im
Kleinformat).

*Die analoge Reversion ist **Phase 2**; Phase 1 verlässt sich auf den
iNav-eigenen Failsafe (RTH/Loiter).*

### 2.4 Flugzustände

```
   [ARMED] ── einschalten · Start-Taste am Flieger · werfen
      │        (Motor bleibt aus, bis der Wurf erkannt ist — Sicherheit)
      ▼
   [AUTO-CLIMB] ── Wurf erkannt → Motor an, Steigflug auf Zielhöhe, Flügel level
      ▼
   [LOITER] ◄──── kreist per GPS über Home, hält Höhe, wartet ──────┐
      │                                                             │ Link kehrt
      │  Pilot übernimmt jederzeit                                  │ zurück
      ▼                                                             │
   [MANUAL] ── normaler Flug ── Signalverlust ──► [RTH] ────────────┘
      │                                            fliegt per GPS heim
      ▼                                            (Link kommt unterwegs
   [APPROACH] ── HUD: Home + Anflugwinkel            meist von selbst zurück)
                 → manuelle Landung
```

- **ARMED:** einschalten, Start-Taste am Flieger, werfen. Der Motor bleibt aus,
  bis der DSP den Wurf erkennt (Sicherheit — kein Prop in der Hand).
- **AUTO-CLIMB:** Accel erkennt den Wurf → nach kurzer Verzögerung Motor an,
  stabilisierter Steigflug auf Zielhöhe (ein paar hundert Meter) — ganz ohne Sender.
- **LOITER:** kreist per **GPS** exakt über den Home-Koordinaten, hält Höhe
  (Baro); der Warte-/Ausgangszustand. Jederzeit übernehmbar.
- **MANUAL:** Pilot übernimmt (validierte Korrekturwünsche, Envelope-geschützt).
- **RTH (Failsafe):** bei Signalverlust fliegt der DSP autonom **per GPS Richtung
  Home.** Weil 2,4 GHz Sichtlinie ist, **kommt der Link beim Näherkommen meist von
  selbst zurück** → der Pilot steuert sofort weiter; sonst erreicht das Modell
  Home und geht in LOITER. So fängt RTH die kürzere 2,4-GHz-Reichweite auf.
- **APPROACH:** das HUD führt mit Home-Richtung und Anflugwinkel zur manuellen Landung.

### Autonomer Handstart (ohne Fernsteuerung) & Slow-Flyer-Auslegung

Der Start läuft **komplett auf dem Flieger, ohne Sender**: einschalten,
**Start-Taste** am Modell drücken, werfen. Danach fliegt der DSP autonom.

- **Ablauf:** Start-Taste → `[ARMED]` (Anzeige-LED/Piepser, Motor AUS). Wurf →
  der DSP erkennt den **Accel-Spike + Fahrtaufbau**, wartet ~0,5–1 s (Modell aus
  der Hand — **kein Prop in der Hand**), dann Motor an, **stabilisierter Steigflug**
  (Flügel level per Fusion, fester Steigwinkel + Leistung) auf Zielhöhe (paar
  hundert Meter, Baro). Oben → `[LOITER]`, kreist und wartet.
- **Wurf-Erkennung:** robuster Accelerometer-Trigger (das „Launch-Mode"-Muster
  gängiger Autopiloten) — Schwelle überschritten → Sequenz startet. Die vorhandene
  Fusion (ICM-42688 + F405) macht das ohne Zusatzhardware.
- **Slow Flyer hilft:** niedrige Stallgeschwindigkeit → ein normaler fester Wurf
  reicht über die Mindestfahrt; der Steigflug ist gutmütig.
- **Ohne Sender-Autorität:** der Pilot muss zum Start nichts tun — er greift erst
  ein, wenn er will (`[MANUAL]`). Bis dahin und bei Signalverlust fliegt/kreist das
  Modell selbstständig. Das ist der stärkste Ausdruck des „Gehirn im Modell".
- **Antrieb:** effizienter **Li-ion-Slow-Flyer** (lange Flugzeit); der Handstart
  braucht keine hohe C-Rate — Wurf + gutmütiger Steigflug genügen.

### 2.5 Funk & Link

- **Uplink (Steuerung): ExpressLRS 2,4 GHz.** FHSS-Spread-Spectrum, in CH bis
  **100 mW EIRP legal** (RLAN-Zuweisung RIR1010, **kein Duty-Cycle-Limit** — passt
  auch zum aktiven Handfliegen der Landung). Der ELRS-RX gibt **CRSF** (seriell) an
  den F405. Billig (~10 CHF), winzige ~3-cm-Antenne, **Telemetrie-Rückkanal
  inklusive** (RSSI/GPS/Akku) — kein zweites Band nötig.
- **Downlink (Video): 5,8 GHz analoges FM-Video**, 25 mW EIRP (RIR1008-12).
  Niedrige Latenz, graceful degradation.
- **Telemetrie: über den ELRS-Rückkanal** (bidirektional) — Lage/GPS/Akku/RSSI
  kommen als **Daten** zur Flightbox. Der analoge VTX hat keinen Telemetriekanal
  und braucht auch keinen.
- **Warum 2,4-GHz-Digital statt 35-MHz-Analog:** robust/störsicher/billig schlägt
  hier den Klassik-Charme. FHSS ist deutlich jamming-fester als 35-MHz-Schmalband,
  hat kein Duty-Problem und eine winzige Antenne. Der einzige Nachteil (2,4 ist
  Sichtlinie, kürzere Reichweite) wird durch **RTH bei Signalverlust** aufgefangen
  (§2.4) — fliegt er zu weit, dreht er um und gewinnt den Link zurück.
- **„Verweigern, nicht übernehmen" — von ELRS ab Werk:** FHSS macht gezieltes
  Jamming schwer und graceful (Link-Quality sinkt, kein Total-Hijack); das
  **ELRS-Binding** verhindert, dass ein fremder Sender die Kontrolle bekommt. Kein
  Milspec-Krypto, aber genau richtig fürs Bedrohungsmodell (wie beim
  Video-Zugangsschutz). Bei Linkverlust greift ohnehin RTH → das Modell bleibt sicher.

### 2.6 HUD — mit Home-Indikator und Anflugwinkel

Das HUD rendert die **Flightbox / WASM-Command-Center** als **Overlay übers
Video**, gespeist aus der **ELRS-Telemetrie** (Lage/GPS/Akku) — **kein
Onboard-OSD-Chip nötig** (Phase 1). Angezeigt:

- **Künstlicher Horizont** (aus der Lagefusion), **Höhe** (Baro), **Kurs**, **Akku**.
- **HOME-Indikator:** ein **Pfeil** relativ zur Nase zur Heimatrichtung, plus
  **exakte Distanz** — beides aus **GPS** (Home-Koordinaten beim Arming gesetzt).
  Führt dich heim **auch ohne Sicht** (Instrumenten-Heimflug).
- **Anflugwinkel-Anzeige (Glideslope):** das Gehirn rechnet den nötigen
  Sinkwinkel zur Heimat
  `γ = arctan( Höhe / GPS-Distanz )`
  (jetzt **präzise**, da GPS die Distanz liefert) und vergleicht mit dem
  Ziel-Anflugwinkel (z. B. 7°). Anzeige wie ein ILS-/PAPI-Cue: **„zu hoch /
  auf Pfad / zu tief"** — der Pilot fliegt das Symbol auf die Mitte und landet
  manuell, ohne Autoland.
- **Onboard-OSD (optional, später):** ein onboard MAX7456 würde das HUD direkt
  ins Video brennen — latenzärmer und unabhängig von der Flightbox, aber ärmer.
  In Phase 1 nicht nötig.

### 2.7 Heim- & Wegpunkt-Navigation per GPS (Steerpoint-Prinzip wie F-16)

Ein billiges GPS-Modul (~5–10 CHF, ~5–10 g, seriell an den DSP) liefert Position.
Der DSP führt eine kleine Liste von **Steerpoints** — genau die klassische
**F-16-Wegpunktnavigation**: **Home** ist Steerpoint 1 (beim Arming gesetzt),
weitere Punkte optional vorab geladen. Zum aktiven Steerpoint rechnet der DSP
**Bearing, Distanz und Time-to-go** und zeigt im HUD einen **Steering-Cue**
(Kurs-Caret/„Tadpole" + Steerpoint-Raute), den der Pilot auf die Mitte fliegt —
reine Instrumentennavigation, auch ohne Sicht. Reine Software, kein Extra-Bauteil.

- **Steerpoints durchschalten:** per Knüppelschalter oder automatisch beim
  Erreichen (Umschaltradius) → eine einfache Route abfliegen.
- **RTH/Heimflug:** Steerpoint = Home → bei Signalverlust fliegt der DSP autonom
  heim; im Handbetrieb führen Pfeil + Distanz + Glideslope zurück in den Anflug
  (§2.6); der Pilot landet manuell.
- **LOITER:** kreist per GPS exakt über dem aktiven Punkt (Default: Home).
- **Zwei Betriebsarten:** *Pilot fliegt den Cue* (Default — „Gehirn zeigt, Pilot
  steuert") oder optional **DSP fliegt die Route autonom** (Autonomie ist aus
  AUTO-CLIMB/RTH/LOITER schon da); Pilot übernimmt jederzeit.
- **GPS-Ausfall-Rückfall:** Baro-Loiter + letzte Lage halten und warten; die
  schnelle Stabilisierung bleibt ohnehin auf der IMU. GPS ist digital (UART, NMEA/UBX).

### 2.8 Nachtflug (Auslegungsfall)

- **Kamera:** Starlight-Low-Light-Sensor (analog) — bei Nacht besser *und*
  billiger als digitales HD, behält niedrige Latenz und die Scramble/Overlay-Kette.
- **HUD wird Hauptinstrument:** man fliegt effektiv auf Instrumente; Horizont,
  Höhe, Home-Pfeil und Anflugwinkel sind primär, das dunkle Bild sekundär.
- **Positionslichter** am Modell (vorn/hinten verschieden), damit der Pilot
  Lage und Richtung am Nachthimmel erkennt.
- **Kante:** Dämmerung (thermal crossover) und bedeckter Himmel schwächen den
  optionalen Thermo-Horizont; klare Nacht ist der beste Fall.

### 2.9 Video-Zugangsschutz *(Phase 2)*

Ziel ist **kein** Hochsicherheits-Krypto, sondern: das Bild soll **nicht sofort
als Videosignal erkennbar** sein und **kein zufälliger FPV-Zuschauer** soll sich
reinschalten können. Dafür ist analoges Scrambling genau das richtige, billige
Werkzeug:

- **Sync-Suppression** (Pegel/Sync in der Austastlücke verwürfeln) → ein normaler
  FPV-Empfänger rastet nicht ein, zeigt nur rollenden/zerrissenen Müll und
  überspringt den Kanal.
- **PRNG-variiert** (zeilenweise wechselnd, Schlüssel vom vorhandenen DSP — kein
  Extra-Bauteil) → schlägt die simplen fixen Gated-Sync-Descrambler und lässt das
  Signal eher wie Rauschen als wie ein Bild aussehen. Optional zusätzlich
  Videoinvertierung.
- Der eigene **Boden-Decoder** (spiegelbildlich) stellt das Bild sauber her.
- **Einordnung — passend zum Bedrohungsmodell:** gegen den zufälligen Zuschauer
  mit FPV-Brille voll wirksam; ein gezielter Angreifer mit Laborausrüstung käme
  durch — aber das ist hier ausdrücklich nicht der Gegner.

### 2.10 Flightbox (Herzstück) & Command Center

Die **Flightbox** ist das zentrale Bodengerät — eine eigenständige, wetterfeste
Box (idealerweise **auf dem Dach**: freie Sicht für beide RF-Strecken). Sie
vereint **alles Bodenseitige** und macht jeden Browser im WLAN zum Cockpit:

- **2,4-GHz-ELRS-Sender** (Steuerung hoch) + **5,8-GHz-Diversity-Video-RX**
  (Video runter, 2 Antennen). Dach = Höhe = Sichtlinie = Reichweite.
- **SBC (Raspberry Pi Zero 2 W)** mit **Webserver**: serviert die **WASM-App**,
  streamt Video (WebRTC), bridged Steuerung/Telemetrie und treibt den ELRS-TX.
- **WLAN** ins Haus — PC/Tablet/Handy lädt nur die WASM-App aus der Flightbox.
- **Strom** am Standort (Akku oder PoE), wetterfestes Gehäuse.

Datenwege:
- **Video:** 5,8-RX → USB-Grabber → WebRTC-Stream → Browser.
- **Steuerung:** Browser (Gamepad) → WebRTC/WS → **CRSF** → ELRS-TX → Flieger.
- **Telemetrie:** Flieger → **ELRS-Rückkanal** → ELRS-TX → Flightbox → Browser
  (über ELRS, **nicht** über den Video-Sender).
- **HUD:** die **Flightbox/Command-Center rendert das HUD** als Overlay übers
  Video aus der ELRS-Telemetrie (Horizont/Höhe/Kurs/Home/Glideslope + Moving-Map)
  — **kein Onboard-OSD nötig.**

**Command Center = WASM-App**, von der Flightbox serviert, in jedem Browser
geöffnet: Video + HUD-Overlay + Moving-Map, Game-Controller (Web-Gamepad-API) →
Steuerung. Der Gamepad liefert *Korrekturwünsche* — der Flieger stabilisiert
selbst (§2.3), daher ist etwas WLAN-Latenz unkritisch.

*(Der Video-Descrambler sitzt in Phase 2 vor dem Grabber; §2.9.)*

### 2.11 Antennen

| Strecke | Flugzeug | Bodenstation (Dach) |
|---|---|---|
| **2,4 GHz Steuerung (ELRS)** | winzige ~3-cm-Antenne (Dipol/Keramik) | kleine Omni oder **Richtantenne (Patch, zum Fluggebiet)** für Reichweite |
| **5,8 GHz Video** | kleine **zirkular polarisierte** Antenne (Cloverleaf/Pagoda), wenige g | **Diversity:** Richtantenne (Patch/Helix ~8–14 dBi, zum Fluggebiet) + CP-Omni |
| **GPS** | integrierte Keramik-Patch am Modul | — |
| **WLAN** | — | Pi-Onboard oder kleine externe Antenne (nur bis ins Haus) |

Beide RF-Strecken sind jetzt **klein und leicht** auf der Flugseite (2,4 & 5,8 GHz
= cm-Wellenlänge) — die mühsame ~1-m-35-MHz-Drahtantenne entfällt. Der Gewinn der
**Dachplatzierung** ist nun nicht mehr eine riesige Antenne, sondern **Höhe =
Sichtlinie** plus die Möglichkeit, boden­seitig **Richtantennen** (Patch) zum
Fluggebiet einzusetzen. 5,8 GHz **zirkular** gegen Multipath. Leistungslimits (CH)
bleiben: 2,4 GHz ≤ 100 mW EIRP, 5,8 GHz ≤ 25 mW EIRP.

### 2.12 Software — Phasen, fertig vs. selbst geschrieben

**Phase-1-Datenfluss** (Flieger ↔ Flightbox ↔ Command Center):

```
 FLIEGER (iNav-FC)         FLIGHTBOX (Pi Zero 2 W)          COMMAND CENTER (WASM/Browser)
  Cam → VTX 5,8 ──────────► 5,8-RX → Grabber → WebRTC ─────► Video-Anzeige
  ELRS-RX ◄──────── ELRS-TX ◄── CRSF ◄── Control-Bridge ◄─── Gamepad (Web-API)
  Telemetrie ──► ELRS ─────► CRSF ─► Telemetrie-Relay ──────► HUD + Moving-Map
                                       └──────── WLAN ─────────┘
```

**Empfohlener Stack (Phase 1):**
- **Flieger:** nur **iNav-Konfiguration** — kein eigener Code.
- **Flightbox (Pi):** **GStreamer** (Capture → **WebRTC**, low-latency) +
  **Control-Bridge** (WLAN → **CRSF** ans ELRS-TX über UART) + Telemetrie-Relay
  (Rust oder Python).
- **Command Center:** **Rust → WASM mit egui** (Canvas: Video, HUD, Moving-Map mit
  Home/Steerpoints) + **Web-Gamepad-API** + **WebRTC-Data-Channel** (Steuerung +
  Telemetrie in einer Verbindung). Browser-basiert, plattformunabhängig.

Der Autopilot-Kern ist Open Source (**iNav** bzw. ArduPlane) — der schwierigste
Teil ist damit erprobt und geschenkt, was zu „robust/billig" passt. Selbst
geschrieben werden nur die Ränder.

**Fertig (nur konfigurieren):**

| Komponente | Software |
|---|---|
| ELRS RX/TX | ELRS-Firmware flashen + Binding/Paketrate |
| ESC | BLHeli_S |
| Flug-Gehirn (F405): FBW-Stabilisierung, Elevon-Mix, GPS-RTH, Wegpunkte, Failsafe, Auto-Launch, MAX7456-OSD (Horizont/Home-Pfeil/Distanz) | **iNav / ArduPlane** — konfigurieren + tunen |
| Rate-Damper-Backup + Watchdog-MUX | Hardware (OpAmps + Timer), quasi keine Software |

**Selbst geschrieben:**

| Komponente | Eigene Software | Umfang |
|---|---|---|
| **Video-Scrambler + Descrambler** *(Phase 2)* | PRNG-Sequenz + Sync-Timing (Flieger *und* Bodenseite, gleicher Schlüssel) auf je einem Mini-MCU | klein, Eigenprojekt |
| **Flightbox (Pi Zero 2 W)** | **Webserver** (serviert WASM) + Video-Capture→**WebRTC**-Stream + **Control-Bridge** (WLAN → CRSF ans ELRS-TX) + Telemetrie-Relay | mittel |
| **Command Center (WASM)** | von der Flightbox serviert: Video + **HUD/Moving-Map-Overlay** (aus ELRS-Telemetrie) + Gamepad → Steuerung | mittel |
| **HUD/Karte** | Horizont/Höhe/Kurs/Home + **F-16-Steering-Cue** + Moving-Map im Command Center aus ELRS-Telemetrie rendern | klein–mittel |
| **Senderloser Arm-/Startablauf** | „Start-Taste am Flieger + werfen, ohne TX" — kleiner Eingriff/Mod an iNav oder Zusatz-MCU (iNav armt sonst über den Sender) | klein |

**Einordnung:** Der Autopilot-Kern (das Schwierigste) ist fertig und robuster als
Eigenbau. Die überschaubaren Eigen-Brocken sind **PC-App, Pi-Bridge/Stream,
Scrambler-PRNG** (Python/C). Zwei davon sind echte Neuentwicklung, weil es sie
nirgends fertig gibt — und genau sie machen das Projekt besonders: das **analoge
Video-Scrambling** und der **senderlose Selbststart**.

---

## 3. Teileliste

**Fluggerät** (Zielrahmen ≤ 150 CHF; Preise AliExpress/LCSC-Niveau):

| Subsystem | Teil | ~CHF | Domäne |
|---|---|---|---|
| Airframe | Foamboard/EPP-Nurflügel, Horns, Gestänge | 6 | — |
| Motor | effizienter Low-Kv BLDC (Slow-Flyer, Pusher) | 9 | — |
| ESC | 20 A BLHeli_S (Telemetrie) | 6 | digital |
| Prop | großer langsamer Prop (effizient) | 1 | — |
| Servos | 2× Micro (Elevon) | 3 | analog |
| Akku | **Li-ion 3S1P–3S2P (18650)** — hohe Energiedichte, lange Flugzeit | 14 | — |
| **Steuer-RX** | **ELRS 2,4-GHz-RX** (CRSF an F405, Telemetrie) | 10 | digital |
| **Gehirn (DSP)** | STM32F405 (Fusion + Validierung + CRSF + Nav + HUD) | 4 | digital |
| IMU | ICM-42688 (rauscharm, Lage/Rate) | 4 | digital |
| Höhe | BMP280 (Baro) | 1 | digital |
| GPS-Modul | ATGM336H / BN-180-Klasse (seriell, ~5–8 g) | 6 | digital |
| Kamera | Starlight-Low-Light, Composite PAL | 28 | analog |
| VTX | 5,8 GHz schaltbar (Betrieb 25 mW) + CP-Antenne | 12 | analog |
| HUD/OSD *(optional)* | onboard MAX7456 — **Phase 1 nicht nötig** (HUD läuft auf der Flightbox) | (5) | — |
| Video-Scrambler *(Phase 2)* | LM1881 + Analog-Switch (Sync-Suppression) | (2) | analog |
| Rate-Damper-Backup *(Phase 2)* | Analog-Gyro-Rest + OpAmp + MUX + Watchdog | (4) | analog |
| Positionslichter | LEDs vorn/hinten | 1 | — |
| Start-Taster | Taster + Arm-LED/Piepser am Modell | 1 | — |
| Strom/PCB/Kabel | BEC, LDO, Caps, Stecker, Platine | 8 | — |
| **Summe Phase 1** (ohne Phase-2/optionale Module) | | **≈ 114** | |
| + Phase 2 (Scrambler + Analog-Reversion) | | **+ 6** | |
| + 15 % Reserve/Versand (Phase 1) | | **≈ 131** | |

> **Phase-1-Vereinfachung:** ein integrierter F405-FC vereint Gehirn + IMU + Höhe
> + OSD + BEC auf einem steckbaren Board (~20 CHF) → die Einzelzeilen verschmelzen,
> **kein Custom-PCB, minimales Löten** (nur Motor/Akku/RX/VTX Standard-Lötpunkte).

**Optional (noch unter 150):**

| Teil | ~CHF | Nutzen |
|---|---|---|
| Ultraschall-Rangefinder | 2 | Höhe-über-Grund fürs sanfte Abfangen bei Nacht |
| Landelicht-LED | 1 | Aufsetzpunkt bei Nacht ausleuchten |

**Bodenstation (Dach, separat vom 150-CHF-Fluggerät-Budget):**

| Teil | ~CHF |
|---|---|
| Raspberry Pi Zero 2 W (WLAN — Stream + Bridge) | 18 |
| USB-Composite-Video-Grabber | 8 |
| 5,8-GHz-Diversity-RX + 2 Antennen (Richt + Omni) | 25–40 |
| Descrambler (LM1881 + Switch, PRNG-Schlüssel) | 5 |
| **ELRS-2,4-TX-Modul** (CRSF vom Pi) | 25–35 |
| 2,4-GHz-Antenne (klein, ggf. Richt-Patch) | 3–8 |
| Strom + wetterfestes Gehäuse (Akku/PoE + Box) | 15–25 |
| USB-Game-Controller (PC vorhanden) | ~20 |
| **Summe Bodenstation** | **~120–160** |

---

## 4. Offene Punkte & ehrliche Kanten

- **2,4 GHz ist Sichtlinie & kürzere Reichweite als 35 MHz** — kein NLOS um
  Gelände. Aufgefangen durch **RTH**: fliegt er zu weit, dreht er um und gewinnt
  den Link beim Näherkommen zurück. Dachplatzierung + Richtantenne maximieren die
  nutzbare Reichweite.
- **Jamming:** FHSS ist jamming-fest und degradiert graceful (Link-Quality sinkt).
  Ein sehr starker Breitband-Jammer kann den Uplink dennoch verweigern — Ergebnis
  bleibt sicher (RTH/LOITER), aber überschreien kannst du ihn nicht.
- **GPS-Abhängigkeit:** RTH, Home-Navigation, präziser Loiter und Glideslope
  hängen am GPS; bei Störung/Ausfall bleibt Baro-Loiter + Lage halten. GPS lässt
  sich jammen/spoofen — fürs Lernprojekt unkritisch, aber ehrlich benannt.
- **Flare bei Nacht:** GPS-/Baro-Höhe ist für den Aufsetzpunkt zu grob; ein
  langsamer Schaumflügel landet aber weich (Stall aufs Gras). Optional
  Ultraschall-Rangefinder + Landelicht für sauberes Abfangen.
- **Nacht + günstige Kamera:** die Starlight-Klasse ist der Kompromiss; das
  Videobild bleibt körnig — deshalb HUD als Hauptinstrument.
- **Latenz durch PC/WLAN:** Digitalisieren + WLAN + PC-Anzeige addieren
  ~30–100 ms zum Videobild (gegenüber ~10 ms an einem direkten Monitor). Für den
  Slow Flyer mit Onboard-Stabilisierung ok; für minimale Latenz an der
  Bodenstation zusätzlich einen direkten Analog-Monitor abgreifen. Der Steuerpfad
  verträgt Latenz, weil der Flieger selbst stabilisiert.
- **Rechtslage (CH):** 2,4-GHz-Steuerung ≤ 100 mW EIRP (RLAN, RIR1010);
  5,8-GHz-Analogvideo ≤ 25 mW EIRP (RIR1008-12); Nacht-FPV hat BAZL-Auflagen
  (Beleuchtung, Spotter/Sicht). Vor dem realen Flug aktuelle BAKOM/BAZL-Regeln
  (NaFZ 2026) prüfen — SRD/RLAN gilt „non-interference/non-protection".
- **Autonomer Handstart:** der Motor läuft erst nach erkanntem Wurf an
  (Sicherheit); ein zu zaghafter Wurf unter Stallfahrt ist das Risiko → fester
  Wurf nötig. Der senderlose Steigflug verlangt saubere Fusion/Trim-Kalibrierung
  (Selbsttest beim Arming).
- **Zwei Ausfallnetze getrennt:** Link weg → Gehirn macht RTH/Loiter; Gehirn weg →
  analoge Reversion hält es fliegend.

---

*Grundlagen-Recherche (Machbarkeit, Sensor-/Funk-/Video-Optionen, CH-Bandwahl
gegen NaFZ 2026 / RIR verifiziert) im Projektverlauf erarbeitet; diese Fassung ist
die verdichtete Bauspezifikation.*

---

## Datenquellen & Lizenzen

Der Simulator lädt reale Welt-Daten on-demand. Attributionspflicht der Quellen:

- **Kartendaten:** © OpenStreetMap contributors — lizenziert unter der
  [Open Database License (ODbL)](https://www.openstreetmap.org/copyright)
  (Shortbread-Vektorkacheln).
- **Luftbilder:** Esri World Imagery — © Esri und seine Datenlieferanten.
- **Geländehöhe:** Copernicus DEM (Terrarium-kodiert).
- **Sternkatalog:** [HYG Database](https://github.com/astronexus/HYG-Database)
  (Hipparcos-abgeleitet) — lizenziert unter CC-BY-SA 4.0.
