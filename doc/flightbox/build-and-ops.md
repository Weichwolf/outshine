# Build, Gates und Betrieb

> Body still in German — translation pass pending (see [roadmap](roadmap.md)).

Rezepte leben im Makefile, nicht in Agenten-Köpfen. Diese Datei sagt, welches Target es gibt, was ein
Beweis ist, und was auf diesem Rechner besonders ist.

## Spec

What a proof is, and what has to hold before a change counts as verified.

| Gate | Must hold |
|---|---|
| Warnings = errors | all targets clean under `-Wall -Wextra -Wpedantic` |
| `nm` gate | `build/fb-gym` contains zero Dawn/WebGPU symbols |
| Harnesses | all seven test binaries rc=0 |
| Frame proof | build-effective changes need a rendered frame **or** a numerical measurement |
| Regression | telemetry of all `sim/missions/*.fbm` byte-compared; every deviation justified individually, every verdict change separately |
| Determinism | `--threads 1/2/4` × repetitions produce a single signature |
| WASM | `make -C sim wasm` builds and the app boots in the browser — the one client used daily |
| Model deltas | `make -C sim verify-models` green: every copy deviates from the pinned upstream by EXACTLY the entries in `sim/assets/MODEL-DELTAS.md` |
| vendor read-only | `sim/vendor/jsbsim` is never modified; only the COPY changes, and then as a named delta |

Two rules about how measuring is done at all: accepted properties of the vanilla JSBSim F-16 are the
truth and not defects (`CLAUDE.md` principle 5), and measurements run through the **mission control
loop** (telemetry), never through single observations.

Recipes live in the Makefile, not in agents' heads.

## State

All gates exist and are runnable today; the delta gate is the newest and was proven in four failure
directions (see the German detail below and [`journal.md`](journal.md)).

## Gaps

| Thing | Where it is tracked |
|---|---|
| The mission control loop effectively runs on `const`/`swiss` elevation because `payerne-full` crashes under `--elev tiles` | [`clients/clients.md`](clients/clients.md) |
| The delta entry format is untested for a multi-file delta or a new file (diff against `/dev/null`) | [`aircraft/stores.md`](aircraft/stores.md) |

## Knowledge

Targets, gates and host facts in full.

### Make-Targets

Jedes Projekt trägt sein eigenes Makefile.

#### `sim/`

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
| `verify-models` | die Delta-Prüfung: `assets/aircraft` gegen das gepinnte Submodul + `assets/MODEL-DELTAS.md` |

Fehlt der Tile-Worker, hängt die WASM-App still beim Start (404 im Worker). Deshalb die feste
Abhängigkeit statt zweier getrennt zu merkender Targets.

#### `tiles/`

`build` | `image` | `run`

### Gates

Eine Änderung gilt erst als verifiziert, wenn sie diese Prüfungen besteht.

| Gate | Prüfung |
|---|---|
| **Warnings = Errors** | alle Targets sauber unter `-Wall -Wextra -Wpedantic` |
| **`nm`-Gate** | `build/fb-gym` enthält 0 Dawn-/WebGPU-Symbole |
| **Harnesses** | alle sieben Test-Binaries rc=0 |
| **Frame-Beweis** | build-wirksame Änderungen brauchen einen gerenderten Frame **oder** eine numerische Messung |
| **Regression** | Telemetrie aller `sim/missions/*.fbm` byte-verglichen; jede Abweichung einzeln begründet, jede Verdikt-Änderung eigens |
| **Determinismus** | `--threads 1/2/4` × Wiederholungen ergeben eine einzige Signatur |
| **WASM** | `make -C sim wasm` baut und die App startet im Browser — der einzige Client, der täglich benutzt wird; ein gebrochener Boot ist teurer als jeder andere Fehler |
| **Modell-Deltas** | `make -C sim verify-models` grün: jede Kopie unter `sim/assets/aircraft` weicht vom gepinnten Upstream um EXAKT die Einträge in `sim/assets/MODEL-DELTAS.md` ab |
| **vendor read-only** | `sim/vendor/jsbsim` wird nie geändert — Engine wie Modelle. Geändert wird höchstens die KOPIE, und dann als benannter Delta-Eintrag |

#### Der Delta-Gate im Detail

`verify-models` rechnet je Datei den kanonischen Unified-Diff (`difflib`, 3 Zeilen Kontext) zwischen
Upstream und Kopie und vergleicht ihn zeichenweise mit dem Diff-Block des zugehörigen Eintrags. Es
schlägt in **vier** Richtungen fehl, alle nachgemessen:

| Fall | Meldung |
|---|---|
| unerklärte Abweichung in einer Kopie | `UNEXPLAINED difference from upstream` + der Block, der fehlt |
| erklärter Delta, den die Kopie nicht (mehr) trägt | `declares a delta that is NOT present in the copy` |
| Eintrag vorhanden, aber Diff stimmt nicht überein | `the declared delta does not match the actual difference` |
| Modell unter `assets/aircraft`, das die Herkunftstabelle nicht nennt | `is not declared in ... ('## Herkunft')` |

Ein Delta-Block wird **generiert, nicht getippt** — `python3 tools/verify_models.py --emit` gibt das
fertige Eintragsgerüst aus. In einem Unified-Diff ist Whitespace bedeutungstragend (Kontextzeilen tragen
ihr führendes Leerzeichen), also ist Abtippen eine Fehlerquelle ohne Nutzen.

Das Target ist bewusst KEINE Voraussetzung der Build-Targets: es hat nur etwas zu sagen, wenn eine Datei
unter `assets/aircraft` oder im Submodul sich ändert, und dafür einen Python-Interpreter auf den
kritischen Pfad jedes C++-Compiles zu legen wäre der falsche Tausch.

### Mess-Disziplin

- Akzeptierte Modell-Eigenschaften der vanilla JSBSim-F-16 sind die Wahrheit, keine Defekte
  (CLAUDE.md, Prinzip 5).
- Messungen laufen über den **Missions-Regelkreis** (Telemetrie), nicht über Einzelbeobachtungen.
- Ziel-GPU-Fähigkeiten: `doc/webgl-webgpu-report.txt`.

### Der Missions-Regelkreis

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

### Host und Betrieb (dieser Rechner)

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
