Type: bug
Area: generators
Tags: static-assert, telemetry

# A note table proves it names every note

Four generators publish their telemetry names as a C array sized by the enum that indexes it:

| file:line | table |
|---|---|
| `src/generators/Forest.cpp:33` | `static const char *const kNames[kNotes] = {` |
| `src/generators/Water.cpp:6` | `static const char *const kNames[kNotes] = {"waterSurfaces", ...` |
| `src/generators/Buildings.cpp:16` | `static const char *const kNames[kNotes] = {"footprints", "roofless", "highestRoofAglM"};` |
| `src/generators/Infrastructure.cpp:6` | `static const char *const kNames[kNotes] = {"ways", "widestWayM"};` |

**A `Note` added to the enum without a name beside it is not a compile error.** The array is
sized by `kNotes`, so the missing entries are value-initialised to `nullptr`, the span handed to
`Yield` (`src/clients/RegionForge.cpp:88-92`) is full length, and the telemetry publishes a null
name. Silent, and the consumer prints it.

This is not hypothetical: `a17ed496` inserted `NoSpecies` into `Forest::Note`
(`src/generators/Forest.h:33-35`) and had to remember to insert `"noSpecies"` into the table
(`src/generators/Forest.cpp:33-36`). It was remembered. The next one need not be.

## What will be true

- [ ] Each table is declared without an explicit bound and proven against the enum beside it:

      ```
      static constexpr const char *const kNames[] = {...};
      static_assert(std::size(kNames) == kNotes);
      ```

      -- the obligation the type system can hold, held by the type system, which is what
      CLAUDE.md means by `static_assert` over checkers.
- [ ] No note name is empty, proven the same way where a `constexpr` check can reach it.
- [ ] Negative control: a `Note` added with no name -> the translation unit does not compile.

## Comments

- 2026-08-24, reviewer round -- filed against the delta's own edit. The repair is four lines
  and removes a class of silent telemetry corruption from every generator at once.
