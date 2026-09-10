Type: feature
State: active
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

## Typisierte Tabellen als geprüfte Datenbasis
TableBook trennt jetzt Schema, Zahlenkonvertierung, Zeilenaufbau und Publikation;
ParseFiniteNumber ersetzt den eigenen Parser. Nicht endliche Zahlen, doppelte/leere
Spaltennamen und überzählige Typangaben werden abgelehnt. Spaltenvalidierung sortiert
geliehene string_views, ohne quadratische Suche. Table-ID/Spaltennamen nichtleer und eindeutig,
mindestens eine Spalte, exakte Zeilenbreite; fehlende Typangaben bleiben Text.
4096-Zeilen-Grenze erhalten. Erste Zelle ist der eindeutige, unveränderte Textschlüssel
(auch bei Zahlenspalten); endliche Dezimalzahlen vollständig lesen, keine Clamps.
Zellspeicher einmal mit geprüftem Produkt reservieren, Zeilen direkt darin aufbauen.
TableBook und Simulation publizieren nur vollständige Kandidaten. Vier Tests prüfen
Schema-/Zahlenfehler, eigene Datenspeicherung, typisierte Abfrage, Schlüssel, Grenzen
und erhaltene Simulation nach Assembly-Ablehnung. Beide neuen Tests scheitern im Altstand.
Öffentliche Table-Verträge sind dokumentiert; keine Quest-/Script-Fähigkeit behaupten.

## Eindeutige Trigger-Ereignisse
Der verengte uint16_t-Sentinel ist entfernt. Genau 2^16 Ereignisse sind unterstützt
(Indices 0..65535), größere Kataloge werden vor Aufbau abgelehnt. Nichtleere eindeutige
Namen werden mit kurzlebigem string_view-Index aufgelöst. Ereigniskatalog und
Volume-Vorbereitung sind getrennte Phasen; Occupants speichern keinen redundanten
Volume-Index. Assembly prüft und übernimmt Events auch ohne Volumes.
Fünf Tests bestehen: erster/letzter Index samt Listener/Zählern, 65537 Einträge,
doppelte/leere Namen, unbekannter Verweis und Simulationserhalt. Beide neuen Fälle
scheitern im Altstand. Event-Vertrag dokumentiert: Feldnamen sind keine Payloadwerte.
Offen bleiben Live-Probe-Validierung und die vereinfachte Probe-Zustandsmaschine.

## Geometrische Trigger-Grenzen
Implementiert: endliche Zentren und nichtnegative endliche Ausdehnungen prüfen;
Dwell braucht endliche positive Dauer. Nullausdehnung bleibt eine gültige geschlossene
Punkt-/Flächenmenge. Sphere nutzt ExtentM.x als Radius; y/z bleiben ungenutzt, aber gültig.
Quadratsummen durch std::hypot ersetzt: große endliche Distanzen dürfen nicht durch
inf <= inf als innerhalb gelten. Analytische Rand-, Außen- und Extremwerttests scheitern
im Altstand und bestehen mit der Korrektur; alle drei Trigger-Tests bestehen. Live-Probe-Zeit/Entity-Validierung und Zustandsmaschine separat offen.
