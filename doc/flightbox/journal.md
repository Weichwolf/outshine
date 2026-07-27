# Journal — the chronicle of the rounds

> Body still in German — translation pass pending (see [roadmap](roadmap.md)).

**What this file is:** one line per finished round, in the order they happened — commit, what it
built, what it measured. It is history, not a plan and not a contract.

- **What each area must do** → the `## Spec` section of its topic file ([`INDEX.md`](INDEX.md)).
- **What is built right now** → the `## State` section of that same file.
- **What is missing, and what was tried and rejected** → its `## Gaps` section.
- **What comes next, in order** → [`roadmap.md`](roadmap.md).

Every round adds a line here; nothing here is ever rewritten to look better. Rejected approaches are
kept — in the Gaps of the file they belong to, with their measurements.

State of the entries below: commit `793e1fe` + the model-root/delta round (2026-07-27).

## Reifegrad je Bereich

| Bereich | Zustand | Doku |
|---|---|---|
| FDM-Adapter | **fertig** — instanzfähig, IC-abgeschottet, Schadens- und Zuladungskanäle | [fdm.md](sim/fdm.md) |
| Core / Avionik-Bus | **fertig** — typisierte Blöcke mit Dreizustands-Gültigkeit, Kommandobus mit Quittung | [core.md](sim/core.md) |
| Missions-Orchestrator | **fertig** — vier Schritte, kein Missionswissen im Code | [units-and-missions.md](sim/units-and-missions.md) |
| Multi-Unit | **fertig** — Verband als Missionsdaten, Thread pro Einheit im Gym, deterministisch | [units-and-missions.md](sim/units-and-missions.md) |
| Sensoren | **gebaut** — Datalink, Radar, RWR, Gegenmaßnahmen. Ohne Terrain-Maskierung. | [sensors.md](sim/sensors.md) |
| Waffen | **gebaut** — AIM-120, Mk-82, M61A1, Bodenziele, Schadensmodell | [weapons-and-damage.md](sim/weapons-and-damage.md) |
| Piloten-KI | **in Arbeit** — Start/Route/Landung, BFM, BVR-Abfang, Luft-Boden fliegen; Verfeinerung läuft | [pilot-ai.md](sim/pilot-ai.md) |
| Renderer | **gebaut** — Stage-Split abgeschlossen. Einheiten und Waffen noch unsichtbar. | [rendering.md](render/renderer.md) |
| HUD | **gebaut** — generisches Default-HUD + volle F-16-Symbologie, Coverage-AA | [modules-f16.md](aircraft/f16.md) |
| Cockpit-Displays | **nicht begonnen** — die Werte liegen auf dem Bus, die Darstellung fehlt | [clients/clients.md](clients/clients.md) |
| HOTAS | **nicht begonnen** — bewusst zuletzt, ist nur ein Mapping | [clients/clients.md](clients/clients.md) |

## Chronologie

### Fundament (24.–25.07.)

| Commit | Abschnitt |
|---|---|
| `59f08c8` | Modul-Architektur runtime-polymorph, neun System-Slots mit NoOp-Defaults |
| `c9206eb`…`2099cb0` | Renderer-Stage-Split in vier Scheiben — am Ende null Inline-Shader in `FBRenderer.cpp` |
| `4cb92e8` | HUD-Provisorium → generisches Default-HUD im Displays-Slot |
| `2f3c277`, `8997eec`, `6f160af` | HUD-Font: Coverage-AA statt Alpha-Test, Split generisches Font-System / MAX7456-Hook, 16×16-Glyphen aus B612 Mono, dieselbe AA-Technik für alle Striche |
| `6802a6d`, `d31b1a9` | F-16-Haupt-HUD mit echter Combiner-Apertur, Lesbarkeit für 720p |

### Piloten-KI und der Regelkreis (26.07.)

| Commit | Abschnitt |
|---|---|
| `681c5f8` | Piloten-KI-Framework: `FBPilot`, Units, Airframe-Controls |
| `65d334c` | Missions-Runner + Telemetrie — **der Regelkreis selbst**, die Voraussetzung für alles Weitere |
| `e49d335` | Phase 1: Takeoff fliegt |
| `e4d7c26` | Telemetrie-/Log-Architektur: deklarative Sources, zentraler Bus, `FBLog` |
| `705c90a` | Lib/Client-Split: Core-Lib, `fb-gym`, Elevation-Hook, eingebackenes Schweiz-DEM |
| `28e74e5` | `FBFlightMonitor` — unbestechliches Physik-K.O., modell-abgeleitet |
| `92fe8a4` | Missions-Orchestrator auf vier Schritte, deklarativer Spawn, `FBMissionMonitor` |
| `8cd3a74` | Phase 3: Landung — `payerne-full` fliegt komplett autonom |
| `bf4ee62` | **Härtung**: stille Falschwerte, Abbrüche, Client-Divergenz — siehe „Gefundene Defektklassen" |

### Multi-Unit (26.07.)

| Etappe | Commit | Was sie gebaut hat |
|---|---|---|
| 1 | `c1bc9de` | FDM instanzfähig — `FBFdm` als Objekt, keine globale Instanz |
| 2 | `c08a168` | der Akteur ist EIN Objekt (`units/FBSimUnit`) |
| 3 | `2c03704` | der Verband ist Missionsdaten — zwei Jets fliegen |
| 4 | `6d7ed5a` | Thread pro Einheit im Gym, Lockstep-Barriere, bit-identisch |
| 5 | `9190e7c` | Datalink — Einheiten sehen einander über ein System |
| 6 | `4049a7b` | FCR-Radar mit ACM-Modi, anonyme Kontakte, IFF |
| 7 | `b375bef` | BFM-Manöver-KI — fliegt allein auf Radarkontakten |
| 8 | `071ea2b` | Avionik-Datenmodell: Ausgabeblöcke mit Gültigkeit + Kommandobus |

### Wissensbasis (26.07.)

`2dd1142`, `e22f228`, `c4e96e7` — die offizielle ED-Dokumentation destilliert nach `doc/f16/`.
`weapons.md` und `defence-rwr-cm.md` von SHALLOW auf FULL; `controls-commands.md` neu als Vorlage der
Kommandoblöcke.

### Waffen, Schaden, Taktik (27.07.)

| Commit | Abschnitt |
|---|---|
| `b62c769` | Waffen-Fundament: die Waffe ist eine eigene Einheit mit eigenem FDM |
| `5c68fc5` | AIM-120 mit Sucher, Lenkung und Datalink-Führung |
| `439f53a` | RWR und Gegenmaßnahmen — wer merkt, dass er gesehen wird |
| `1ecd433` | Intercept-KI: BVR-Taktik — führen, schießen, stützen, verteidigen |
| `6d84647` | Schadensmodell: Treffer werden Systemausfälle, Ausfälle werden Ungültigkeit |
| `82df2e2` | Kampfziele und evolutionäre Turniere |
| `a1a8fbf` | Bordkanone M61A1: hergeleitete Ballistik, EEGS-Trichter, kinetischer Schaden |
| `1eeff72` | Luft-Boden: Bodenziele ohne FDM, CCIP/CCRP aus einer Integration |

### Verfeinerung der KI (27.07., laufend)

| Commit | Abschnitt |
|---|---|
| `cac7b62` | Piloten-Gedächtnis: das Datum statt des letzten Messpunkts; Kanonen-Nachführung mit Ratenanteil; Rollraten-Regler |
| `9673e00` | Führung hält eine Bahn, wo eine Bahn deklariert ist — Querfehler und Wegpunkt-Fang |

### Eine Modellwurzel und die Delta-Regel (27.07.)

**Was gebaut.** Alle geflogenen JSBSim-Modelle liegen jetzt unter `sim/assets/aircraft` — `f16`
(inkl. `Systems/` und der beiden referenzierten Engine-XML, die als `f16/engine/` ins Modellverzeichnis
gewandert sind: JSBSims eigenes Pro-Flugzeug-Layout, das seine Loader zuerst durchsuchen), `mk82`, und
die schon dort liegende `aim120`. `FBModelRoots` hat EINE Wurzel, `FBModule::FdmModelVendored()` und
`FBStoreSpec::Vendored` sind ersatzlos entfallen, `FBFdm`s Engine-/Systems-Sondierung (`stat` + Parent-
Trunkierung) ist zu zwei bedingungslosen Pfaden geworden, und der WASM-Build embedded eine Wurzel statt
fünf Einzelpfade.

**Warum.** Prinzip 1 ist von „nie gepatcht" zur **Delta-Regel** geworden: das gepinnte Submodul ist die
Basis, die Kopie fliegt, und jede Abweichung ist ein benannter, belegter Eintrag in
`sim/assets/MODEL-DELTAS.md` — ein besseres Missionsergebnis ist ausdrücklich kein Beleg. Das Gate ist
`make -C sim verify-models` (`sim/tools/verify_models.py`): kanonischer Unified-Diff je Datei,
zeichenweise gegen den Diff-Block des Eintrags. Bewusst kein `patch`/`git apply` — eine Anwendung mit
Fuzz könnte eine Abweichung verschlucken.

**Gemessen.**

| Prüfung | Ergebnis |
|---|---|
| Regression 50 Missionen | **121/121 Telemetriedateien byte-identisch**, alle 50 Exit-Codes gleich, `events.log` identisch bis auf Ausgabepfad und Wall-Clock |
| `verify-models` | grün (4 upstream-gedeckte Pfade, 0 Deltas, 1 FlightBox-eigenes Modell) |
| Negativtest | ein geändertes Byte in `f16.xml` → rc=1 mit dem fehlenden Block; ebenso ein deklarierter, aber nicht vorhandener Delta, ein nicht passender Diff und ein nicht deklariertes Modellverzeichnis |
| Harnesses | alle sieben rc=0; Corner-Speed unverändert 380 KCAS / 16,2214 °/s |
| Determinismus | 5 Missionen × `--threads 1/2/4` × 2 Wiederholungen = je eine Signatur |
| WASM | baut; JSBSim lädt `f16` aus dem eingebetteten `/fb/aircraft` und trimmt (`trimConverged=1`) |
| Frame | `gpu_native --mission payerne-takeoff --interval 20` → 28 PNGs, Gelände + HUD |

## Gefundene Defektklassen

Was der Regelkreis an Fehlern zutage gefördert hat, die eine Inspektion nicht gefunden hätte. Die Liste
ist Warnung und Prüfmuster zugleich.

| Klasse | Konkreter Fall |
|---|---|
| **Stille Falschwerte** | `ApplySetup` gab 0.0 für unparsbaren Text zurück und meldete Erfolg. Eine HTML-Fehlerseite vom `/elev`-Endpunkt wurde als Meereshöhe gecacht — eine ganze 216-s-Mission flog über Meereshöhe und meldete SUCCESS. |
| **Fehlende Divergenzprüfung** | 16 injizierte NaN-Fälle liefen alle mit `tripped=0` durch. |
| **Ungeschützte Aufrufe** | ungeprüfte JSBSim-Aufrufe → `std::terminate`, Exit 134. |
| **Fehlende Header-Abhängigkeiten** | Das Makefile hatte kein `-MMD -MP`: veraltete Objekte, Phantommessungen. Bewiesen durch eine absichtliche Header-Änderung, die einen Telemetrie-Hash veränderte und danach wiederherstellte. |
| **Architektur-Leck** | `FBFlightMonitor` kannte Runways. Physik-K.O. und Missions-Urteil wurden getrennt. |
| **Modul-Spezifika im generischen Code** | F-16-Referenzen im `FBFlightMonitor`; Grenzwerte werden jetzt vollständig aus dem Modell abgeleitet. |
| **Nicht-Determinismus durch Reihenfolge** | Log-Zeilenposition hing am Scheduler. Gelöst über Merge-Reihenfolge statt Locks. |
| **Zwei Kopien derselben Daten** | `sim/web/missions/*.fbm` war eine handgepflegte Kopie im alten Format — die WASM-App blieb schwarz. Jetzt Build-Kopie. |
| **Aliasing durch Taktraten** | Der Sucher schaute mit 20 Hz auf Posen, die mit 10 Hz publiziert wurden: 446 m/s gemessen statt 654 m/s. Gelöst über ein Dwell-Fenster statt zweier Einzelmessungen. |
| **Zombie-Zustand** | Ein detonierter Flugkörper strahlte 74 s nach seiner Detonation weiter. `Retire()` leert jetzt die Signatur. |
| **Falsche Regelgröße** | Ein reiner P-Regler gegen eine Rampe (die Kanonenlösung gegen einen kurvenden Gegner) parkt bei Rampenrate × Zeitkonstante. Ein Punktregler gegen eine Bahn hat stationären Querversatz. Beides ist Regelungstyp, nicht Tuning. |
| **Veraltete Dokumentation im Datenfile** | Zwei Missionsköpfe dokumentierten noch „endet im Timeout", nachdem beide Läufe zu Abschüssen geworden waren. Der Kopf trägt die Leseregel und muss mitgeführt werden. |

### Documentation: the spec-driven restructuring (27.07.)

**What it built.** `doc/flightbox/` moved from "one file per subsystem plus a central TODO" to a
spec-driven shape: every topic file now carries `## Spec` / `## State` / `## Gaps` / `## Knowledge`,
grouped into `sim/`, `aircraft/`, `render/`, `clients/`. New: `vision.md` (the direction),
`roadmap.md` (R1–R10, thin, pointing at the Spec each stage must satisfy), `aircraft/mig29.md` and
`render/units-visual.md` (both spec-only, nothing built), `aircraft/stores.md`,
`clients/clients.md`. `PROGRESS.md` became this file; `TODO.md` dissolved into the Gaps sections of
the files it belonged to, plus `roadmap.md`/`vision.md`. `render/rendering.md` split into
`renderer.md` + `hud.md` + `clouds.md` (whose Spec is the owner-approved rebuild, including the
cirrus layer) + `units-visual.md`.

**Rule change.** The maintenance obligation is now spec-first: change the Spec, build until State
meets it, then update State/Gaps and add a line here (`conventions.md`). There is no second list of
open work any more.

**Not done.** Existing bodies stay German for now (each file says so); the translation wave plus the
schema alignment of `doc/f16/` and `doc/mig29/` is roadmap R10. `world-and-terrain.md` stays at its
old path until the `/wx` round lands, then splits into `world/terrain.md` + `world/weather.md`.
