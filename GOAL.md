# /goal — Echte iNav-Firmware im Loop + OSM-3D-Kamerabild

**Aktives Fokus-Ziel.** Der Aircraft-Container fährt die **echte iNav-Firmware**
(SITL) statt des handgeschriebenen Flugmodells — mein Code wird reines
**Physik-FDM + Funk-Bridge**. Das „Kamerabild" wird von blau/grün auf eine
**realistische 3D-Welt (osmmesh: OSM-Gebäude + Copernicus-Terrain)** umgestellt,
gerendert aus der GPS-Pose des Fliegers. Alles in C; zwei Podman-Container;
„Funk" = UDP; emsdk für WASM. Fertig ist es erst, wenn beide Teile **bewiesen**
laufen (headless über MSP bzw. sichtbar im Browser).

---

## Teil A — iNav-SITL-Integration (echte Firmware fliegt)

**Baustein `sim/aircraft/xp_bridge.c`** (ersetzt die Logik in `aircraft.c`; dessen
Physik bleibt als FDM erhalten). Der Aircraft-Container startet **`SITL.elf`**
(`--sim=xp --simip=<self> --simport=49000`) + `xp_bridge`.

Der Bridge verbindet drei Enden:
1. **X-Plane-UDP-Server (Port 49000):**
   - RREF-Subscriptions von iNav beantworten; Sensoren aus der Physik streamen:
     `phi/theta/psi`, `P/Q/R`, `g_axil/side/nrml`, `latitude/longitude/elevation/
     y_agl`, `local_vx/vy/vz`, `groundspeed/true_airspeed`, `barometer_current_inhg`.
   - iNavs Steuer-Ausgänge lesen (`yoke_roll/pitch/heading_ratio`,
     `throttle_ratio_all`) und in die Physik einspeisen.
2. **MSP-Client (TCP 5760):**
   - Pilot-RC vom Flightbox-Uplink → `MSP_SET_RAW_RC` (Receiver = MSP/SIM).
   - `MSP_STATUS/_EX` (Arm/Modi/Flags), `MSP_RAW_GPS`, `MSP_ATTITUDE`,
     `MSP_ANALOG` lesen → Telemetrie.
3. **Bestehende UDP-Downlink** zur Flightbox (Telemetrie + Video) bleibt.

**iNav-Einmal-Config** (per CLI/MSP, als `sim/aircraft/inav-config.txt` +
`eeprom.bin`-Rezept im Repo): Nurflügel-**Elevon-Mixer**, `receiver = MSP`,
Sensoren `= FAKE`, Modi ANGLE + NAV RTH, `failsafe_procedure = RTH`.

### Done-Kriterien A (alle headless via MSP verifizierbar)
- [ ] `SITL.elf` + `xp_bridge` starten im Container, X-Plane-Handshake steht
      (iNav bekommt Sensoren, Bridge bekommt Servo-Outputs).
- [ ] iNav **armt** sauber (Arming-Checks bestanden; GPS-Fix vom FDM geliefert).
- [ ] **ANGLE-Mode hält Lage:** RC-Roll/Pitch → Attitude folgt, Selbst-Nivellierung
      zurück auf 0 (per `MSP_ATTITUDE` gemessen).
- [ ] **Failsafe = echtes NAV-RTH:** RC-Verlust → iNav schaltet in RTH (per
      `MSP_STATUS`-Modus + zurücklaufende GPS-Position bewiesen).
- [ ] Zwei-Container-Setup grün; Command Center zeigt **echte iNav-Telemetrie**
      (Modus/Arm/Position stimmen mit MSP überein).
- [ ] `run-podman.sh` startet den SITL-Aircraft-Container ohne Handarbeit.

---

## Teil B — OSM-3D-Kamerabild (osmmesh)

Das simulierte „Kamerabild" wird eine **Out-the-window-3D-Sicht** der echten Welt
(Region Hameln/Grohnde), Kamera = **GPS-Pose (lat/lon/alt) + Attitude
(roll/pitch/yaw)** des Fliegers aus der Telemetrie. `~/Git/wasm-osm` (osmmesh, C99)
liefert Terrain- + Gebäude-Meshes in ENU um einen Origin.

**Architektur-Entscheidung (im Ziel festzuhalten):** Rendering **im Command
Center** aus der Telemetrie-Pose (Browser hat GPU/WebGL) — nicht offscreen am
Flieger. Bevorzugt **all-C GLES2/WebGL** über den SDL2-GL-Kontext (konsistent mit
„alles in C"); osmmesh-WASM liefert die Vertex-/Index-Buffer. Three.js-Pfad nur,
falls der C-GLES2-Weg zu teuer wird — dann dokumentiert.

### Done-Kriterien B
- [ ] osmmesh als WASM ins Command Center eingebunden (oder als C-Lib gelinkt);
      Region-PMTiles (Shortbread + Terrain) geladen.
- [ ] GLES2/WebGL-Renderer im Command Center: Kamera-Transform aus Flieger-Pose,
      Terrain + Gebäude gezeichnet; ersetzt das blau/grüne Frame.
- [ ] **HUD-Overlay bleibt drüber** (Horizont/Home/Distanz/Glideslope/Steerpoint),
      2D über der 3D-Szene.
- [ ] Beweis: bei Bewegung/Manövern im Sim **wandert und kippt die Welt passend
      zur Pose**; sichtbar im Browser, konsistent mit der Attitude-Telemetrie.
- [ ] Latenz/FPS akzeptabel (Ziel ≥ 20 fps im Browser).

---

## Reihenfolge & Grenzen
- **A vor B:** erst liefert die echte Firmware echte Pose/Telemetrie, dann wird
  daraus das Kamerabild gerendert.
- **Nicht-Ziele (bleiben Phase 2 / Real-Hardware):** Video-Scrambler, analoge
  Reversion, echtes ELRS/5,8-GHz/WebRTC (im Sim UDP + WebSocket-Frames).
- **Verifikation zuerst:** jede Stufe wird bewiesen (MSP-Messung bzw. Browser-
  Sichtprüfung), bevor sie als erledigt gilt — kein „läuft vermutlich".
