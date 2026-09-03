# The instrument reads the bus that was built for it

State: open

`src/base/io/Telemetry.h` carries a complete telemetry bus: `Register(TelemetrySource *)`,
`SetSink`, `Start`, `Tick(simTimeS)`, a schema and a row. NOTHING in this tree inherits from
`TelemetrySource`. NOTHING inherits from `TelemetrySink`. NOTHING calls `Tick`. Both ends are
unconnected, so thirteen measurements sit behind getters no caller reads --
`HeapProbe::PeakLiveBytes`, `StackProbe::Floor/Limit/PeakBytes`, `TilePool::SchedulerBytes`, and
seven `...Taken` counters the building and roof generators keep.

And beside it, `src/client/PlaceCamera.cpp` collects frame times into a `std::vector<double>` by
hand, sorts it, and reads quantiles off it. That is what the bus is FOR. The tree has two
mechanisms for one job and the better one is the one nobody wired up.

**Benchmark**: Unreal declares a counter with `DECLARE_CYCLE_STAT` and it appears in `stat` groups
without anyone writing a getter -- the counter REGISTERS, it is not fetched. RAGE does the same
through its telemetry channels beside `bkBank`. They agree completely: a measurement that needs its
reader to know its name is a measurement that goes unread, which is exactly what happened here.
Taken: theirs. A probe registers with the bus; the bus owns the schema; a sink writes the rows.

This is goal 2's instrument as much as it is tidying. `0 of 120 frames over 16.7 ms` is a time
SERIES over a run, one row per frame -- which is the shape the bus already has and the shape a
hand-rolled vector in the client does not.

**Closes when** the client's frame walk is a `TelemetrySource`, `make shots` writes its rows
through a sink, and the hand-rolled collection in `PlaceCamera.cpp` is gone. The thirteen getters
then have a reader or they are deleted with their probes -- that judgement belongs to this item and
not to a walk over symbols.

**The measurement that shows I was wrong:** if the bus's per-tick row costs measurable time on the
frame path, it is the wrong shape for a frame and the client's vector was right. Compare p99 with
the bus wired and unwired over the same place.

Beside it, deleted in the same round: `Physics::Shed(Shearing, Rolling)`. It computes a slip angle
from two velocity components and hands it to `ShedAt` -- but `Rig.cpp` needs `Relaxed()` between
those two steps, so it computes the same `atan2(-across, rolling)` itself and calls `ShedAt`
directly. The overload could never be called by the one caller that exists.
