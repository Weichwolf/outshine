# Build, Gates und Betrieb

Rezepte leben im Makefile, nicht in Agenten-Köpfen. Diese Datei sagt, welches Target es gibt, was ein
Beweis ist, und was auf diesem Rechner besonders ist.

## Make-Targets

Jedes Projekt trägt sein eigenes Makefile.

### `sim/`

| Target | Ergebnis |
|---|---|
| `core-lib` | `build/libfbcore.a` — der Simulator als Bibliothek |
| `gym` | `build/fb-gym` — headless, GPU-frei |
| `native` | `build/gpu_native` — Referenz-Renderer / Frame-Orakel |
| `wasm` | `web/gpu.js` + `web/gpu.wasm` — **hängt vom `worker`-Target ab und baut immer beide** |
| `worker` | `web/fbtileworker.js` + `.wasm` — einzeln aufrufbar |
| `image`, `up` | Container-Bau bzw. -Start |
| `test-monitor` | `fb-test-hard-landing`, `fb-test-loc-departure`, `fb-test-nan-divergence` |
| `test-fdm` | `fb-test-two-fdm` — zwei koexistierende FDM-Instanzen |
| `test-corner` | `fb-test-corner-speed` — misst Corner-Speed/-g/-Drehrate des Modells |
| `test-missile` | `fb-test-missile-airframe` |
| `test-gun` | `fb-test-gun` — Streuung, Flugzeit, Trichtergeometrie, Vorhaltelösung, Munitionsverbrauch |

Fehlt der Tile-Worker, hängt die WASM-App still beim Start (404 im Worker). Deshalb die feste
Abhängigkeit statt zweier getrennt zu merkender Targets.

### `tiles/`

`build` | `image` | `run`

## Gates

Eine Änderung gilt erst als verifiziert, wenn sie diese Prüfungen besteht.

| Gate | Prüfung |
|---|---|
| **Warnings = Errors** | alle Targets sauber unter `-Wall -Wextra -Wpedantic` |
| **`nm`-Gate** | `build/fb-gym` enthält 0 Dawn-/WebGPU-Symbole |
| **Harnesses** | alle sieben Test-Binaries rc=0 |
| **Frame-Beweis** | build-wirksame Änderungen brauchen einen gerenderten Frame **oder** eine numerische Messung |
| **Regression** | Telemetrie aller `sim/missions/*.fbm` byte-verglichen; jede Abweichung einzeln begründet, jede Verdikt-Änderung eigens |
| **Determinismus** | `--threads 1/2/4` × Wiederholungen ergeben eine einzige Signatur |
| **vendor read-only** | `sim/vendor/jsbsim` und das f16-Modell werden nie geändert |

## Mess-Disziplin

- Akzeptierte Modell-Eigenschaften der vanilla JSBSim-F-16 sind die Wahrheit, keine Defekte
  ([principles.md](principles.md), Prinzip 5).
- Messungen laufen über den **Missions-Regelkreis** (Telemetrie), nicht über Einzelbeobachtungen.
- Ziel-GPU-Fähigkeiten: `doc/webgl-webgpu-report.txt`.

## Der Missions-Regelkreis

Die Arbeitsweise für alles, was Piloten-KI oder Systemverhalten betrifft:

```
Mission definieren  →  headless simulieren  →  Telemetrie maschinell analysieren  →  Korrektur  →  Loop
```

Format `.fbm`, zeilenbasiert, zero-dependency — [`doc/mission-format.md`](../mission-format.md).
Geparst von `core/FBMissionFile.h` (reine Text→`FBMission`-Funktion, kein File-I/O — das macht die App).

Terminierung → Exit-Codes:

| Ergebnis | Exit |
|---|---|
| SUCCESS | 0 |
| FAIL | 1 |
| CRASH | 2 |
| TIMEOUT | 3 |

**Der Exit-Code ist nicht immer das Urteil.** Ein Kampf hat kein Wegpunkt-Ziel; solche Missionen enden
absichtlich im Timeout, und das Urteil steht in den Ereignissen und der Telemetrie. Wo das gilt, sagt es
der Kopfkommentar der jeweiligen `.fbm`-Datei — und der ist verbindlich, weil er die Leseregel trägt.

Ausgabe je Lauf in `--out/`:

| Datei | Inhalt |
|---|---|
| `telemetry.csv` | 10 Hz, feste Spaltenzahl. Neue Quellen werden **immer hinten angehängt**, damit keine gemessene Spalte ihre Position verliert. |
| `telemetry_<callsign>.csv` | je weitere Einheit |
| `events.log` | `t=SEK EVENT key=val`, greppbar |

## Host und Betrieb (dieser Rechner)

Kein verstecktes Agenten-Memory — alles Betriebswissen steht hier.

| Sache | Zustand |
|---|---|
| emsdk | `~/Git/emsdk` |
| `nproc`-Shim | `~/.local/bin` (macOS hat kein nproc) |
| Container | Podman-VM zuerst (`podman machine start`), dann `tiles/up.sh` (:8081) und `sim/up.sh` (:8080) |
| Live-Mount | fb-sim mountet `sim/web` live — `make wasm` wirkt per Refresh |
| WASM-Artefakte | gitignored |
| Native Builds | brauchen `sim/vendor/.compat-headers` (gitignored, host-lokal) |
| Git | Commit-Mail ist der GitHub-noreply-Alias; Push per SSH-insteadOf |
| `timeout(1)` | **existiert auf macOS nicht** — nicht in Skripte einbauen |

Die Missionsdateien werden beim WASM-Build aus `sim/missions/` nach `sim/web/missions/` kopiert; die
Kopie ist gitignored. Eine handgepflegte zweite Kopie wäre eine Fehlerquelle und war schon eine.
