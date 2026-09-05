Type: feature
State: open
Area: scenario, engine
Tags: architecture, owner, ai-first
Depends: 2131, 2136

# A scenario declares a WHOLE game, and a minimal RPG played through the door proves it

**Benchmark** -- Unreal: a game is Blueprints and `GameMode`/`GameState`/`SaveGame` objects a
studio scripts; RAGE: a script VM (`scr`) over the world with `STAT_` and save blocks. **Both
agree** that state, ownership, inventory and persistence are the ENGINE's records and the game's
rules are content over them. Neither has an author who is an AI writing text; here the rules
are the scenario itself and the only authoring verb is a declaration, so the door has to carry
what a script carries in the references. **The choice is mine**, and the front page names it:
a scenario that cannot be written is a game that cannot be made.

## Where it stands, measured 2026-09-05

```
  Scenario::Document       Kinds, Instances, Regions, Doors, Events, Tables, Bodies, Player,
                           Clock, Input, State (Persisted)         include/scenario/Scenario.h:670-701
  Scenario::Mind           Tier, Programme, Prompt, Model, Hz, budgets   Scenario.h:292-306
  the engine acts on       Kinds and Instances, a Script::Program; Minds, Regions, Doors are
                           carried by Unacted() and acted on by nothing   Declaring.cpp:312
  round trip               writeScenario() exists                     include/Outshine.h:174
  inventory · ownership    no section
  quest state · time       Events and Tables exist as records; nothing advances a quest
  save · load              Persisted is a record; nothing writes a played state back
```

## The solution

The door gains the records a game is made of and NOTHING that decides how a game goes:

- **holdings**: who owns what (`Holding{Owner, Item, Count}`), a table the engine keeps and a
  mind's `give`/`take` verbs move; the scenario seeds it and reads it back
- **state**: a quest is `Table` rows a mind reads and writes through declared verbs, and
  `Event`s are what a change of a row fires; no engine code knows the word quest
- **time**: `Clock` advances the world and the minds' schedules (`EverySeconds`)
- **save and load**: `writeScenario()` writes the PLAYED state (holdings, tables, positions,
  clock) as a scenario, and declaring that scenario resumes the game -- the round trip of
  board:2131 carried to a running world

## What will be true

- [ ] A minimal RPG -- one place, three minds, one quest whose completion is a table row -- is
      ONE scenario file with no code beside it
- [ ] Played through the door for N steps, written back, declared again and played the same
      N steps: the two runs' pictures and tables are byte-identical (the minds' answers
      replayed from board:2142's event log)
- [ ] Every section the RPG uses round-trips: read, write, diff empty (board:2131)
- [ ] Negative control: a scenario that gives a mind an item no `Asset` declares is REFUSED at
      declare, loudly, and a mind's `give` of an item it does not hold is refused at act
