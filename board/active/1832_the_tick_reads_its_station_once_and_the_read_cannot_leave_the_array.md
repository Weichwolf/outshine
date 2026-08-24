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
