# FlightBox — Fidelity-Baseline & Mess-Konventionen

Referenz für sim-critic und sim-developer. Enthält die **akzeptierten Modell-Eigenschaften** (keine
Defekte — die Referenz ist das vanilla JSBSim-F-16-Modell selbst) und die **Mess-Konventionen**.

## Referenzkette des FDM

Die Aerodynamik des JSBSim-F-16-Modells stammt aus **NASA TP-1538** (Nguyen et al. 1979,
Langley-Windkanaldaten, F-16A, α −20°..+90°, β ±30°, statisch + forced-oscillation) — belegt im
Modell selbst (`jsbsim/aircraft/f16/f16.xml` Zeile 29, README). Kette: NASA-Windkanal → JSBSim-Modell
→ FlightBox. Nachbau-Spezifikation und recherchierte Design-Ziele: `doc/f16/` (bes.
`flight-controls-flcs.md`, `aerodynamics-performance.md`). Ehrliche Modellgrenzen: F-16A-Datensatz,
subsonisch/low-speed-Schwerpunkt; numerische Schub-Decks des F110 sind nicht öffentlich → die
JSBSim-Propulsion-Tabellen sind die Implementierung.

## Akzeptierte Modell-Eigenschaften (nicht flaggen)

| Eigenschaft | Beleg |
|---|---|
| Rollrate ~190 °/s (echter Jet 270–320) | aero-limitiert: FLCS-Kommando ≈ Raw-Override, flach über q |
| Rest-Slip im Loiter: ny ≈ −0.10 g, Ruder steht | Yaw-FLCS füttert rohe Yaw-Rate (Gain 100, kein Washout, ±1-Clip); volle Pedale kompensieren nur teilweise |
| Euler-Turn-Identität liest ~15–20 % niedrig | Folge des Slips; **kräfte-kohärent prüfen**: φ_eff = φ − atan(\|ny\|/nz), ω ≈ g·tan(φ_eff)/V (±15 %) |
| Roll-in-nz-Transient (≈0.5…2.3 g, settle <2 s) | im Bare-Model-Envelope (neutraler Pitch-Stick, gleiche Rollrate: −0.3…+1.9) |
| Spiralmodus hands-off divergent | modell-inhärent (auch unter tFull-Trim); Produktion fliegt nie hands-off |
| Kein hartes 9-g-Cap oberhalb Corner | Modell-FLCS hat AoA-, kein g-Struktur-Limit |

## Mess-Konventionen

- **Artefakt-Hash-Lock:** `md5sum flightbox/web/cc.js cc.wasm` vor/nach jedem Messlauf; bei Änderung
  Lauf verwerfen. Während eines Critic-Laufs wird nicht rebuildet.
- **AGL/Boden numerisch, nie HUD-OCR:** die 1-Hz-Konsole `[agl] alt=… agl=… ground=…` ist die
  Wahrheit (OCR von HUD-Crops hat nachweislich Ziffern halluziniert).
- **Bare-Model-Vergleich:** Verdacht auf Regelungs-Defekt → dasselbe Manöver am nackten Modell
  (`fb_jsbsim_set_controls`+`step`, neutraler Stick) messen; nur die DIFFERENZ ist unser Defekt.
- **Frame-Beweis:** Build-wirksame Änderungen gelten erst mit gerendertem Frame oder numerischer
  Messung als verifiziert ("bootet, keine Pageerrors" hat einen Totalausfall übersehen).
- **Headless-Eigenheiten:** SwiftShader-Chromium killt das WebGPU-Device nach wenigen Frames
  ("DEVICE LOST reason=2") — headless-only, im echten Browser stabil; Pipeline-Validität an
  Validation-Errors + den Frames davor messen. Headless läuft die Sim ~1/10 Echtzeit.
- **Walk-Kadenz ist Tile-Grenz-getriggert (by design):** der Quadtree re-walkt beim Überqueren einer
  z14-Tile-Grenze (~1.67 km) oder solange unkonvergiert — bei 220 m/s Echtzeit alle ~7.6 s; HEADLESS
  (~1/10 Speed) entsprechend alle ~76 s Wall-Clock. Log-Stille zwischen Grenzen ist KEIN Stall
  (Print zusätzlich auf moved/done/sharpen gegated). Verbesserungs-Kandidat (offen, kein Defekt):
  zusätzlicher Distanz-Trigger (~400 m Bahnstrecke).
- **Streaming-Zähler:** `[world3d] quadtree:`-Zeilen; `over budget` ist 0 by design
  (Priority-Refinement, dynamische Buffer).

## Produktions-Regelpfad

`flightsim.h` → `flightctl.h` (`fc_f16`, FLCS-Kommando-Inner) → `jsbsim_adapter` → vanilla
`jsbsim/aircraft/f16`. Produktionspfad seit 2026-07-22: `flightsim.h` → `fb/FBAutopilot` + `fb/FBFlightControl` (+`FBState`),
bit-identisch zu flightctl.h verifiziert (beide Branches); flightctl.h bleibt nur für native Harnesse.
Native Harness: `g++ -O2 -std=c++17 <harness> aircraft/fdm/jsbsim_adapter.cpp
validator/build/jsbsim-host/lib/libJSBSim.a -I validator/build/jsbsim-host/include/JSBSim
-I aircraft/fdm -I command_center -lm` (cwd `sim/`).
Headless-Probe: Playwright aus `sim/tools/node_modules`, Chromium-Args
`--no-sandbox --enable-unsafe-webgpu --enable-features=Vulkan`, Seite `http://localhost:8080/`
(bzw. `/gpu.html`).
