Type: task
State: open
Area: src
Tags: hygiene, diagnostics, shaders

# A diagnostic is a LABEL, and a shader takes its constants as data

**Benchmark** — Unreal: user-facing text goes through `LOCTEXT` string tables; developer diagnostics stay as literals at the point of failure. RAGE draws the same line. **Neither NAMES its diagnostics**, so this borrows from a third place that does: **Business Central AL**, where every message is a declared `Label` with placeholders and the call site passes values into it. That is a better answer than either and it costs nothing in C++23.

## The shape, and it needs no new type

    namespace Says {
    inline constexpr std::string_view kADrawNamesASlotOverTheTable =
        "a draw names placement slot {} and {} instance(s) over a table of {} placements";
    }
    ...
    error = std::format(Says::kADrawNamesASlotOverTheTable, batch.ModelSlot, batch.Instances, rows);

    named          the refusal has an identity to grep, to cite in an item, to count
    listed         a file's ways of refusing are a BLOCK at its top: its failure modes read
                   without its logic
    checked        a wrong placeholder count is a BUILD ERROR -- measured, six of them
    no machinery   no macro, no table, no identifier between a failure and its text

**Measured before proposing.** A `constexpr std::string_view` is accepted by `std::format` as a
compile-time-checked format string, and `std::format(Says::kTwo, 1)` against `"{} and {}"` fails to
compile. So the AL property that matters -- the text is declared, named and checked -- arrives with
the standard library and nothing else.

**Where they live: the file that raises them**, in an anonymous `namespace Says` at its top. NOT a
global catalogue: an identifier between a failure and its explanation is exactly the indirection
that lets a message drift from the code it describes, and AL puts its labels in the object that
raises them for the same reason.

**Why this matters MORE now than last week.** `make` deletes every comment `src/` may not keep, so
the refusal strings are the only prose left in the motor. Naming and declaring them is what stops
them becoming the new place narration collects.

## The endpoints are labels too, and that closes the other half

    src/world/data/VersatilesVector.cpp    "https://tiles.versatiles.org/tiles/osm/{}/{}/{}"
    src/world/data/TerrariumDem.cpp        ".../terrarium/{}/{}/{}.png"

An earlier draft of this item called these hard-coded CONFIG and demanded a JSON asset. That was
wrong: `VersatilesVector::Url` returning a versatiles URL is not a setting, it is **what that class
IS** -- a configurable endpoint belongs to a generic `WebTileSource` a declaration parameterises,
which is a different design and not this one. As named labels they are declared, greppable and
checked, and `grep -rn 'https\?://' src/` comes back empty. Done.

## What is left

- [ ] the remaining diagnostics become labels: 172 assignments in `src/`, of which 5 are converted
      (`SubjectResidency.cpp`) as the worked example. Staged like the lint baseline -- new code is
      held to it and old code converts as it is touched
- [ ] `make lint` refuses a raw string literal reaching a refusal, so a free-floating diagnostic
      cannot come back
- [ ] no `"constant ` in `src/`: nine values are printed into shader PREAMBLES today
      (`MediumMultiScatterStage` 4, `MediumRadianceStage` 3, `MediumTransmittanceStage` 2), so a
      shader is compiled per value and the two sides agree only because one printed the other. The
      shaders themselves are already assets -- every stage loads `src/render/shaders/*.msl` and
      there is no inline MSL anywhere. A uniform is what SDL_GPU offers; Metal's function constants
      it does not expose

## What this does NOT cover

**User-facing text does not exist yet.** When it does it goes in a table from the first line,
because a translator and a designer read it and neither can be asked to edit C++. That is a
different genre from a refusal read by the person who broke the build, and conflating the two is
what makes diagnostics worse in every codebase that tries it.
