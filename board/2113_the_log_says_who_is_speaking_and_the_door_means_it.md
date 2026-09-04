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
