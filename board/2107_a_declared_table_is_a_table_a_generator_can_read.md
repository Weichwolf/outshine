# A declared table is a table a generator can read

State: open

A scenario may declare `<tables>`; `include/scenario/Scenario.h` carries `std::vector<Table>`, the
reader fills it, `Engine::Declare` builds a `TableBook` and puts it in the session. And then
NOTHING READS A CELL. `TableBook::Number` and `TableBook::Text` have no caller anywhere in the
tree, and `Tabled` sits in `EngineHeld` where no client can reach it.

That is a capability a declaration reaches half way. A scenario can be told something the engine
can never act on, which is the failure CLAUDE.md calls loud: accepting a declaration and doing
nothing with it is worse than refusing it.

The book itself is sound and stays -- `mdspan` grid, a `ByKey` index, a numeric/text distinction
the reader enforces. What is missing is the consumer.

**What Unreal does:** data tables (`UDataTable`) are assets a Blueprint or C++ class looks up by row
name, and the row struct is the schema. The lookup is a first-class engine verb, not a side table.

**What RAGE does:** `parCodeGen`-generated metadata and `configFile` tables are read by the systems
that need them at load, and a table nothing loads is not shipped.

**Taken:** Unreal's. A table is looked up BY A GENERATOR through a reader it is handed, because that
is who has a use for one -- a species table for a flora generator, a storey table for buildings.
This closes when a generator reads a declared cell and a case declares a table whose value changes
the picture.

**The measurement that shows I was wrong:** a place whose declaration carries a table and whose
digest is identical with and without it. If the value cannot move a picture, the section should be
removed from the door instead.

Beside it, deleted in the same round: `src/base/math/CatmullRom.{h,cpp}` -- also unreached, but
unlike the book its quality did not earn a second look. `step > 0.0 ? step : 1.0` is a number with
no derivation, the tangents are written unscaled where glTF's CUBICSPLINE wants them scaled by the
segment, and the interface is `const double *` plus two `size_t` in a tree that has `mdspan`. When
a spline is needed it is written against a corpus, not recovered from `git log`.
