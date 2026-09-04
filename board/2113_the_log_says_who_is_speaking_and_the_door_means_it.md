# The log says who is speaking, and the door means it

State: active

`LogSink::Write` takes the tag as `const char *`, and `LogField` takes its key the same way. Both
are BINS: every caller spells its own, so `ground` and `Ground` and `terrain` become three
subsystems that are one, and no query over the log can find them all. A client implements
`LogSink` through the DOOR, so this is a promise the door makes and does not keep.

Measured on 2026-09-04, and the bin turns out to be small:

  36 log calls in the whole tree
   4 tags        ground · render · veg · world
  18 events      already machine-readable: tile_refused, plan_alias, readback_map_failed, ...
   8 field keys  and `msg` is ELEVEN of them

## Three channels, and FREQUENCY decides which

The scope settles this: outshine is worldwide and will hold hundreds of thousands of NPCs. At one
percent having an event each second that is a thousand lines a second -- seventeen per frame, each
one formatting, allocating and writing. That is the stall `NoFramePathCallReachesABlock` refuses.

  LOG     rare events, read backwards by a HUMAN        once per run or per load     text
  STATS   aggregates the program and the gates read     once per frame               numbers
  TRACE   mass data, examined offline                   per entity per frame         binary ring

"A thousand NPCs lost their target" is a STAT, never a thousand log lines. One NPC losing its
target is nothing at all -- until somebody follows that one on purpose, which is what a trace is
for. **The frequency decides the channel, not the importance.**

Unreal: `UE_LOG` for events, `stat` groups for aggregates, Unreal Insights
(`TRACE_CPUPROFILER_EVENT_SCOPE`) for mass -- binary, no string in the frame. RAGE: logs for
faults, timebar and sampled telemetry channels for the rest. Neither ever puts mass data through
the log. Taken: theirs, and the third channel does not exist here yet.

## Done 2026-09-04

`LogTag` is an enum in the door with `nameOf`, ten files went with it, both sinks print it. The
four `Log::Debug` calls are gone -- every one was a COUNTER written as text, `vectile` fired once
per vector tile, and the fields are built before `Emit` filters the level, so each tile allocated
five `std::string` and threw them away with Debug switched off. What fell out with them proves
they were nothing else's: `kBytesPerKB`, `kBytesPerMB`, `kGroundGridBytes` and two locals in
`TilePool` had no other reader.

The level stays in the door -- a client may filter on it -- and this tree no longer emits it. What
remains is 23 errors, 1 warning and 8 infos, and the infos are the BOOT LOG, which is a fair use of
a line that runs once per run.

## What is left, and it is the reason those counters were logged

`world/ground/` CANNOT write to the ledger: `Published` belongs to the engine. Those classes logged
their numbers because the stats channel does not reach them. Removing the log lines removed the
symptom.

The shape that fixes it is already in the tree, for generators only: `Making::NoteNames()` declares
what a class counts and the frame PULLS it. Every class that keeps numbers should be able to say so
the same way, rather than pushing them somewhere or writing them as prose. That is the next step
here, and it is the same missing piece board:2108 names from the other side.

## What this item does

**Benchmark**: Unreal declares the CATEGORY as a symbol (`DECLARE_LOG_CATEGORY_EXTERN`) and leaves
the MESSAGE free text; RAGE declares channels as macros. Both need a macro because C++ has no
reflection, and both stop at the category. outshine can go one step further without a macro,
because its 18 events are already named rather than written as prose:

    static constexpr LogTag kTag = LogTag::Render;   // who I am, once per class
    Log::Error(kTag, Says::kMapFailed, {{"why", SDL_GetError()}});

The tag is an enum -- a closed set of four. The event is a declared `constexpr` label, so a
`static_assert` can hold the set the way this tree already holds shader entries against their
callers. The FIELDS stay open and typed, because every value is different.

`{"msg", ...}` is the seam where prose slips past the event, eleven times. Those eleven become
events.

**The measurement that shows I was wrong:** if adding a diagnostic then costs more than a line, the
declaration is too heavy and Unreal's free message was right. Count the lines a new event needs
before and after.
