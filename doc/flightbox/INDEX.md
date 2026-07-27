# FlightBox — Wissensbasis

Was FlightBox ist und wie es gebaut ist. Diese Sammlung ist die **Autorität**; `CLAUDE.md` im
Wurzelverzeichnis ist nur ein Session-Start-Zettel und verweist hierher. Widersprechen sich beide, gilt
das hier und `CLAUDE.md` ist nachzuführen.

Geladen wird das über den Skill **`flightbox`** (`.claude/skills/flightbox/SKILL.md`), aufgabenbezogen.

Stand: `793e1fe` + Modellwurzel/Delta-Regel, 27.07.2026.

## Immer zuerst

| Datei | Inhalt |
|---|---|
| [conventions.md](conventions.md) | Namen, Struktur, die Keine-printf-Regel, und die Regel, dass **jede Zahl ihre Herkunft trägt** — hergeleitet, gemessen oder `[SET]`. |
| [architecture.md](architecture.md) | Prozessmodell, Core-Lib plus drei Clients, Verzeichniskarte, das Schichtungsmuster, Multi-Unit in Kürze. |

## Subsysteme

| Datei | Zeilen | Inhalt |
|---|---|---|
| [core.md](core.md) | 2177 | Der Avionik-Blockbus mit Dreizustands-Gültigkeit, der Kommandobus mit Quittung und Ablehnungskatalog, `FBLog`/`FBTelemetry`, die **zwei Richter**, Missionsdaten-Typen, Kampfziele, Gesundheitsregister und Schadensmodell, Ballistik- und Waffen-Wertetypen, Elevation-Hook, Geodäsie. |
| [fdm.md](fdm.md) | 445 | Der JSBSim-Adapter: die Ein-TU-Naht, Instanzfähigkeit, **IC-Abschottung**, Ownership, Zuladung und Schadenskanäle über modell-eigene APIs, der vollständige Ladeablauf. |
| [units-and-missions.md](units-and-missions.md) | 736 | `FBUnit`/`FBSimUnit`/`FBUnitRegistry`, die Snapshot-Barriere, der Vier-Schritt-Orchestrator, Spawn, die Multi-Unit-Etappen 1–4 samt Thread-Pool und den ehrlichen Skalierungszahlen, Detonations- und Aufschlag-Auflösung. |
| [systems.md](systems.md) | 879 | Die generischen Slots: Guidance (inkl. **vollständiger Bahnfolge-Herleitung**), FBW-Innenschleife, Luftdaten, Radarhöhenmesser als Referenzfall für `Invalid`, Warnungen, Navigation und Wegpunkt-Sequenzierung, Display-Slot, Airframe-Controls. |
| [sensors.md](sensors.md) | 804 | Datalink, Radar, RWR, Gegenmaßnahmen — und **die Wahrnehmungsgrenze**: wer die Registry sehen darf, warum ein Kontakt anonym ist, warum IFF zweiwertig ist. |
| [pilot-ai.md](pilot-ai.md) | 1148 | Phasenmaschine, Attack, BFM mit seinem eigenen Regelgesetz, das **Datum** als Piloten-Gedächtnis, BVR-Abfang, Debriefing-Kanäle, Varianten und Turnier, der Missions-Regelkreis. |
| [weapons-and-damage.md](weapons-and-damage.md) | 1525 | Waffe-als-Einheit, SMS und Kanone, geteilte Ballistik, die **drei Auflösungsgrenzen**, das Schadensmodell von der Geometrie bis zur Systemfolge, und die Kopplung „Ausfall → Block ungültig". |
| [modules-f16.md](modules-f16.md) | 1003 | Das F-16-Modul: Komposition, Takt, Kommando-Router, und jeder Override mit seinen Zahlen und deren Herkunft. HUD-Symbologie-Umsetzung. |
| [rendering.md](rendering.md) | 663 | WebGPU, ECEF camera-relative, Reversed-Z, die **Pass-Topologie als Vertrag**, der Stage-Katalog, das HUD-Backend mit Coverage-AA, Kamera und Bodenwahrheit. |
| [world-and-terrain.md](world-and-terrain.md) | 501 | `FBWorld`, Tile-Streaming und Worker, Elevation über Kacheln, die Terrain-Lib, `fb-tiles` aus Klientensicht. |

## Betrieb und Stand

| Datei | Inhalt |
|---|---|
| [build-and-ops.md](build-and-ops.md) | Make-Targets, die **Gates**, Mess-Disziplin, der Missions-Regelkreis, Host-Eigenheiten. |
| [PROGRESS.md](PROGRESS.md) | Reifegrad je Bereich, Chronologie der Bauabschnitte, und die **gefundenen Defektklassen** als Prüfmuster. |
| [TODO.md](TODO.md) | Offene Arbeit nach Wert, bewusste Lücken, **verworfene Ansätze mit ihren Messungen**. |

## Verwandte Sammlungen

| Ort | Gegenstand | Verhältnis |
|---|---|---|
| `doc/f16/` (Skill `f16-systems`) | der **echte** F-16C aus den Handbüchern | Design-Ziele, keine Defektkriterien. Wo eine FlightBox-Zahl aus einem Handbuch stammt, zitiert die hiesige Datei die dortige — sie wiederholt sie nicht. |
| [`doc/mission-format.md`](../mission-format.md) | das `.fbm`-Format | Referenz; `units-and-missions.md` verweist darauf, statt es zu duplizieren. |
| `doc/clouds/` | Wolken-Renderingstudien | Vorarbeit, teils noch nicht verdrahtet. |

## Pflege

Diese Sammlung wird **mitgeführt, nicht nachgeholt.** Jede Runde, die Verhalten ändert:

1. aktualisiert die betroffene Subsystem-Datei — **einschließlich der Herleitungen**, denn eine Zahl
   ohne ihre Herleitung ist hier ein Defekt;
2. trägt sich in [PROGRESS.md](PROGRESS.md) ein (Commit, was gebaut, was gemessen);
3. pflegt [TODO.md](TODO.md) — was geschlossen wurde raus, was geöffnet wurde rein, **verworfene
   Ansätze mit ihren Messungen bleiben stehen**;
4. fasst `CLAUDE.md` nur an, wenn sich eine Session-Start-Tatsache geändert hat.

Jede Subsystem-Datei endet mit `## Offene Punkte`. Dort stehen bekannte Lücken und Widersprüche
zwischen Kommentar und Code. Keiner davon wird stillschweigend aufgelöst: entweder behoben und
vermerkt, oder stehen gelassen.
