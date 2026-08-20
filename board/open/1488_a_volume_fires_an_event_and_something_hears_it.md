Type: task
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
