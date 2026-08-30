Type: task
State: open
Area: architecture
Tags: library, door, corpora

# The script side is a LIBRARY with its own door, proven by its own corpora

**Benchmark** — Unreal: Slate/UMG is a module with its own public interface and the engine depends
on the module, not on its internals. RAGE: `rage::grcore` and the UI layer are separate libraries
linked by declaration. **They agree**, and this tree already holds the pattern: `src/generators/`
is "a library with its own door -- a client registers its own beside them, and the tier links with
none of the engine behind it" (CLAUDE.md).

## What was measured

    src/base/format/Script.{h,cpp}     1225 lines   JS
    src/ui/Markup.{h,cpp}               350 lines   HTML
    src/ui/Style.{h,cpp}               1076 lines   CSS
    src/ui/Layout.{h,cpp}              1530 lines   box layout
                                       4181 lines

Reached from FOUR files outside `src/ui/` and `src/base/format/`: `SubjectDraw.cpp`, `Overlay.h`,
`Live.h`, `EngineHeld.h`. A four-point coupling over 4181 lines is a library that has not been
declared one.

## What will be true

- [ ] the four parts stand as one tier with ONE public header, and `reaches` names what it may see
- [ ] WPT and test262 are ITS corpora, scored at ITS door, and they pass
- [ ] the engine links it the way it links `generators/`: through the declaration, with none of
      the engine behind it
- [ ] the scorers move out of `test/harness/` and stand beside the corpus they score, so
      `test/harness/` holds no C++ at all

## Why this resolves a collision rather than adding work

Two rules were stated together: `test/harness/` holds no C++, and the html/css/js and geodesic
corpora test the code DIRECTLY. Both hold only if those scorers live somewhere other than
`harness/`. A library with its own door gives them that somewhere, and it is the place they
belonged anyway -- a corpus proves the thing it is a corpus FOR, and it should compile against
that thing's door rather than against the engine that happens to contain it.

## What this does NOT cover

Whether the UI should keep using it, and whether a browser-shaped layout engine is the right
answer for an engine overlay at all. That question is real and it is not this item's -- this item
only says that whatever it is, it is a library and it is proven at its own door.
