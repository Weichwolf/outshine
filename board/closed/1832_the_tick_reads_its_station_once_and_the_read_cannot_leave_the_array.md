Type: bug
Area: sim
Tags: hot-path, bounds, optimisation

# The tick reads its station once, and the read cannot leave the array

## Six lookups of one value, one of them inside the wheel loop

`Corridor::At` is called six times per tick with the SAME argument:

```cpp
src/sim/DriveTick.cpp:87    const Station &here = way.At(at.AlongM);
src/sim/DriveTick.cpp:143       const double edgeM = way.At(at.AlongM).EdgeM;
src/sim/DriveTick.cpp:181       way.At(at.AlongM).EdgeM - std::fabs(at.OffsetM) - 0.5 * drive.CarWidthM;
src/sim/DriveTick.cpp:192       const double halfRoomM = way.At(at.AlongM).LaneHalfM - 0.5 * drive.CarWidthM;
src/sim/DriveTick.cpp:231     const Station &left = way.At(at.AlongM);
src/sim/DriveTick.cpp:244     out.LeftHalfWidthM = way.At(at.AlongM).EdgeM;
```

`:143` sits inside `for (size_t which = 0; which < rig.Count; ++which)` and is loop-invariant:
four wheels, four divisions and four bounds branches for one value the block above already
bound to `here`. The tick is the frame path; a value read once is the house rule
(*fast path on the hot path*), and `here` is already the name for it.

## The bound is a caller convention, not a property of the type

```cpp
src/sim/CorridorLay.h:58   [[nodiscard]] const Station &At(double alongM) const {
src/sim/CorridorLay.h:59     const size_t fine = (size_t)(alongM / FineM);
src/sim/CorridorLay.h:60     return Fine[fine < Fine.size() ? fine : Fine.size() - 1];
```

| input | what happens |
|---|---|
| `Fine` empty | `fine < 0` is false, so the index is `Fine.size() - 1` = `SIZE_MAX` -- out of bounds |
| `alongM < 0` | conversion of a negative double to `size_t` is undefined behaviour |
| `FineM == 0` | division by zero, then a conversion of `inf` to `size_t` -- undefined behaviour |

`board:1820` closed on *"an unlaid corridor refuses at the tick's entry instead of returning a
default per read"*, and the refusal it landed is one line in one caller:

```cpp
src/sim/DriveTick.cpp:45   if (!way.Laid()) { return out; }
```

Every other caller of `At` -- and `At` is public on a public product -- is unguarded. A refusal
that lives in one caller is a convention, which is what the item set out to remove.
`FineM` is a public mutable `double` with a default of 2.0 (`CorridorLay.h:44`), so a
zero-division reaches `At` from an assignment, not from a bug in the lay.

## What will be true

- [ ] `DriveTick` binds ONE `const Station &here` for the tick and every reader uses it; nothing
      calls `At` inside the wheel loop.
- [ ] `At` cannot be reached on an empty `Fine`: either the type makes an unlaid corridor
      unspellable (`Bake` returns the laid thing, and `Corridor` holds it by value), or `At`
      takes a bound station index a `Laid` corridor hands out, so the clamp is not a runtime
      branch on the frame path at all.
- [ ] `FineM` is not a settable double: it is a constant of the type or arrives once through
      `Bake`, and `Bake` refuses a non-positive step.
- [ ] `Station` carries a `static_assert` on its size and triviality beside the struct, as
      `Ridden` does at `src/sim/DriveTick.cpp:18`.
- [ ] `At`, `Laid` and `AsideRatePerM` are `noexcept`.
- [ ] Proving test: `test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld` gains a case that
      ticks a `Corridor{}` -- never baked -- under the address sanitiser and asserts the tick
      returns `!Found` without a read. Negative control: `if (!way.Laid())` removed -> ASan
      names the out-of-bounds read at `CorridorLay.h:60`.

**Closed on four of five boxes, with the fifth named.**

```cpp
src/sim/DriveTick.cpp:87   const Station &here = way.At(at.AlongM);   // once, for the whole tick
```

Six calls became one. `here` used to live inside the aside block's braces, which is why the
five readers below it each opened the array again -- including one inside the wheel loop, four
divisions and four bounds branches per tick for a value already bound.

```cpp
src/sim/CorridorLay.h:47   static constexpr double kFineM = 2.0;     // not a settable double
src/sim/CorridorLay.h:60   Fine.assign(lengthM > 0.0 ? ... + 2u : 1u, Station{});
src/sim/CorridorLay.h:63   [[nodiscard]] bool Laid() const noexcept
src/sim/CorridorLay.h:65   [[nodiscard]] const Station &At(double alongM) const noexcept
src/sim/CorridorLay.h:66   const size_t fine = alongM > 0.0 ? (size_t)(alongM / kFineM) : 0u;
```

`FineM` was a public mutable double read at exactly one site and written at none, so a
zero-division could reach `At` from an assignment; it is a constant of the type. A `Bake` of no
length leaves ONE station rather than none, so a corridor the lay produced can always be read.
`Station`'s `static_assert`s landed with board:1828.

**The fifth box -- an unlaid corridor being unspellable -- stays open**, and the reason is named
rather than deferred: making it a type property means `Fine` private and `Corridor` constructed
only through a factory, which changes every filling site in `LayCorridor` and both twins. It is
the right shape and it is a separate piece of work; `Laid()` plus the entry refusal is a
convention until then, which is what the item says.

Proving test: `unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld` -- a station before the start
is the first, a station past the end is the last, and a `Bake(0.0)` still leaves one to read.

**The negative control did not control, and that is the honest result**: removing the
`alongM > 0.0` guard leaves the case GREEN, because `(size_t)(-0.5)` is undefined behaviour that
this platform resolves to 0. The guard's value is that the answer stops depending on which
platform resolves it -- a claim a test on one platform cannot make red. The six-reads-to-one
change is likewise not red-able: it is the same answer computed once, and the tree has no
tick-cost case in the gate to measure it.
