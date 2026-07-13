# FlightBox — Simulation (alles in C)

Zwei-Container-Simulation des ganzen Systems. Die **„Funkverbindung" ist UDP**
zwischen den Containern; der Browser spricht mit der Flightbox über HTTP+WebSocket.

```
  [ aircraft ]  --UDP downlink (Telemetrie + Video/Kunsthorizont)-->  [ flightbox ]
   (C-Sim:      <--UDP uplink (Steuerung) ------------------------     (C: HTTP-Server
    Dynamik,                                                            + WASM-App
    FBW, GPS,                                                          + WS-Bridge
    Kamera)                                                            + UDP-Radio)
                                                                             |
                                                              HTTP + WebSocket (WPA2 im Real)
                                                                             v
                                                            [ Browser: Command Center (C->WASM) ]
                                                             Video + HUD-Overlay + Gamepad/Tastatur
```

## Komponenten
- `common/protocol.h` — gemeinsame UDP/WS-Wire-Structs (Control, Telemetrie, Video).
- `aircraft/aircraft.c` — simuliert den kompletten Flieger: FBW-Dynamik, Zustands-
  automat (ARMED→CLIMB→LOITER/MANUAL/RTH), Sensoren, GPS, Akku und die „Kamera"
  = künstlicher Horizont (oben blau, unten grün, kippt mit der Lage).
- `flightbox/server.c` — HTTP-Server (serviert die WASM-App aus `web/`), WebSocket-
  Bridge (Video/Telemetrie→Browser, Steuerung←Browser), UDP-Radio zum Flieger.
- `command_center/cc.c` — C→WASM (Emscripten + SDL2): zeigt Video, rendert das HUD
  aus der Telemetrie, liest Gamepad **oder Tastatur**, sendet Steuerung über WS.

## Bauen & Starten

**In Podman-Containern (Zielbetrieb):**
```bash
./build-wasm.sh      # C -> WASM (nutzt ~/Git/emsdk); einmal, dann bei cc.c-Änderungen
./run-podman.sh      # baut beide Images, startet sie, published :8080
# Browser: http://localhost:8080
# Stop: podman rm -f fb-aircraft fb-flightbox
```

**Nativ (schnelles Iterieren, ohne Container, UDP über loopback):**
```bash
./build-wasm.sh
./run-native.sh      # Ctrl-C zum Stoppen
# Browser: http://localhost:8080
```

## Steuerung im Command Center
Ins Bild klicken, dann:
- **←↑↓→** Roll/Pitch · **A/D** Yaw · **W/S** Gas · **Enter** Arm · **L** (halten) Link-Verlust → RTH.
- Ein angeschlossenes **Gamepad** wird automatisch erkannt (rechter Stick Roll/Pitch,
  linker Stick Yaw/Gas, A = Arm, B = Link-Verlust).

## Status
Verifiziert (nativ **und** in Containern): HTTP/WASM-Serving, WebSocket-Handshake
(eigener SHA1/Base64 in C), UDP-Radio in beide Richtungen, Video-Frames (~20 fps),
Telemetrie, und Failsafe (Link-Drop → RTH). Das Browser-Rendering (SDL2/WASM)
kompiliert; visuell im Browser prüfen.

## Bezug zur Spec
Entspricht **Phase 1** aus `../README.md` (§2.12): Flightbox = Herzstück
(Webserver + WASM + „Sender"/„Video-RX"), Command Center = WASM, Flieger-Logik.
Statt echtem ELRS/5,8-GHz/WebRTC hier UDP + WebSocket-Frames — dieselbe
Architektur, simulierte Funkstrecke.
