Type: defect
State: active
Area: test, render
Tags: measured, guard, regression

# The entry check knows which arms a TABLE generates

**Benchmark** — Unreal declares its permutations in `FShaderPermutationParameters` and lets the
compiler enumerate them; the check that a permutation exists is a build-time one, not a grep. RAGE
does the same through its shader groups. **Both agree** that a generated set is proven by
GENERATION, never by two lists agreeing. This tree already reached that answer once and wrote it
down.

## The check has met this before, and said so

`test/scripts/entries_vs_shaders.py` compares entry points spelled in an `.msl` against name
literals spelled beside the renderer. Its own comment records what happened when the VERTEX arms
became a table:

> The sixteen vertex arms are now GENERATED from VertexArms.h's table, so they appear in neither --
> both of this check's inputs lost the same fifteen names at once and the difference stayed zero.
> It stayed GREEN through the change.

**The fragment arms have now gone the same way and the symmetry broke.** `FragmentArms.h` replaced
the renderer's literals with a table, but the `.msl` still DEFINES every entry as text, so only one
of the two inputs lost its names:

    before   23 entry point(s) the shaders define, 22 the renderer names, 0 and 0
    now      24 entry point(s) the shaders define,  4 the renderer names, 19 defined and unnamed

The 19 are not unreachable. They are reached through the table and held by
`static_assert(EveryFragmentArmIsAtItsOwnIndex(), ...)`, which is STRICTER than the grep ever was --
the same argument the vertex arms already carry. What is lost is the check's ability to see a
genuinely unnamed entry: a real one would now stand as the twentieth in a list of nineteen false
ones, and nobody would look.

## And it does not gate, which is the other half

    test/lint.sh:74   sh test/run.sh harness/claims > "$REPORT/claims.log" 2>&1 || true

`make lint` RUNS the tree's own claims and cannot be failed by them -- the summary line
`lint: 36 tests: 22 PASS 14 FAIL` is printed and discarded. `make help` calls lint "this tree's own
repository rules", and the rules are exactly those claims. The gating happens in `make test`, so
nothing is unguarded; what is wrong is that a run which prints 14 red cases exits 0 and reads green
to anyone watching the last line.

## What will be true

- [ ] The fragment arms are excluded the way the vertex arms are, and the exclusion NAMES what
      holds them instead -- the static_asserts in `FragmentArms.h` -- so the count returns to
      `0 defined and unnamed` for a reason rather than by coincidence
- [ ] Negative control: an `.msl` entry that NO table row and no literal reaches makes the check
      go red. Today it would be one line among nineteen
- [ ] `make lint` either fails on a red claim or stops printing a test summary it does not stand
      behind. A line that looks like a verdict and is not one is worse than no line

## What this does NOT cover

Whether the arms are RIGHT. The static_asserts prove every arm sits at its own index and that the
catalogue is exhaustive; they say nothing about whether the shader at that index draws what the
domain wanted. That is the picture's job and `make shots` holds it.
