# FDM-Adapter — `sim/src/fdm/`

> Body still in German — translation pass pending (see [roadmap](../roadmap.md)).

**Quellen dieser Datei:** die Kommentar-Banner der sieben Dateien in `sim/src/fdm/`
(`FBFdm.h`, `FBFdm.cpp`, `FBFdmBoot.h`, `FBFdmBoot.cpp`, `FBFdmTelemetrySource.h/.cpp`, `em_compat.h`),
`sim/src/app/FBTestTwoFdm.cpp` (der Koexistenz-Beweis), `sim/src/core/FBDamageModel.h` (die
Folge-Konstanten) und CLAUDE.md. Zahlen ohne Quellenangabe stehen so im Code; Herleitungen sind als
solche markiert, Setzungen mit `[SET]`.

Gegenstand: die **einzige** Naht zwischen FlightBox und der gepinnten JSBSim-Engine
(`sim/vendor/jsbsim`, read-only, CLAUDE.md Prinzip 1). Alles über dieser Naht sieht ein flaches POD und
eine Klasse; niemand sieht `FGFDMExec`.

---

## Spec

The **only** seam between FlightBox and the pinned JSBSim engine. Everything above it sees a flat POD
plus one class; nobody sees `FGFDMExec`.

| Contract | Acceptance / measurement anchor |
|---|---|
| One translation unit includes JSBSim headers | `FBFdm.cpp`, and nothing else |
| An `FBFdm` is one aircraft — instance-capable, no static mutable globals | `make -C sim test-fdm` → two coexisting FDMs with independent physics |
| Initial conditions are structurally sealed off | loading constructor private, single friend `FBFdmBoot`, no re-init/reset; only `app/` files name `FBFdmBoot.h` |
| Borrowed handles cannot cheat | every command method non-const, every readback const |
| The pinned model is the truth; a copy may deviate only as a declared delta | `make -C sim verify-models` green (`../build-and-ops.md`) |
| Carriage and damage act through model-owned JSBSim APIs, never by patching model XML | point masses + a named `fb-stores` external force; `fb-damage` force, throttle cap/cutoff, control authority |
| Neutral until something happens | a clean, undamaged jet computes bit-identically to one that never heard of stores or damage (measured) |

## State

Built and closed. Seven files.

| Piece | Status | Anchor |
|---|---|---|
| Instance-capable adapter, pimpl, no globals | built | `c1bc9de` |
| IC lockdown (`FBFdmBoot`) | built | `c08a168` |
| Stores carriage: point masses + external force | built | `b62c769` |
| Damage channels: control authority, throttle cap, drag | built | `6d84647` |
| Single model root + delta rule | built | model-root round (see `../journal.md`) |
| `FBFdmTelemetrySource` (raw FDM pose) | built | `e4d7c26` |

Regression evidence for the last change: 121/121 telemetry files byte-identical over 50 missions, all
seven harnesses rc=0, corner speed unchanged at 380 KCAS / 16.2214 °/s.

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `fdm/FBFdm` | count of process-wide JSBSim state: `CLAUDE.md` says three, `FBFdm.cpp` lists four. The code is authoritative. |

### Inventory (German, from the previous `Offene Punkte` section)

- **Widerspruch in der Zahl der prozessweiten JSBSim-Dinge.** CLAUDE.md sagt „die drei in `FBFdm.cpp`
  dokumentierten Dinge", `FBFdm.cpp` listet **vier** Punkte (debug_lvl, Logger, `Element::convert`,
  `JSBSIM_*`-Env). Die Liste in `FBFdm.cpp` ist die maßgebliche. Der vierte Punkt ist streng genommen
  kein eigener Zustand, sondern ein Schreiber auf den ersten — vermutlich die Quelle des
  Zählunterschieds. (Die abweichende Zwei-Behauptung im `FBFdm.h`-Banner ist mit der Kommentar-Runde
  entfallen, ebenso dessen veraltetes Ownership-Banner.)
- **`GetGroundClearanceM` bei `gearDown=false`** überspringt einziehbare Kontakte, aber nicht solche,
  die zwar `ctSTRUCTURE` sind und trotzdem einziehbar deklariert wurden — Konsequenz für Modelle mit
  ungewöhnlicher Kontaktdeklaration ist nicht geprüft.
- **`GetGearPos`/`GetSpeedbrakePos` klemmen nicht**: sie geben die Modell-Property roh weiter. Ein
  Modell, das außerhalb [0,1] fährt, würde das nach oben durchreichen. Bisher kein Fall.
- **Es gibt keinen Readback für `Authority`/`ThrottleMax`/`DamageCdA`.** Die Schadenswirkung ist damit
  nur über ihre Folgen (Telemetrie/Verhalten) beobachtbar, nicht direkt. `core/FBSystemHealth`s
  `dmg_*`-Spalten decken den ZUSTAND ab, nicht den angewandten Faktor.
- **Es gibt keinen Readback für `ElevTrim`.** Der Trimm-Bias ist nur im `loaded`-Logeintrag sichtbar,
  danach nicht mehr abfragbar.
- **`SetStoresDrag` mit `cdaFt2 <= 0` NACH einmaliger Anlage** setzt die Magnitude auf 0, entfernt die
  Kraft aber nicht (JSBSims `FGExternalReactions` kennt kein Entfernen). Dokumentiert wirkungsgleich,
  aber die Kraft bleibt in der Modellstruktur bestehen.
- **Ungeprüft in dieser Runde:** die konkreten Aufrufer der Tank-Setter (`FBModule::ApplySetup`-Keys
  `fuel_lbs`/`fuel_pct`) wurden nicht gelesen; die Schlüsselnamen stammen aus dem `FBFdm.h`-Banner und
  aus CLAUDE.md, nicht aus `doc/mission-format.md`.


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 1. Dateien

| Datei | Rolle |
|---|---|
| `fdm/FBFdm.h` | Die öffentliche Naht: `struct fb_fdm_state` (POD) + `class FBFdm`. Nennt KEINEN JSBSim-Typ. |
| `fdm/FBFdm.cpp` | Die EINE Übersetzungseinheit mit JSBSim-Headern. Enthält `struct FBFdm::Impl` (pimpl) mit der `FGFDMExec`. |
| `fdm/FBFdmBoot.h` | `struct FBFdmSpawn` (die IC als Daten) + `class FBFdmBoot` — der einzige Friend von `FBFdm`s privatem Lade-Konstruktor. |
| `fdm/FBFdmBoot.cpp` | `FBFdmBoot::Spawn` — die einzige Stelle, an der ein `FBFdm` entsteht. |
| `fdm/FBFdmTelemetrySource.h/.cpp` | `FBTelemetrySource` für die rohe FDM-Pose (10 Spalten). |
| `fdm/em_compat.h` | Build-Shim für JSBSim unter emscripten/musl (`strerror_r`). Force-Include NUR für JSBSim-Quellen. |

---

### 2. Die Ein-TU-Naht

**Vertrag.** `FBFdm.cpp` ist die einzige Übersetzungseinheit im ganzen Baum, die einen JSBSim-Header
inkludiert. Jeder Aufrufer (jedes Modul, jedes System, jede Telemetriequelle, jeder Client) sieht
ausschließlich:

- `fb_fdm_state` — flaches POD, snake_case-Name, weil es der `FBModule::Run`-Vertrag ist, gegen den
  jedes Modul und jede Telemetriequelle geschrieben ist;
- die Methoden von `FBFdm`.

**Mechanik.** Die Engine liegt hinter einem pimpl (`std::unique_ptr<Impl> P`, `Impl` trägt die
`FGFDMExec` by value). Daraus folgt eine bewusste Abweichung von der FB-Konvention „Getter inline im
Header": die Getter **können** nicht inline sein — der Header darf den Typ nicht nennen, aus dem sie
lesen.

**Was das garantiert.**

| Garantie | Warum |
|---|---|
| Ein Build-Bruch in JSBSim trifft genau eine `.o`-Datei | Nur diese TU kennt die Header. |
| Kein System/Modul kann an der Naht vorbei ins Property-Tree schreiben | `SetPropertyValue` ist nirgends sonst erreichbar. |
| Die Liste aller berührten JSBSim-Properties ist endlich und steht im Header | `FBFdm.h` nennt zu jeder Methode die Property, die sie schreibt/liest. |
| Exceptions bleiben lokal | Die Firewall (§7) sitzt in dieser TU; darüber gibt es nur `bool` und `Faulted()`. |

#### `fb_fdm_state` — Felder und Einheiten

| Feld | Einheit | JSBSim-Property |
|---|---|---|
| `roll`, `pitch`, `yaw` | deg (φ/θ/ψ) | `attitude/phi-deg`, `theta-deg`, `psi-deg` |
| `p`, `q`, `r` | deg/s, Körperraten | `velocities/[pqr]-rad_sec` × rad→deg |
| `lat`, `lon` | deg, **geodätisch** | `position/lat-geod-deg`, `position/long-gc-deg` |
| `elev` | m ASL | `position/h-sl-ft` × ft→m |
| `speed` / `gs` / `cas` | m/s (TAS / Ground / **dichtekorrigiert** CAS) | `velocities/vt-fps`, `vg-fps`, `vc-fps` |
| `mach` | — | `velocities/mach` |
| `vx`, `vy`, `vz` | m/s, **X-Plane-local**: +x Ost, +y hoch, +z Süd | `v-east-fps`, `−v-down-fps`, `−v-north-fps` |
| `nx`, `ny`, `nz` | g, Körperlastfaktoren (long/lat/normal) | `accelerations/N[xyz]` |
| `alphaDeg` | deg | `aero/alpha-deg` |

Die `vx/vy/vz`-Konvention ist ein **Erbstück der Vor-Pivot-Bridge** und bleibt bewusst, weil Renderer-
und HUD-Mathematik sie bereits konsumieren. Jeder Konsument außerhalb (z. B.
`app/FBMissionBoot.h::FBMissionSpawnStore`, `FBMissionRunner.cpp::GroundCrossing`) rechnet sie explizit
in NED/ENU um und sagt das im Kommentar.

`cas` ist dichtekorrigiert — die ehrliche „wie nah am Stall"-Größe auf jeder Platzhöhe.

---

### 3. Instanzfähigkeit

**Vertrag.** `FBFdm` ist EIN simuliertes Flugzeug. Beliebig viele Objekte koexistieren im selben Prozess
mit unabhängiger Physik; jede `FGFDMExec(nullptr)` allokiert ihren **eigenen** `SGPropertyNode`-Root und
ihren eigenen FDM-Zähler. Keine statischen mutablen Globals im Adapter (grep-verifizierbar).

**Beweis:** `make -C sim test-fdm` → `build/fb-test-two-fdm` (`app/FBTestTwoFdm.cpp`). Der Harness
behauptet drei Dinge und prüft sie:

1. zwei Zellen laden und trimmen unabhängig (zwei `FGFDMExec`, je eigener Property-Tree);
2. ihre Zustände **divergieren gemäß ihren eigenen Kommandos** (A rollt rechts, B links) — also liest/
   schreibt keine die Physik der anderen;
3. eine DRITTE Zelle mit derselben IC und denselben Kommandos reproduziert A **bit-für-bit** — also
   trägt eine Instanz keinen versteckten Cross-Talk ihrer Nachbarn.

Exit 0 = bewiesen, 1 = nicht unabhängig, 2 = Setup-Fehler. Kein Threading, keine Unit-Liste: diese Stufe
beweist nur Instanzfähigkeit.

#### Was in JSBSim SELBST prozessweit bleibt

Verifiziert gegen `vendor/jsbsim` am gepinnten Commit; im Banner von `FBFdm.cpp` dokumentiert. **Vier
Punkte**, keiner davon trägt Physikzustand:

| Prozessweit | Was es ist | Warum unkritisch |
|---|---|---|
| `FGJSBBase::debug_lvl` | statisches Debug-Level, `SetDebugLevel()` wirkt für ALLE Instanzen | Jede `FBFdm` setzt es im Konstruktor auf 0, also kann keine Instanz eine andere überraschen. Wird nur aus einem Ctor und aus `FGFDMExec`s Child-FDM-Trim-Pfad geschrieben (den FlightBox nicht benutzt), und ist für die gesamte `Run()` read-only. |
| `JSBSim::SetLogger`/`GetLogger` (`input_output/FGLog.cpp`) | EIN Logger — am gepinnten Commit `thread_local` | Per-INSTANZ-Logrouting ist damit unmöglich, aber zwei Threads teilen sich nie einen Logger. FlightBox setzt ihn nie; JSBSims eigene Ausgabe bleibt bei Debug 0 aus. |
| `Element::convert` (`input_output/FGXMLElement.cpp`) | statische Einheiten-Konvertierungstabelle, lazy aus dem `Element`-Ctor gefüllt und mit `operator[]` gelesen — das **einfügen kann** | Wird ausschließlich beim **LADEN** eines Flugzeugs berührt. Genau deshalb spawnt `fb-gym`s paralleler Pfad seine Einheiten sequenziell und parallelisiert nur den STEP (`app/FBTickPool.h`). |
| `JSBSIM_DEBUG` / `JSBSIM_DISPERSE` (Env-Variablen) | im `FGFDMExec`-Ctor in denselben geteilten Static gelesen | Gilt für den ganzen Prozess, nicht für eine Zelle. |

**Konsequenz für Nebenläufigkeit:** keiner der vier ist aus `Step()` erreichbar, also dürfen N Zellen
KONKURRENT integrieren, ein Thread je Zelle. Das interne Reference-Counting der Engine (`SGReferenced`)
ist NICHT atomar — was genau deswegen unschädlich ist: jeder Property-Node und jedes `Element` hängt an
dem einen `FGFDMExec`-Root seiner Instanz und wird nie geteilt.

---

### 4. Ownership

| Rolle | Handle | Regel |
|---|---|---|
| Besitzer | `std::unique_ptr<FBFdm>` | Wer die Einheit besitzt, besitzt ihre Zelle — heute `units/FBSimUnit` (deklariert VOR dem Modul, damit die Zelle das Modul überlebt, das sie nur borgt). |
| Kommandierender Borger | `FBFdm&` | Jede Kommando-Methode ist **nicht-const**. |
| Lesender Borger | `const FBFdm&` / `const FBFdm*` | Jeder Readback ist **const**. |

Die Konstheit ist tragend, nicht kosmetisch: ein Lese-Handle **kann** die Physik nicht schreiben — das
ist CLAUDE.mds „Kein Cheaten", durchgesetzt vom Typsystem statt per Konvention.

`FBFdm` ist nicht kopierbar (`= delete` auf Copy-Ctor und -Zuweisung).

**`FBModule::AttachFdm(FBFdm&)` ist der Konstruktor-Injektions-Ersatz.** Module werden von
`FBModuleRegistry` **argumentlos** gebaut (Name → Factory), also gibt es keinen Konstruktor, in den eine
Zelle hineingereicht werden könnte. `AttachFdm` wird vom Besitzer GENAU EINMAL gerufen — direkt nach dem
Spawn, vor dem ersten `Run()` — und ist von da an dauerhaft. Es ist zugleich der Punkt, an dem das Modul
eine echte `FBFdm&` an die Systeme weiterreichen kann, deren Zuordnung zu einer Zelle FIX ist
(`FBJsbsimAirframeControls`, konstruktor-injiziert).

Das Modul besitzt nie eine Zelle und kann keine erzeugen — es kann nicht: die IC liegt hinter
`fdm/FBFdmBoot.h`, das kein Modul inkludiert (§5).

---

### 5. IC-Abschottung

**Struktur, nicht Konvention:**

| Element | Wirkung |
|---|---|
| `FBFdm::FBFdm()` + `FBFdm::Load()` sind **privat** | Niemand kann eine Zelle konstruieren oder laden. |
| `friend class FBFdmBoot` — der EINZIGE Friend | Genau ein Produzent. |
| `FBFdmBoot` steht in einem **separaten Header** | Wer `FBFdm.h` inkludiert (jedes Modul/System, um ein `FBFdm&` zu halten), erreicht KEINE IC. Wer IC will, muss `FBFdmBoot.h` NAMENTLICH nennen. |
| Es gibt **kein** `Init`/`Reset`/`Respawn` auf `FBFdm` | Eine geborgte Referenz kann die Zelle nicht neu platzieren, neu trimmen oder neu spawnen. |
| `Spawn()` gibt `nullptr` zurück, wenn das Modell nicht lud | Eine `FBFdm`, die existiert, ist IMMER eine geladene — kein Aufrufer und keine Methode braucht einen „nicht initialisiert"-Zweig. |

**Wer `FBFdmBoot.h` nennen darf:** nur `app/` — `app/FBMissionBoot.h`, `app/FBAppWasm.cpp` und die
Test-Harnesses. `grep -rn FBFdmBoot src/systems src/modules` ist leer und **kann nicht still aufhören,
leer zu sein**: der Compiler erzwingt es, weil der Konstruktor privat ist.

Zweite Stufe derselben Schranke: ein `units/FBSimUnit` lässt sich nur aus einer bereits gespawnten
`FBFdm` bauen — also ist `FBMissionBoot.h` auch der einzige Produzent eines vollständigen Akteurs.

#### `FBFdmSpawn` — die IC als Daten

| Feld | Bedeutung |
|---|---|
| `ModelsRoot` | die EINE Modellwurzel. native/gym `assets/aircraft`, WASM der eingebettete FS-Pfad `/fb/aircraft` (→ `app/FBModelRoots.h`). |
| `Aircraft` | Modellverzeichnis + XML-Name unter `ModelsRoot`. |
| `LatDeg`, `LonDeg` | **geodätisch** (passt zu GPS/HOME_LAT). |
| `GroundElevM` | aufgelöste Bodenhöhe unter dem Spawnpunkt, m ASL. |
| `HeightOffsetM` | `< 0` = **auf dem Fahrwerk sitzen** (Modell-eigene Gear-Down-Clearance); `>= 0` = so viele Meter über `GroundElevM`. `0` fällt auf provisorische **3 m** zurück (Luftstart ohne expliziten Offset). |
| `SpeedMs` | kalibrierte Fluggeschwindigkeit. `0` = stehender Bodenstart → **keine** Trimmsuche. |
| `HeadingDeg` | Kurs; negativ wird +360 gerechnet. |
| `FbwOverride` | setzt `fcs/fbw-override` = 1, überbrückt die modelleigene FLCS. |
| `Ballistic` + `PitchDeg`/`RollDeg` + `VelNorthMs`/`VelEastMs`/`VelDownMs` | Die **Abwurf-IC** (§6). Bei `Ballistic == false` vollständig ignoriert. |

**Ein IC-Anwendung je Zelle.** `Spawn()` legt Position/Lage/Geschwindigkeit **gemeinsam** an — für den
Bodensitz genauso wie für die explizite Luft-Höhe. Es gibt keinen zweiten, getrennten Luft-Codepfad und
kein Re-Init danach.

---

### 6. Der Ladeablauf (`LoadUnguarded`)

Schritt für Schritt, weil jeder Schritt eine Begründung trägt:

1. **Pfad-Auflösung Engine/Systems.** `<root>/<aircraft>/engine` und `<root>/<aircraft>/Systems`,
   bedingungslos. Jedes FlightBox-Modell ist unter seinem eigenen Verzeichnis vollständig — genau das
   Layout, das JSBSims eigene Loader ZUERST durchsuchen (`FGPropulsion::FindEngineFullPathname` und
   `FGFCS::FindFullPathName` probieren `<aircraft>/engine` bzw. `<aircraft>/Systems` vor jedem
   übergebenen Pfad). Die frühere Sondierung (existiert `<ac>/engine`? sonst `<parent>/engine`) samt
   Parent-Trunkierung ist mit der einen Modellwurzel entfallen; ein Modell ohne Triebwerk (`mk82`) löst
   den Pfad schlicht nie auf.
2. **IC setzen:** Geod-Lat/Lon, ASL-Höhe = `GroundElevM + max(HeightOffsetM, 3 m)`, Psi.
3. **Ballistic** (Abwurf): Theta/Phi direkt aus der Trägerlage, plus der volle NED-Geschwindigkeits-
   VEKTOR. **Sonst**: kalibrierte Geschwindigkeit + Flugbahnwinkel 0 (level).
4. `RunIC()`.
5. **Bodensitz-Nachkorrektur** (`HeightOffsetM < 0`): jetzt ist der CG gültig, also wird auf die
   Modell-Gear-Down-Clearance (`GetGroundClearanceM(true)`, Schwelle > 0,1 m) neu platziert und `RunIC()`
   wiederholt — die Spawnhöhe ist damit die geometriewahre Radhöhe, ohne Sprung beim ersten Schritt.
6. **Triebwerke starten** (`FGPropulsion::InitRunning` für jeden Index) — **außer bei `Ballistic`**.
   Begründung im Code: ohne laufendes Triebwerk gibt es während `FGTrim` keinen Schub, also meldet eine
   angetriebene Zelle „udot not trimmable", die IC ist kein Gleichgewicht, und die ungetrimmte Zelle
   departed beim ersten Schritt violent. Für einen abgeworfenen Store ist es das Gegenteil und
   **nicht kosmetisch**: `InitRunning` schlägt den Throttle auf 1 und marschiert das Triebwerk in einen
   stationären Zustand — bei einem FESTSTOFFMOTOR (`FGRocket`: Zündung = Throttle == 1, und einmal
   gezündet brennt er bis zur Erschöpfung) hieße das, der Motor brennt schon in der IC und kein Kommando
   könnte ihn halten. Ein ungetriebener Store hat keine Triebwerke, also ist das für jeden Store, der vor
   der ersten Rakete flog, bit-identisch.
7. `fcs/fbw-override` = 1, falls verlangt. `Setdt(kStepS)`.
8. **Trimm** — nur wenn `SpeedMs > 0` UND nicht `Ballistic`. Modus `tLongitudinal` (Pitch/Throttle/
   Alpha, Flügel level): robuster als `tFull` auf leichten/langsamen Zellen. In `try/catch`, ein Wurf =
   nicht getrimmt.
   - **Warum V=0 nicht getrimmt wird:** null Fluggeschwindigkeit = null Aero-Kraft/Moment, also kann
     keine Ruderstellung `udot`/`qdot` nullen. Früher lief `FGTrim` trotzdem, meldete „not trimmable" und
     ließ `ElevTrim` auf der letzten Iterierten der gescheiterten Suche stehen (Rauschen). Jetzt wird
     neutral gesetzt — was dem ungetrimmten Knüppel eines echten Jets vor dem Roll entspricht.
9. **`ElevTrim` festhalten:** der Höhenruderausschlag, auf den der Trimmer sich gesetzt hat, wird als
   **Trimmruder-Bias** gespeichert (`fcs/elevator-cmd-norm` nach dem Trimm; 0 bei V=0). Damit hält
   neutraler Knüppel LEVEL statt der Nase-hoch-Lage der Zelle bei Neutral.
10. Abschließendes `RunIC()` — saubere, ebene IC (Lage + Speed); gehalten wird sie vom Trimmruder, nicht
    vom perturbierten Suchzustand.
11. `FBLog::Info("fdm","loaded", …)` in drei Varianten (ballistisch / getrimmt / Bodenstart).

---

### 7. Schritt, Exception-Firewall, `Faulted()`

`static constexpr double FBFdm::kStepS = 0.01` — **100 Hz**, die EINE Definition dieser Rate. Der
Substep-Akkumulator des Moduls und die Test-Harnesses lesen sie hier, statt `0.01` zu wiederholen.

`Step(fb_fdm_state &out)`: einen festen Schritt vorrücken, dann den Zustand nach `out` lesen.

**Firewall.** JSBSim wirft `JSBSim::BaseException` (ein `std::runtime_error`) aus dem XML-Parsen und
`FGJSBBase::FloatingPointException` aus Tabellenauswertungen. Ungefangen tötet eine kaputte
`aircraft.xml` den Prozess mit `std::terminate`, und die Missionsschleife bekommt nie eine `RESULT`-Zeile
zum Verzweigen. Deshalb:

| Ebene | Guard |
|---|---|
| `Load` | umschließt `LoadUnguarded` (XML-Parsen, IC, Trimm, Triebwerksstart) |
| `Step` | umschließt `StepUnguarded` |
| `FBFdmBoot::Spawn` | umschließt das EINZIGE, was außerhalb liegt: die Konstruktion des Engine-Objekts selbst (`FGFDMExec`-Ctor allokiert Property-Root, liest `JSBSIM_*`) |

Gefangen wird über `std::exception` (JSBSims Hierarchie leitet davon ab) plus Catch-All — so muss hier
kein JSBSim-Typ genannt werden. **In WASM identisch:** libJSBSim aus dem Submodul, diese TU und der
finale Link tragen alle `-fexceptions` (`vendor/build_jsbsim_wasm.sh`, das `wasm`-Make-Target) — die EINE
Stelle, an der Exceptions eingeschaltet sind, und die Firewall sitzt darin.

**`Faulted()` ist gelatcht:** ist der Integrator einmal gestiegen, ist die Physik dieser Zelle vorbei.
Jeder spätere `Step` ist ein No-Op, `out` behält seine letzten guten Werte (nie halb geschrieben), der
Aufrufer liest einen eingefrorenen aber ENDLICHEN Zustand. Der App-seitige Richter
(`core/FBFlightMonitor` über `FBFlightMonitorSample::FdmFault`) macht daraus ein
`NumericalDivergence`-K.O. Das Modul sieht davon nichts.

---

### 8. Kommandokanäle

Der einzige Weg, auf dem irgendetwas über dieser Klasse die Physik beeinflusst (die simulierte
Steuerfläche — CLAUDE.md „Kein Cheaten").

| Methode | JSBSim-Property | Bereich / Anmerkung |
|---|---|---|
| `SetControls(roll,pitch,yaw,thr)` | `fcs/aileron-cmd-norm`, `fcs/elevator-cmd-norm`, `fcs/rudder-cmd-norm`, `fcs/throttle-cmd-norm` | roll/pitch/yaw ∈ [−1,1], thr ∈ [0,1] |
| `SetGear(cmd)` | `gear/gear-cmd-norm` | [0,1], 1 = unten; der Kinematik-Transit ist der modelleigene `flight_control`-Kanal |
| `SetFlap` / `SetSpeedbrake` | `fcs/flap-cmd-norm`, `fcs/speedbrake-cmd-norm` | [0,1] |
| `SetWheelBrakes(l,r)` | `fcs/left-brake-cmd-norm`, `fcs/right-brake-cmd-norm` | je [0,1], geklemmt |
| `SetNosewheelSteer(cmd)` | `fcs/steer-cmd-norm` | [−1,1], generisch in `FGGroundReactions` verdrahtet; wie viel Ausschlag daraus wird, sagt der Gain-Block der jeweiligen `aircraft.xml` — diese Klasse behauptet nur das Pilotenkommando |
| `EngineStart()` | `propulsion/cutoff_cmd`=0, `starter_cmd`=1 | propulsions-weit (`FGPropulsion::SetStarter/SetCutoff` gelten default für ALLE Triebwerke) |
| `EngineCutoff()` | `starter_cmd`=0, `cutoff_cmd`=1 | dito |

#### Drei Eigenheiten in `SetControls`

1. **Schadenswirkung sitzt HIER**, zwischen kommandierendem System und Physik: `roll/pitch/yaw`
   werden mit `Authority` skaliert, `thr` auf `ThrottleMax` gedeckelt. Beide sind 1.0 auf einer
   unbeschädigten Zelle — also Arithmetik ohne Wirkung, bis wirklich etwas abgeschossen wurde.
2. **Throttle wird gerampt, nicht gestuft.** `kEscSpinupS = 0.5 s` ⇒
   `kThrottleSlew = kStepS / kEscSpinupS = 0.01/0.5 = 0.02` pro Schritt (voller Hub 0→1 in 0,5 s).
   Begründung im Code: ein 0→0,95-Sprung sprengt die RPM-ODE des Triebwerks und departed die Zelle.
3. **Vorzeichen und Trimm.** JSBSims `+elevator` = Nase RUNTER, FlightBox' `+pitch` = Nase HOCH, also
   geht `-pitch + ElevTrim` hinaus. `+yaw` koordiniert die Kurve, `−yaw` schiebt sie (gemessen: starkes
   adverses Giermoment).

---

### 9. Außenlasten (Carriage)

Zwei Mechanismen, **beide modell-eigen**, beide zur LAUFZEIT bestückt statt per Modell-XML-Patch —
`vendor/jsbsim` bleibt read-only (Prinzip 1), und kein Modell bekommt eine Station, die es nicht hatte.

| Methode | Mechanismus | Wirkung |
|---|---|---|
| `AddStorePointMass(name, xIn, yIn, zIn) → index` | `FGMassBalance::AddPointMass` (die `<pointmass>`-Mechanik, von der die F-16 genau eine deklariert: ihren Piloten) | Masse, Schwerpunkt UND der r²-Trägheitsbeitrag kommen aus der Engine |
| `SetStorePointMassLbs(index, lbs)` | `inertia/pointmass-weight-lbs[i]` | die Masse ändert sich auch beim Abwurf (auf 0) — deshalb fliegt ein beladener Jet anders als ein sauberer und ein entladener anders als beide |
| `SetStoresDrag(cdaFt2, xIn, yIn, zIn)` | eigene `<external_reactions>`-Kraft `fb-stores` | Kraft `CdA · qbar` entlang der Körper-**−x**-Achse, angreifend am Schwerpunkt der belegten Stationen — das MOMENT einer außermittigen Last kommt damit aus derselben Physik wie die Kraft |

**Koordinaten:** strukturelle Zoll (`IN`) im Frame, in dem die `aircraft.xml` bereits Pilot, Fahrwerk und
Tanks platziert.

**Index-Entdeckung statt Mitzählen.** Nach `AddPointMass` ist unsere Masse die letzte; der Index wird aus
dem Property-Tree gelesen (erster Index, dessen `inertia/pointmass-weight-lbs[n]` NICHT existiert, ist
die Anzahl; `n−1` sind wir). Damit ist der Adapter generisch über jedes Modell, egal wie viele
Punktmassen dessen XML mitbrachte.

**`EnsureDragForce(name, x, y, z)` — die geteilte Hilfsroutine** (Carriage und Battle Damage brauchen sie
beide, deshalb ist sie eine Funktion und kein Block). Sie legt eine benannte Körper-**−x**-Kraft auf dem
geladenen Modell an. Feinheit: `FGExternalReactions::Load` HÄNGT Kräfte an und bindet danach seine sechs
Aggregat-Ausgabeproperties neu — die das geladene Modell bereits gebunden hat, falls es selbst eine
externe Kraft deklarierte, und ein doppelter Tie loggt pro Property einen Fehler. Deshalb werden die
Aggregate (`moments/[lmn]-external-lbsft`, `forces/fb[xyz]-external-lbs`) vorher **untied** und von
`Load` an dasselbe Objekt neu gebunden: keine Ausgabe, keine Modelldatei angefasst, und das Flugzeug
behält jede Kraft, die es selbst deklariert hat.

**Kosten pro Schritt:** genau ein `SetPropertyValue` je aktivem Kanal
(`magnitude = CdA · aero/qbar-psf`), und nur auf einer Zelle, die wirklich etwas trägt.
`SetStoresDrag` ruft man je **Loadout-ÄNDERUNG**, nicht pro Frame. Bei `cdaFt2 <= 0` (Default) wird die
Kraft **nie angelegt** — eine saubere Zelle ist bit-identisch zu einer, die nie von Stores gehört hat.

---

### 10. Schadenskanäle

Drei Konsequenzen, die eine aufgelöste Detonation auf eine Zelle haben kann (`core/FBDamageModel` nennt
die Werte und ihre Begründung), jede über einen Mechanismus, den JSBSim bereits hat. Gesetzt werden sie
**nur** vom Besitzer der Einheit (`units/FBSimUnit::ApplyDamageToAirframe`), nie von einem Modul: ein
Modul kommt an eine nicht-const `FBFdm` nur, wenn der Besitzer sie ihm reicht — und der reicht sie für
Steuerung, nicht für Schaden.

| Methode | Physikalische Wirkung | Werte (aus `core/FBDamageModel.h`) |
|---|---|---|
| `SetControlAuthority(norm)` | Skaliert JEDEN kommandierten Ausschlag (roll/pitch/yaw) **in** `SetControls`. Die FCS kommandiert unverändert weiter, das Flugzeug antwortet nur nicht mehr — was ein durchtrennter Aktuatorstrang bedeutet. | degradiert **0,5** [SET, aber mit strukturellem Grund: die F-16 hat zwei unabhängige Hydrauliksysteme, also ist der Verlust eines davon die natürliche Bedeutung von „degradiert"]; ausgefallen **0,0** |
| `SetThrottleLimit(maxNorm)` | Deckelt den kommandierten Throttle — der Nachbrennerbereich ist schlicht nicht mehr erreichbar. Der Schub selbst bleibt JSBSims Triebwerksmodell. | degradiert **0,6** [DERIVED: dort liegt das AB-Gate in der `throttle-cmd-norm`-Konvention des F-16-Modells]; ausgefallen = Cutoff (JSBSims eigenes Engine-Out, kein hier erfundener Schubterm) |
| `SetDamageDrag(cdaFt2)` | Zusätzliche Widerstands-FLÄCHE entlang Körper-−x **durch den CG** — also Widerstand ohne behauptetes Nickmoment (wo die Löcher sind, weiß dieses Modell nicht). Eigener Kraftkanal `fb-damage`, damit Carriage und Schaden sich nie überschreiben. | degradiert **1,5 ft²**, ausgefallen **6,0 ft²** [SET; zur Einordnung: die Nullauftriebs-Widerstandsfläche einer sauberen F-16 liegt in der Größenordnung 4 ft², also ist „degradiert" spürbar schmutzig und „ausgefallen" ein Loch in der Zelle] |

**Alle drei sind neutral, bis etwas getroffen wurde** — Authority 1, kein Throttle-Limit, kein Drag; der
`fb-damage`-Kanal wird bei `cdaFt2 <= 0` nie angelegt. Eine unbeschädigte Zelle rechnet **bit-identisch**
wie eine, die nie von Schaden gehört hat (nachgemessen, CLAUDE.md).

Was daraus FOLGT — ein Jet, der nicht mehr rollt; ein Triebwerk ohne Nachbrenner; eine Zelle, die die
Höhe nicht hält — ist JSBSim, das das Flugzeug integriert, das sie jetzt ist. Kein zweites, paralleles
Flugmodell.

---

### 11. Tank-Verdrahtung

Generisch über `FGPropulsion`s eigenes Tankinventar — enumeriert nach Index, nimmt nie an, wie viele
Tanks eine `aircraft.xml` deklariert.

| Methode | Verhalten |
|---|---|
| `SetFuelTankLbs(idx, lbs)` | ein Tank; Index außerhalb = No-Op; negativ → 0 |
| `SetFuelTotalLbs(lbs)` | verteilt **proportional zum Kapazitätsanteil JEDES Tanks** (so füllt eine reale Betankungszahl den Jet), nicht Tank-für-Tank |
| `SetFuelPct(pct)` | 0..100 der deklarierten Gesamtkapazität, über `SetFuelTotalLbs` |
| `GetFuelTankCount/TankLbs/TotalLbs/CapacityLbs` | Readbacks; `GetFuelTotalLbs` ist die `fuelLbs`-Telemetriespalte |

**Spritmangel simuliert diese Klasse NICHT.** Leerlaufen lässt JSBSims eigenes `FGEngine`-Modell das
Triebwerk NATIV verhungern; der Adapter macht den Füllstand nur beobachtbar und setzbar. Missionsseitig
kommen die Werte aus `set fuel_lbs` / `set fuel_pct` über `FBModule::ApplySetup`.

---

### 12. Readbacks

Alle const — deshalb ist `const FBFdm&` ein echtes Nur-Lese-Handle.

| Methode | Quelle | Wofür |
|---|---|---|
| `GetQbarPsf()` | `aero/qbar-psf` | die Größe, gegen die beide Drag-Kanäle gemessen werden |
| `GetCgXIn()` | `inertia/cg-x-in` | Carriage-Wirkung auf die BALANCE, beobachtbar; zugleich Angriffspunkt des `fb-damage`-Kanals |
| `SetGroundElevM` / `GetGroundElevM` | `position/terrain-elevation-asl-ft` | Welt-Wahrheit vom Elevation-Hook (`FBElevationProvider`) statt Flat-Default. Der Getter existiert, damit ein Aufrufer BEWEISEN kann, dass der DEM-Wert wirklich in JSBSim ankam — nicht nur, dass der Setter gerufen wurde. |
| `GetGroundClearanceM(bool gearDown)` | `FGGroundReactions` → je Gear `GetBodyLocation(3)` (Körper-z, ft unter dem CG), Maximum | CG-Höhe über Grund, wenn der tiefste aktive Kontakt aufsetzt. `gearDown=false` überspringt alle einziehbaren Kontakte → nur feste Struktur (Bauch). **Pro Modell und Fahrwerkszustand**, also sind Start/Aufsetzen/Crash-Erkennung und die Kamera-Augenhöhe geometriewahr statt eine feste Zahl. |
| `GetGearPos()` / `GetSpeedbrakePos()` | `gear/gear-pos-norm`, `fcs/speedbrake-pos-norm` | kinematisch verzögerte IST-Position |
| `GetWow()` | `gear/wow` (`FGGroundReactions::GetWOW`) | modellweit: wahr, sobald IRGENDEIN Bogey einfedert. Eine Aufschlüsselung je Bein gehört einem künftigen Fahrwerkssystem, nicht dieser Naht. |
| `GetStructureContact()` | enumeriert Kontakt-TYP `ctSTRUCTURE`, nie einen Index oder Flugzeugnamen | wahr, wenn ein nicht-berädeter Zellengeometriepunkt einfedert. Ein Modell ohne solche Punkte liest immer false. Konsument: `FBFlightMonitor`s Struktur-K.O. |
| `GetMaxGearForceLbs()` | max. `FGLGear::GetCompForce` über alle BOGEY-Beine | die modelleigene Feder/Dämpfer-Reaktion, KEINE abgeleitete Sinkraten-Heuristik. `FBFlightMonitor` vergleicht sie gegen `GetWeightLbs()` und urteilt „harte Landung" allein aus Physik. |
| `GetWeightLbs()` | `inertia/weight-lbs` | u. a. weil `FBPilot`s Rotationsgeschwindigkeits-Tabelle nach Gewicht indiziert |
| `GetEngineRunning(i)` | `propulsion/engine[i]/set-running` | je Triebwerksindex |
| `Faulted()` | gelatchtes Flag | s. §7 |

---

### 13. `FBFdmTelemetrySource`

Sitzt an der **Adapter-Naht**, nicht im Modul: `fb_fdm_state` ist das POD des FDM, und die einzigen
Zusatzeingaben sind die Zelle selbst und eine geborgte Boden-ASL — kein Modulzustand ist beteiligt. Alle
drei Referenzen sind geborgt und konstruktor-injiziert; `const FBFdm*` ist ein Nur-Lese-Handle:
Telemetrie beobachtet, sie kommandiert nie.

**Die Zelle ist OPTIONAL, und der Zeiger sagt das** — eine Einheit ohne Airframe (statisches Bodenziel)
hat trotzdem Pose, Höhe und Bodenprobe, also den größten Teil dieses Schemas.

| Spalte | Einheit | Quelle |
|---|---|---|
| `lat`, `lon` | deg | `st.lat/lon` |
| `altM` | m | `st.elev` |
| `aglM` | m | `st.elev − groundAslM` |
| `vsMs` | m/s | `st.vy` (X-Plane-local +y = hoch) |
| `pitchDeg`, `rollDeg`, `hdgDeg` | deg | `st.pitch/roll/yaw` |
| `fuelLbs` | lb | `GetFuelTotalLbs()`, **0 ohne Zelle** |
| `gearLoadFactor` | — | `GetMaxGearForceLbs() / GetWeightLbs()`, **0 ohne Zelle**. Es ist genau `FBFlightMonitor`s eigenes Hartlandungs-Verhältnis (`kHardLandingForceFactor = 3.0` löst aus), aber **jeden Tick** geloggt statt nur beim Auslösen — damit die Aufsetzhärte einer Landung messbar ist, auch wenn sie klar unter der K.O.-Schwelle bleibt. |

Nur die zwei genuin zellen-eigenen Spalten gehen auf 0; das Spaltenset bleibt identisch, also hat jede
Trace eines Laufs denselben Header, gleich welche Art Einheit sie erzeugt hat.

---

### 14. Namensraum, `extern "C"`, Build-Shim

**`namespace FlightBox` wie der Rest des Baums — kein `extern "C"`.** Die C-Linkage war für die längst
gelöschte `xp_bridge.c` der Vor-Pivot-Architektur da. Heute ruft niemand den Adapter aus C oder aus JS:
die einzigen WASM-Exporte sind `fb_toggle_ground`/`fb_set_ground` in `app/FBAppWasm.cpp`. `extern "C"` +
`EMSCRIPTEN_KEEPALIVE` bleibt Konvention **ausschließlich** für von JS NAMENTLICH gerufene Symbole
(EMSCRIPTEN_KEEPALIVE allein reicht nicht — C++-Mangling bricht Exporte still).

**`em_compat.h`** ist Klebstoff an der Naht, kein Patch: force-included (`emcc -include`) NUR für die
JSBSim-Quellen, damit das Submodul bit-vanilla bleibt. Inhalt: emscripten definiert `_GNU_SOURCE`
(libc++ braucht es für `strtof_l`/`strtod_l`), musl liefert aber das POSIX-`int strerror_r(...)`, während
`simgear/misc/strutils.cxx` den `_GNU_SOURCE`-Zweig nimmt und `std::string(strerror_r(...))` schreibt,
also den GNU-`char*`-Rückgabewert erwartet. Der Wrapper liefert den Puffer zurück; er wird VOR dem Makro
definiert, damit sein eigener Aufruf die echte libc-Funktion erreicht (keine Rekursion).
