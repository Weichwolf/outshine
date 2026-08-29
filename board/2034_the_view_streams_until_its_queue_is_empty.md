Type: defect
State: active
Parent: 1953
Area: generators, content
Tags: benchmark, measurement

# What the view wants is streamed until its queue is EMPTY, and a building reaches as far as it covers a PIXEL

**Benchmark** — Unreal: `BlockTillAllRequestsFinished` waits on the count of outstanding streaming requests and returns when it is zero; `FStreamableDelegate` exists beside it and is NOT what the frame asks. RAGE: the streaming thread posts `sysIpcSignalSema` on each completion and `LoadAllRequestedObjects` blocks on it until the request queue drains. **They agree**, so the matter is closed: readiness is an EMPTY QUEUE plus a completion signal, never a sample taken on a timer and never a subscription.

## What is wrong, measured

Three defects stack, and the third is the one a viewer sees.

**One — the pumps are coupled.** `GroundStack::Restand` reads

    if (Vectors_->Build(*Pool_, lat, lon, kVectorRing) <= 0) { return; }

and `OsmField::Build` returns the features **this call** decoded, one tile per call. So the moment the ring is fully decoded the return is 0 and the three fields — streets, water, footprints — are never fed again. Their watermarks stop wherever they stood. Measured on the Jura: `vector tiles that settled: 5` out of a 3x3 ring of 9, and the building field holding 159 footprints.

**Two — readiness is sampled, not signalled.** `Engine::preload` spins `sleep_for(20ms)` and asks `settled()`, which is

    World.AskedWanted > 0 && World.AskedPending == 0 && World.Grown

`Grown` is `Snapped::Taken` for the ONE region under the anchor. Nothing in that question asks whether the OSM fields have drained, and `TileWatermark::Done` — the exact question — already exists and is asked by nobody. So the picture is taken when the TERRAIN finishes, and how much OSM got in depends on how many 20 ms rounds happened to fit first. That is timing, so it is cache state: three warm runs back to back give 983 footprints and 31233 triangles identically, and a run half an hour earlier at the same position gave a different picture.

**Three — the reach is set, not derived.** `kVectorRing = 1` at zoom 14. A z14 tile is 40075017/2^14 = 2446 m at the equator, x cos 47.25 deg = **1660 m**, so ring 1 reaches about 2.5 km. Every other place stands INSIDE its city and never noticed. The Jura stands 4.9 km from Solothurn — camera tile (8533, 5746), Solothurn's tile (8535, 5748), outside the ring in both axes — so the town is never fetched and the frame shows the scattered farms of the plain and no town.

## What the reach has to be

Derived from three bounds, and the binding one is stated:

| bound | derivation | answer |
|---|---|---|
| horizon | d = sqrt(2Rh), camera 330 m over the plain | 64.8 km |
| **one pixel** | 1280 px / (2 tan(27.5 deg) d) = 1230/d; a 10 m house subtends 1 px | **12.3 km** |
| atmosphere | contrast falls first; the aerial perspective already computes it | before either |

A 30 m block reaches 37 km and a 100 m tower 123 km by the same test, so the reach is a function of HEIGHT and not one number — which is the DAG's screen-space error, already owned by 1992/1993 and not re-decided here. What this item owns is the STREAMING volume: what exists at all, out to where an ordinary house is still a pixel.

12.3 km at z14 is a ring of 8 -- 289 tiles -- and the decoder takes one per call. So defect one is not merely also present, it is the PRECONDITION.

## What will be true

- `Restand` decodes and ingests as two independent pumps: ingestion never gates on new decoding
- The pump rate is an argument, not a constant. The frame path passes 1 -- bounded terms on the frame path is an invariant -- and `preload`, which is the client's explicit blocking wait and not the frame path, drains what is ready
- `settled()` answers the EMPTY QUEUE: no vector tile pending AND every field's `TileWatermark::Done`. The count exists; the question is asked
- `preload` waits on the arrival signal `TilePool::Landed_` with its deadline, and pumps while there is work. No `sleep_for`. It blocks only when idle, which is what both benchmarks do
- The ring is derived from the pixel test rather than set, and its derivation stands where the number does

## Not a bus

A publish/subscribe bus is the wrong instrument for THIS question and the reason is structural rather than a preference. The edge has one producer and one waiter, so a bus buys a registry, a dispatch, a lifetime and an ordering question for a one-to-one edge. Worse, a subscriber callback fired from the IO thread RUNS on the IO thread inside the producer's critical section, which is a subsystem reaching into another's live state -- the defect the four-way separation exists to prevent. A counter plus a signal is a SNAPSHOT and does not. Unreal owns both mechanisms and uses the counter here, which is the evidence.

The scenario's declared `Event`s are a different thing and are already stood up by `TriggerField::Stand`; nothing here touches them.

## What would show this wrong

`vector tiles that settled` equal to the full ring, every field's watermark done, and Solothurn standing in `build/places/Jura.png` -- and if the town is still absent with the ring proven full, the cause is the mesher and not the stream.
