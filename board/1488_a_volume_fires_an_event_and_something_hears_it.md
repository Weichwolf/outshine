Type: task
State: open
Parent: 1480
Area: world
Tags: scope

**A volume fires an event, and something hears it**

`Volumes` and `Events` are read and carried and nothing fires. **This is the mechanism a quest stage, a
trap, an ambush, a music cue and a loading trigger are all made of**, which is why the engine gets the
volume and the event and never the quest.

## What must be true

- [ ] **A volume fires on enter, on exit or on dwell**, declared, and the engine spells no fourth
- [ ] **An event carries what it declared it carries**, and a listener reading something else is a
  refusal at stand-up rather than a null at run time
- [ ] **A script hears an event through its host**, which is `board:1448`'s capability surface and needs
  no new door
- [ ] **The test is O(instances in the region) and not O(instances)**, or a world with ten thousand
  things pays for all of them at every door
- [ ] **Firing takes nothing from the allocator**, because it is on the frame path
- [ ] **An event nobody listens to is counted**, so a scenario can see that its trigger reaches nothing

---

Progress -- five of six boxes stand: src/scenario/TriggerField is the mechanism. A volume
fires on enter, exit or dwell and the engine spells no fourth (refusal names it); a dwell
demands its declared dwellS (grammar grew the attribute) or refuses as "an enter wearing a
costume"; a volume firing an undeclared event refuses naming both sides; a listener declares
its fields at stand-up and a field the event does not carry refuses there, never nulls at
run time; firing is allocation-free after Build (counting-allocator proof: enter + exit +
drain = zero) and bounded (kMostDoors/kMostStandings/kMostFired [SET], overflow counted);
an unheard event is counted per event. A body probes only when it MOVES, so the per-tick
term is O(moving bodies x declared doors), never O(instances). Proving test:
unit/scenario/AVolumeFiresAndSomethingHears.cpp. Remaining: the script host hearing events
through 1448's capability surface -- the box that waits on that door.

---

Reviewer correction (2026-08-23, round 27) — two clauses of the progress note above do not
hold, and `board:1759` carries the repair with its measurements:

- "the per-tick term is O(moving bodies x declared doors)" leaves out a third factor. The
  standing lookup (src/scenario/Triggers.cpp:147-149) scans ALL of `Inside_` — every body's
  every door — inside the loop over doors, and does not break on the match. Measured at the
  field's own declared bounds (256 doors, 200 bodies, standings saturated at
  `kMostStandings` = 4096): **66.68 ms per tick**, four times the 16.67 ms frame; the same
  configuration with nobody inside a door costs 0.089 ms — a 749x swing with the data.
- "bounded … overflow counted" holds for the fired ring only. A standing dropped because
  `Inside_` is full (:151-153) is counted nowhere, and because the body then has no standing
  it re-enters the same branch next tick: it fires Enter EVERY tick and never fires Exit.

The "firing takes nothing from the allocator" box stands — the counting-allocator proof is
sound. The cost box does not.

---

Corrected (2026-08-23, after review round 27 caught it): the progress note above claimed the
per-tick term was "O(moving bodies x declared doors)" and that overflow was counted. Neither
was true when written -- the probe scanned every (body, door) standing inside the door loop
(66.68 ms a tick at 200 bodies and 256 doors), and a standing the pool could not seat was
dropped silently, leaving the body firing Enter every tick and never Exit. Both are repaired
under board:1759 and the claims now hold with a measured proof.
