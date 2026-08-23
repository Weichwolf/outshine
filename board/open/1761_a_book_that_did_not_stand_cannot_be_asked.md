Type: bug
Area: scenario
Tags: refusal, type-system, c++23, undefined-behaviour

**A book that did not stand cannot be asked**

`ViewBook` is default-constructible and its accessors index unconditionally:

```cpp
[[nodiscard]] const View &Active() const { return Held_[Active_]; }        // Views.h:27
[[nodiscard]] std::string_view ActiveId() const { return Held_[Active_].Id; } // :28
[[nodiscard]] double ClockScale() const { return Held_[Active_].TimeScale; }  // :32
[[nodiscard]] std::string_view ListensFrom() const { return Held_[Active_].Follows; } // :35
```

`Build` (src/scenario/Views.cpp:6-49) clears `Held_` on entry (:8) and returns `false` on
six separate refusal paths — every one of them leaves `Held_` EMPTY and `Active_` at 0. A
`ViewBook` that never stood, or whose stand-up refused, answers `Held_[0]` on an empty
vector: out-of-bounds read, undefined behaviour, no refusal, no loud failure. `ListensFrom`
and `ActiveId` hand back a `string_view` into that read.

`[[nodiscard]] bool Build` makes ignoring the verdict a warning, not an impossibility — and
a warning is not the tree's standard. CLAUDE.md: *refusal at assembly over runtime checks*,
*`std::expected` where a refusal carries its reason*, *the type system over checkers*. A
type whose invariant ("exactly one active view") holds only if the caller read a bool is
the anti-pattern those three lines name.

The same shape stands beside it: `TableBook` (src/scenario/Tables.h) and `TriggerField`
(src/scenario/Triggers.h) are also default-constructible with a `bool Build`, though their
accessors happen to bounds-check today (`TableBook::At` returns nullptr on a miss;
`TriggerField::EventNamed` compares against `Events_.size()`). `ViewBook` is where the
pattern actually bites.

## What will be true

1. An unbuilt book has no spelling: the factory is
   `static std::expected<ViewBook, std::string> Stand(std::span<const View>, std::string_view starting)`,
   the constructor is private, and the six refusal texts travel as the error — the same
   sentences, now unignorable. The three sibling books follow the same form so the layer
   has ONE stand-up shape.
2. `Active()` and its neighbours become `constexpr`/`noexcept` where they then hold, because
   after the factory there is nothing left to check.
3. A unit case in test/unit/scenario/ proves the refusal carries its reason and that no
   route exists from a refused stand-up to an answer — a compile-refusal case under
   test/unit/compile/scenario/ if the default constructor is the thing being deleted.
