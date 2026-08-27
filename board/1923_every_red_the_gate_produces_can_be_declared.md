Type: issue
State: open
Area: test
Tags: gate, guard

# Every red the gate can produce has a declaration channel, so no failure is nameless

**Benchmark** — Unreal: an expected failure is declared in the automation filter. RAGE: known-issue lists. **Both agree** — a nameless red is a red nobody owns.

`EXPECT_FAIL` declares a standing red per CASE with its count, and the gate turns red the day a
declared case passes. `EveryProgramStillLinks` produces a different red -- a program that does
not build or does not answer `--help` -- and had no such channel, so the failure appeared in the
verdict with no name, no reason and no expiry.

`EXPECT_UNLINKED` closed that hole for programs. `EXPECT_UNPREPARED` closes it for oracles: an
oracle that CANNOT EXIST is a different thing from one nobody rendered, and both arrived as
UNPREPARED in a count with no name and no expiry. Three cases cannot be prepared with this
toolchain at all -- Blender's glTF importer refuses `KHR_node_visibility` (`CubeVisibility`,
`LightVisibility`) and crashes on `KHR_animation_pointer` over `KHR_texture_transform` with
`KeyError: 'animations'` (`AnimationPointerUVs`). Nine arms, declared with their reason, and the
day the importer grows the extension the gate turns red on the stale line.

The question this item still holds open is whether any OTHER red the gate can produce is
undeclarable:

- `EverySourceStillCompiles` -> `compileBlind`
- `undeclaredSkips` and `compileBlind` in the verdict
- the audits (`--audit-layers`, `--audit-access`, `--audit-numbers`, `--audit-link`), each of
  which refuses on a count moving

Each needs the same treatment or a stated reason why it does not: a red that cannot be declared
is a red somebody silences by deleting the check.

## What will be true

- [x] `unprepared` has one: `EXPECT_UNPREPARED`, with both arms controlled -- a declared case
      that PREPARES refuses as a stale declaration, an undeclared one refuses bare.
- [ ] Every remaining refusal path either names its declaration variable or states in one line
      why a standing instance of it is impossible.
- [ ] Proving case: a claim walks the runner's refusal paths and refuses when one has no
      declaration channel. Negative control: the path removed from the walk, and it passes.
