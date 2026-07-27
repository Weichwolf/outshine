# Offene Arbeit

Nach Wert geordnet. Jeder Eintrag nennt, wo er sitzt und woher er bekannt ist. Was hier steht, ist
entweder gemessen, beim Destillieren der Doku gefunden, oder als bewusste Lücke deklariert.

**Pflege:** Was eine Runde schließt, wird hier gestrichen und in [PROGRESS.md](PROGRESS.md) eingetragen.
Was sie öffnet, kommt hierher — **einschließlich verworfener Ansätze mit ihren Messungen.** Ein
gemessener Fehlschlag ist Wissen; ihn zu löschen heißt, dass ihn jemand nachbaut.

Stand: `0f52690` + laufende Runden (s. Roadmap).

## 0. Roadmap

Die Richtung, vom Projekteigner gesetzt: **ein taktisches Luftkampfspiel auf echter Physik** — so
korrekt wie nötig, so zugänglich wie möglich. Der Maßstab ist gestaffelt: die F-16 genau (sie ist das
Produkt), Sensoren/Waffenhüllkurven/Gefechtsverlauf glaubwürdig (hier entsteht das Spiel), gegnerische
Flugzeuge treffen ihre Einhüllende und mehr nicht. Kurvenkampf-Fidelity ist nachrangig — meist sieht
man seinen Gegner nicht; die Waffensysteme sind wichtiger als der Flieger, der sie trägt. Wetter,
Wolken und Nacht sind taktische Schichten, nicht Dekoration. Anti-Cheat bleibt aus Spielgründen: einen
schummelnden Gegner merkt man sofort.

Zwei Missionsklassen: die `sim/missions/*.fbm` sind **Messvorrichtungen fürs Gym** und bleiben so
(der Regelkreis arbeitet damit); Missionen **für Menschen** werden später lockerer definiert —
Szenarien statt Testaufbauten.

| # | Etappe | Stand |
|---|---|---|
| R1 | **Kommentar-Kürzung** `sim/src/` (Herleitungen sind in dieser Doku, Code trägt Einzeiler + Verweis; Beweis: `sim/tools/strip_comments.py`-Hash unverändert) | läuft, 4 Agenten; `core/` fertig (−66 %) |
| R2 | **Wetter, Server-Hälfte:** `/wx` auf fb-tiles — NOAA GFS 0,25°, globales Kompaktraster statt Kacheln (1440×721 je Variable), Wind 10 m + 850/700/500/250 hPa, Bedeckung je Stockwerk, Wolkenbasis, Sicht. Format = Schnittstelle, dokumentiert in `world-and-terrain.md` | läuft |
| R3 | **MiG-29-Wissensbasis** `doc/mig29/` nach dem Muster von `doc/f16/` + dessen neuem `flight-model.md` (§11-Checkliste als Template): Systeme/Avionik/Prozeduren aus den zwei DCS-Handbüchern + Web-Recherche, `weapons.md` (R-27R/T, R-73, R-60M, GSch-301, Luft-Boden), `flight-model-spec.md` als Bauvorlage mit belegten Einhüllenden-Ankern | läuft, 2 Agenten |
| R4 | **Wetter, Sim-Hälfte:** `FBWeatherProvider` (konstant / **fixes Gym-Dataset** aus der /wx-Fixture, deterministisch / live), Wind→JSBSims FGWinds (heute null Verdrahtung), `.fbm`-Wetterdeklaration, Datenschnittstelle für den Wolken-Neubau | nach R1 |
| R5 | **Wolken-Neubau** — Spezifikation s. 4.9. Die sechs `FBCloud*`-Stages sind Abriss, nicht Basis | nach R4 |
| R6 | **Asymmetrische Waffen + RCS:** gegnerische Flugkörperfamilie (R-73/R-27/R-77-Klasse aus der MiG-29-Basis; R-37M-Klasse für die Fernbedrohung), AIM-9X, Rückstrahlquerschnitt als Einheiten-Eigenschaft. Begründung: das Duell-Patt (2.3) ist Symmetrie, kein KI-Defekt — Asymmetrie macht aus dem Münzwurf eine Entscheidung | nach R3 |
| R7 | **Gegner-Einheiten** nach der Rangliste: Einwegdrohne (Shahed-Klasse) + Marschflugkörper zuerst (einzige richtig baubaren; reale F-16-Missionen; stressen Doppler-Notch und Kanone; `modules/drone` testet die Modul-Architektur), dann MiG-29 nach R3-Spec bzw. Flanker-Klasse — ausdrücklich BVR-Maßstab | nach R6 |
| R8 | **JSBSim-MiG-29-Modell** nach `doc/mig29/flight-model-spec.md`s Bau-Reihenfolge, jeder Schritt gegen einen belegten Anker im Gym gemessen; Buchführung über `MODEL-DELTAS`-Disziplin | nach R3 |
| R9 | Menschen-Missionen: Szenario-Schicht über dem `.fbm`-Format | offen |

## 1. Widersprüche zwischen Behauptung und Code

Billig zu beheben, hoher Wert: hier lügt der Baum über sich selbst. Alle beim Destillieren gefunden,
keiner davon gesucht.

| Ort | Widerspruch |
|---|---|
| `render/FBCamera` + `sim/up.sh` | **Der Kamera-Clamp „nie unter die Oberfläche" hat keinen Konsumenten mehr.** `FB_GROUND_CLEAR` wird von `fb-sim` erzeugt und von niemandem gelesen. Eine in CLAUDE.md zugesagte Invariante ist faktisch abgeschaltet. |
| `core/FBRadarContact.h` | Banner behauptet, Tracknummern würden nach einem Drop wiederverwendet. `FBRadarSystem::NextTrackNum_` zählt monoton und tut das nie. Konsumenten verlassen sich heute **undokumentiert** auf Eindeutigkeit. |
| `modules/f16/FBF16Rwr.h` | `kOpenThreats = 16` gegen `kMaxRwrThreats = 8` — der OPEN-Deckel kann nie greifen, PRIORITY ist der einzige wirksame. |
| `systems/FBRwrSystem` | `SelfTeam_` wird gespeichert und absichtlich nie gelesen. Toter Zustand mit Cheat-Potenzial, sobald ihn jemand anfasst. |
| `systems/FBFlightControl::Run` | `0.01` hart verdrahtet statt `dt` — bindet die Innenschleife still an 100 Hz. |
| `systems/FBDisplaySystem` | inkludiert `render/FBCamera.h`. Das ist eine **zweite** Core-Lib-Ausnahme; dokumentiert war nur `FBHudGeometry.cpp`. |
| `fdm/FBFdm` | Zählwiderspruch beim prozessweiten JSBSim-Zustand: CLAUDE.md sagt drei, `FBFdm.cpp` listet vier. Der Code ist maßgeblich (die Zwei-Behauptung im Header ist mit der Kommentar-Runde entfallen). |
| `modules/f16/FBF16Sms` | Stationsgeometrie ist **längs kollabiert** — alle neun Pylone auf derselben Rumpfstation, also erzeugt Zuladung kein Nickmoment. |
| `systems/FBWeaponSystem` | vestigial: NoOp-Stub wird mit 20 Hz getaktet, obwohl Stores und Gun real sind. Sein Banner beschreibt einen überholten Zustand. |
| `FBMissionRunner.h` | Docstring nennt `LOC` nicht und spricht von Einzahl-Modul; der Detonations-Banner behauptet „what a hit DOES is deliberately not modelled yet" direkt über dem `ResolveBurst`-Aufruf. |
| `core/FBFlightMonitor.h` | Banner verortet das Off-Runway-Urteil in `FBMissionRunner.cpp`. Es lebt seit dem Missions-Monitor in `core/FBMissionMonitor::Tick`. |
| `core/FBStateBusTelemetry.cpp` | Banner zählt „zwei danach hinzugekommene Blöcke (Rwr, Cmds)" — es sind drei, `blk_gun` folgt derselben Regel. |
| `core/FBDamageModel` | `kMaxZones = 5` ist an `FBDamageZone` gekoppelt, aber **nicht compilergeprüft**. Eine neue Zone verschwindet still in der Bereichsprüfung von `AddKinetic`. |
| `core/FBAvionicsBlocks.h` | `FBStoresBlock::Arm` defaultet auf `Arm`, `FBGunBlock::Arm` auf `Sim`. Asymmetrie ohne Quelle — und die scharfe Seite ist die weniger konservative. |
| `core/FBFlightPlan` | `FBWaypointType` deklariert vier Typen, der Parser erzeugt zwei. |
| `math/FBMat4.h` | Bricht die Coding-Konvention des Baums (Vor-Pivot-Erbe). |

## 2. Piloten-KI

| # | Sache | Bekannt aus |
|---|---|---|
| 2.1 | **Ankunfts-Annäherung noch ~85 kt am Bandrand.** Der Gashebel regelt eine Geschwindigkeits*differenz*, der Fahrplan ist in Entfernungs*rate* geschrieben; beide laufen auseinander, sobald der Verfolger Höhe tauscht (74 kt TAS-Differenz gegen 157 kt Annäherung). **Zwei Kandidaten gemessen und verworfen** (s.u.). Nächster: Lag-Winkel nur innerhalb des Bandes, wo die Schätzung konvergiert ist. | `658014d` |
| 2.2 | **Rollraten-Regler konvergiert nicht**, wenn das rohe Kommando in der Amplitude schwingt — `cmd_prev·cap/rate` hat dann keinen Fixpunkt. Departure bei Punkt-blank gesehen. | `658014d` |
| 2.3 | **Das Duell bleibt ein Patt** — jeder Fernschuss wird im Notch abgewehrt, nichts kam je in den Zünderradius. Entschiedene Ausgänge gibt es nur, wo die Startbereiche differieren. | `cac7b62` |
| 2.4 | Kanone verfehlt noch ~1 von 8 Anflügen gegen den kurvenden Verteidiger. | `658014d` |
| 2.5 | AoA-Spanne 11–13° statt flacher 11° im Anflug (ED-belegt, `doc/f16/procedures-landing.md`); Porpoise nach dem Aufsetzen; `ApproachSpeed` gewichtsgeplant statt fest. | Messung |

### Verworfene Ansätze (nicht erneut probieren ohne neues Argument)

| Ansatz | Warum verworfen |
|---|---|
| Geometrischer Lag-**Winkel** aus der Entfernungsraten-Gleichung, global wirkend | Die Modus-Wahl wird zum Relais, der Auftriebsvektor flattert; mit Lag in der Vertikalen zoomt der Jet 940 m und kommt als Split-S zurück. Beste Variante: 0 von 8 Abschüssen. |
| Gashebel regelt die gemessene Entfernungsrate statt der Geschwindigkeitsdifferenz | Band 21,4 → 23,2 %, aber Trichterzeit gegen den geraden Verteidiger 21,2 → 12,7 s. |
| Konversions-Spiegelung auf den langen Weg herum | Unbegrenzte Rolle, Departure bei t=39 in `bfm-blind`, 2 von 16 Anflügen. |
| Wingline-Commit ohne LOS-Raten-Tor | Kostet 2 von 11 Abschüssen. |
| Gun-Integrator mit ζ = 0,5 | Gegen den Kurvenden besser, gegen den Geraden klingelt die Schleife und die Trichterzeit bricht ein. |

## 3. Nicht modellierte Physik

Bewusste Lücken. Jede ist im jeweiligen Header als solche benannt — keine ist ein Versehen.

| Sache | Folge | Datei |
|---|---|---|
| **Terrain-Maskierung** für Radar, Datalink und Funkpfad | Luft-Luft-Sichtlinie ist immer frei. Bräuchte einen DEM-Raymarch je Kontakt je Look. | [sensors.md](sensors.md) |
| **Kein IR-Sucher** | Fackeln werden gezählt und wirken nicht. | [sensors.md](sensors.md) |
| **Kein Lofting der AIM-120** | Mittelphase fliegt flach, Reichweite bleibt unter dem Möglichen. | [weapons-and-damage.md](weapons-and-damage.md) |
| **Das Mk-82-Modell trägt keine belegte Aerodynamik** — sein eigener `<note>` nennt sich möglicherweise eine grobe Näherung, deren einzige Ähnlichkeit mit dem echten Objekt der Name sei | Die CCIP/CCRP-Genauigkeit (22 m gesamt, 10,6 m quer) ist damit eine Aussage über die Treue zum MODELL, nicht über einen echten Abwurf. Die Fehlerbudget-Aufteilung bleibt gültig — sie misst unsere Guidance gegen unsere eigene Ballistiktabelle —, die absolute Zahl darf nicht als Fidelity-Beleg zitiert werden. Ein Modell mit belegter Aerodynamik zu beschaffen oder zu bauen ist offen. | [weapons-and-damage.md](weapons-and-damage.md) |
| **Kein Strafing.** Ursache ist *nicht* die Nullfläche der Bodenziele, sondern dass `FBGunProjectiles` nach 3 s / 3000 m aufgibt — die Geschosse erreichen den Boden nie. | Luft-Boden mit der Kanone unmöglich | [weapons-and-damage.md](weapons-and-damage.md) |
| Bodenburst wird nicht gegen Flugzeuge aufgelöst | bewusst: die Splittergeometrie gegen eine Zelle gibt es nicht, ein erfundener Radius wäre eine als Physik verkleidete Zahl | [weapons-and-damage.md](weapons-and-damage.md) |
| Keine Splitter-Richtcharakteristik, kein Zünderversagen, kein Munitionsgewicht, kein Kanonen-Einbauwinkel | — | [weapons-and-damage.md](weapons-and-damage.md) |
| Kein ECM/Jammer, kein MWS, IFF nur Mode 4, keine Messfehler und kein Rauschen auf Sensordaten | — | [sensors.md](sensors.md) |
| Kein Verbandskonzept — `fl` ist schlicht die erste Unit | Datalink-Filter „nur Flight Leads" ist damit nicht echt | [sensors.md](sensors.md) |
| `GroundElevPatch` unimplementiert | keine Geländefolge, keine CFIT-Prognose | [world-and-terrain.md](world-and-terrain.md) |

## 4. Renderer und Welt

| # | Sache |
|---|---|
| 4.1 | **`FBUnitsStage`/`FBSpritesStage` sind NoOp** — Waffen und andere Einheiten sind unsichtbar, obwohl `FBWorld` die Registry längst borgt. Der auffälligste Unterschied zu dem, was die Simulation kann. |
| 4.2 | **`payerne-full` stürzt unter `--elev tiles` ab.** Drei Verdachtsflächen benannt: z13-Bilinear gegen 90-m-Raster, 33-m-Cachezelle, 503 beim Kaltstart. Solange das offen ist, hängt der Missions-Regelkreis faktisch an `const`/`swiss`. |
| 4.3 | Transmittance-LUT wird unnötig jeden Frame gerechnet. |
| 4.4 | Aerial Perspective per Default aus; Wolken aus und keine Wetterquelle verdrahtet. |
| 4.5 | Upscale nur bilinear; HUD-Glow fehlt. |
| 4.6 | Toter Code: `w3_frustum_from`, `w3_aabb_visible`. Statischer Terrain-Pfad ungetestetes Erbe. |
| 4.7 | DEM-Cache liegt pro Worker-Instanz (6× Redundanz, ungemessen). Eviction rein zeitbasiert; `kNodeCeil` verweigert stumm jeden Split. |
| 4.8 | Bildmodus (SVS/EVS) ist nicht in `.fbm` deklarierbar. TLS im Tile-Server nicht verdrahtet. |
| 4.9 | **Wolken-Neubau — die Spezifikation** (Roadmap R5, vom Projekteigner festgelegt): **begrenzt-volumetrisch, aber einfach.** EINE separable Dichtefunktion je Stockwerk — 2D-Coverage-FBM (windadvektiert aus dem Weather-Provider) × analytisches Vertikalprofil, optional ein kleines beim Start erzeugtes 3D-Erosionsrauschen (~64³). Marsch NUR im Schichtband: Strahl ∩ Kugelschale analytisch → maximal drei kurze Segmente, 6–12 Schritte je Segment, Blue-Noise-Jitter, 16F-Akkumulation, Dither am Ausgang (das Banding-Rezept). Licht ohne Sekundärmarsch: Beer über die Restdicke aus dem Profil geschlossen + 2–3 Sonnen-Taps ins 2D-Feld, Ambient aus der vorhandenen Sky-LUT. Fullres, EIN Pass, kein Temporal, keine Bakes, `t1` an die Szenentiefe geklemmt (Wolken vor Bergen, Nebel unterm Jet über Terrain gratis). Kamera im Band → Segment beginnt bei ihr: EIN Codepfad für außen/innen/Übergang, Durchflug nahtlos per Konstruktion. **Keine Impostoren** — Preis: aufgelockerte Cumulus lesen sich als fleckige Decke, akzeptiert. Die Dichtefunktion muss WGSL- UND C++-auswertbar sein (geteilte Konstanten): „wie weit sehe ich" ist dieselbe Frage, die später Sensoren/IR stellen. **Die sechs `FBCloud*`-Stages sind Abriss, nicht Basis** — Anforderungen des Eigners: wie viel und wie weit man sieht, Durchflug mit Rücksicht, der Nebel unter einem. |

## 5. Clients

| # | Sache |
|---|---|
| 5.1 | **WASM hat keinen Freigabe- und keinen Schadenspfad.** `FBAppWasm.cpp` leert weder die Release- noch die Burst-Queue und hält keinen Projektil-Pool — im Browser kann nichts abgeworfen und nichts getroffen werden. |
| 5.2 | Cockpit-Displays: die Werte liegen auf dem Bus, die Darstellung fehlt vollständig. |
| 5.3 | HOTAS-Binding — bewusst zuletzt, ist nur ein Mapping. `FBInputSystem` ist NoOp. |
| 5.4 | Keine Lock-/TD-Box-HUD-Symbologie, weil `doc/f16/hud-symbology.md` keine kennt. Wird nicht erfunden. |

## 6. Offene Entscheidungen

| Frage | Stand |
|---|---|
| **Der erste echte Modell-Delta.** Die Delta-Regel und ihr Gate stehen (`sim/assets/MODEL-DELTAS.md`, `make -C sim verify-models`), die Liste ist leer. Ungeprüft ist damit nur eines: ob das Eintrags-Format für einen MEHRDATEI-Delta oder eine neue Datei (Diff gegen /dev/null) im Alltag trägt. Der Verifikator kann beides, gemessen ist es nicht. | offen |
| **Flaperon-Mixer des f16-Modells: erster Kandidat für einen echten Delta.** `doc/f16/flight-model.md` §7.9 belegt vertauschte Vorzeichen im Flaperon-Summer (`left + right` statt `left − right`): die Landeklappen sind aerodynamisch wirkungslos, jedes Rollkommando erzeugt einen symmetrischen Auftriebssprung (gemessen: Nz +1,0 → −0,88 in 0,11 s bei 1,5° Bank) und **negativen Widerstand** (−10.000 lbf, physikalisch unmöglich), asymmetrisch je Rollrichtung — plausibel die Ursache der ~0,2°-Reiseflug-Asymmetrie und eines Teils der BFM-Rollprobleme. Die Korrektur wäre exakt der Fall „nachweisbarer Fehler im Modell", für den die Delta-Regel gebaut wurde — aber sie re-baselined ALLE 50 Missionen. **Entscheidung des Projekteigners, nicht einer Runde.** | **wartet auf Eigner** |

## 7. Nachführung dieser Doku

`pilot-ai.md` und `modules-f16.md` beschreiben den Stand `9673e00`. Commit `658014d` hat davon bereits
drei Punkte verändert (vorzeichenrichtiger Abzug, hergeleiteter Bremsdeckel `a/k` mit dem neuen Hook
`BfmBrakeMs2`, Wingline-Konversion). Betroffen sind `pilot-ai.md` §5.2/§5.7/§5.8/§11/§12 und
`modules-f16.md` §2.1/§2.2/§2.6/§3.3.
