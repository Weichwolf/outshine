Type: issue
Area: scenario
Tags: declaration hygiene, loud-failure, two-truths, xml

**The grammar table and the readers are one truth**

`kGrammar` (src/scenario/ScenarioRead.cpp:27-103) declares, per element path, the children
and attributes a scenario may spell. `ReadScenarioInto` (:232-609) separately reads
attributes by name. **Nothing ties the two together**, and they have already drifted in both
directions.

## Drift one — the grammar permits what nobody reads

| grammar row | attribute | read by |
|---|---|---|
| src/scenario/ScenarioRead.cpp:89-90 `scenario/vehicle/contact` | `node` | nobody |
| src/scenario/ScenarioRead.cpp:95 `scenario/vehicle/seat` | `node` | nobody |

`Contact` (include/outshine/Scenario.h:219-228) has no field for it; the reader takes `at`
and x/y/z (:556-557) and `made.SeatAt` (:578) and never asks for `node`. A declaration that
spells `<contact node="wheel_fl"/>` loads clean and the value evaporates. This is exactly
the failure the JSON side already refuses by path
(test/unit/scenario/AnUnreadPropertyIsRefusedByItsPath.cpp, `Fields::Closed`,
src/scenario/Fields.h:74-83) — the XML door has no equivalent.

## Drift two — the grammar's `Required` column is empty where the reader's consumers demand

`scenario/views/view` (src/scenario/ScenarioRead.cpp:82-83) declares NO required attribute.
`ViewBook::Build` (src/scenario/Views.cpp:16-41) then refuses a view without `id`
("a view nobody can take is dead weight"), without `follows`, and with a `person` that is
not `first`/`third` — and `person` has no default (`made.Person = one.Attr("person")`,
ScenarioRead.cpp:519, absent → `""`), so every view MUST spell it while the grammar says it
may omit it. The refusal is right; the grammar is a second, weaker truth standing in front
of it, and a scenario author reads the grammar's answer first.

The same shape is latent on `scenario/regions/region` (`id` optional in the grammar,
`ByIdField` in ScenarioLayer.cpp:52-55 treats an empty id as "always add") and on
`scenario/tables/table` (`id` optional in the grammar, `TableBook::Build` keys by it).

## What will be true

1. **The grammar is derived from the reader or the reader from the grammar — one of the
   two spells the attribute, never both.** The straight form: each element becomes a
   `constexpr` row of typed fields (name, kind, required, where it lands), the reader walks
   that row, and a typo is a compile error rather than a table edit — the tree's own
   "values over strings · constexpr catalogue" rule, applied to the door that reads the
   declaration.
2. Until then, a claims-style test enumerates `kGrammar` and asserts that every attribute
   it permits appears as a literal in the reader's `Attr`/`Num`/`Int`/`Flag` call for that
   path — the test that would have caught `node` on the day it was added, and which fails
   at HEAD on exactly those two rows.
3. Every attribute a stand-up (`ViewBook`, `TableBook`, `TriggerField`, `InputMap`) refuses
   the absence of appears in that element's `Required` list, so the refusal arrives at the
   grammar and names the element, not four layers later.
4. `Names(const char *list, …)` (:105-116) stops building a `std::string` and a `substr` per
   attribute per element: the lists are compile-time literals and the comparison is a
   `string_view` walk.
