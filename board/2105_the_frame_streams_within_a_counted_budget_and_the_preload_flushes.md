Type: bug
State: active
Area: engine, world
Tags: measured, performance, determinism, owner
Supersedes: 2109

# The frame streams within a COUNTED budget, and every timed frame is digested

**Benchmark** -- RAGE: `CStreaming::Update()` services a BOUNDED number of requests per frame;
`LoadAllRequestedObjects()` is the separate blocking form used on entering a scene. Unreal:
level streaming is asynchronous with a per-frame budget, and `FlushLevelStreaming(Full)` is the
blocking form the automation harness calls before every screenshot. **Both agree there are TWO
modes and they are different functions.** They differ in the frame bound -- Unreal in
milliseconds, RAGE in requests. **Taken: RAGE.** A time budget reads a clock, and this tree's
determinism is compulsory.

## Where it stands, measured 2026-09-04

| | | |
|---|---|---|
| preload refuses loudly | DONE | `Engine.cpp:265-283` on `Overflowing()` and on patience |
| an unpreloaded place is not measured | DONE | `PlaceCamera.cpp:305-311` |
| ingest waits for the whole ring | DONE | `GroundStack.cpp:113` returns while any tile is pending |
| the frame's budget is counted | NO | `GroundStack::Restand` loops `kVectorTiles` (49) passes, one tile per field per pass, from the frame path (`Advancing.cpp:196`) |
| the ceiling refuses | NO | `GroundStack.cpp:115-122` `break`s silently and sets `Overflowing_`; only preload reads it |
| the ceiling can see | NO | `HeapBytes()` misses the frame copies (board:2104) |
| Shibuya | refuses | 856.7 MB against 512 MB with the ceiling lifted -- board:2122 |
| eight places, three runs | steady | one still digest each; the 120 timed frames are unhashed |

The wall clock is gone and the medium-stage `memcmp` is gone (board:2092 holds the record). What
remains of the wander is the ORDER tiles are meshed in: `TileWatermark::Ask` sorts the candidate
set by `(distance², zoom, x, y)`, and `Restand` waits for the ring, so the sinks consume a
declared order today; `BuildingField.cpp:363` still reads `field.Tiles()` directly and is the one
site left to check.

## The solution

`Restand(at, TileBudget)`: the frame passes a declared count (one tile), the preload passes the
ring. The ceiling REFUSES -- returns `unexpected` with the bytes and the bound -- instead of
breaking out of a loop nobody told. With board:2122 the frame's form only PLACES pieces a worker
finished, so its budget is a count of placements and the mesh cost is not in the frame at all.

And the instrument closes the gap board:2109 named: `make shots` writes one digest over the 120
timed frames beside the still's, so a nondeterministic `advance()` on a settled world is visible
on the day, and the walk digest is ready for the day the camera moves again (board:2092).

## What will be true

- [ ] `Restand` takes the budget it may spend, counted in tiles; the frame passes one
- [ ] The 512 MB ceiling refuses with its reason and reads the tagged heap, frame copies included
- [ ] Every timed frame is digested; `shots --all` three times agrees on the still AND the walk
      digest for all nine places, Shibuya included
- [ ] Negative control: set the counted budget so low the walk never catches up, and the walk
      digest moves

## Ruled out, measured

- the candidate set (`PendingTiles() > 0` early return) -- in, right by the invariant, and
  CentralPark still drew two pictures six runs later; the measures could not see the
  difference (five heap numbers of 340), so the next step was a measurement, not a repair
- three runs are not enough to call a place deterministic: Kaiserberg drew a second digest on
  the twenty-eighth run

## Abbruch laufender Downloads
FetchInto prüft Stopping_ synchronisiert vor jedem Collect. Shutdown beendet
Polling und nutzt SourceSet::Abandon zum Freigeben des offenen Tickets.
Nachweis: ShutdownCancelsPendingFetch mit unabhängiger Pending-Quelle prüft
Abbruch unter 1 s statt 3000 Polls mit je mindestens 1 ms sowie genau einen Cancel.
Negativkontrolle ohne Stop-Prüfung: Zeit- und Pollgrenze scheitern; restauriert
bestehen alle drei TilePool-/nichtblockierenden GroundQuery-Tests.
Blockierende Fremd-Callbacks sind durch diesen Pollingvertrag nicht abgedeckt.

## Preload-Bereitschaft untersuchen
Engine::settled und PreloadTimeout nutzen jetzt dieselbe Readiness-Wertaufnahme.
Sie benennt Terrainanforderung, Download, Deckung, Nachbarn, Generator-Snapshot,
Ingestion, Klassifikation/Version, Vektoren und aktivierte Vegetation. Der Timeout
führt keine zusätzliche Grounds-Arbeit aus; schon laufende Arbeit kann überziehen.
Nachweis: jeder einzelne Blocker und mehrere gleichzeitig verhindern Bereitschaft;
Negativkontrolle mit ignorierten Blockern scheitert, restauriert grün. Bestehende
Deadline-API-Prüfung und Lattice bestehen unverändert. Kein visueller Pfad geändert.
Offen: deterministischer API-Nachweis des Timeout-Pfads mit kontrollierten Quellen;
Cache-/Streamingursache des früheren Lattice-Timeouts und harte Arbeitsbudgets.

## Szenenwechsel und Terrain-Lebensdauer
Live-Referenzen werden beim Clear gelöst und vor Grounds neu gebunden; EverLaid
wird beim Szenenwechsel invalidiert. BuildingField::ResetDerived verwirft nach
Jobabschluss Footprints, Bereiche und Verarbeitungsmarken gemeinsam, erhält die
Konfiguration. Drei Resetzyklen samt Negativkontrolle geprüft; Wien pixelidentisch.
Sonnen-A/B/A bleibt zunächst 37,022→37,018. Pauschal erzwungener finaler Neuaufbau
besteht dagegen die exakte Pixelprüfung; Air bleibt FAIL. Gegenprobe zurückgenommen.
Ursache: Focuses berücksichtigt keine neu fertig gewordenen Gebäudefundamente.
Entscheidung: BuildingField revisioniert Accept/Reset; Terrain merkt die verwendete
Revision. Abweichung erzwingt Neuaufbau und verhindert settled. Keine unbedingten
Neubauten im Framepfad. Test: Revisionen über Resetzyklen und exakte Sonnen-A/B/A;
alte Vergleichsbedingung als Negativkontrolle. Air separat untersuchen.
