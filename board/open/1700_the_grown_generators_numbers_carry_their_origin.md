Type: issue
Area: generators
Tags: numbers

**The grown generators' numbers carry their origin**

1693 gave the data boundary its origin marks; the generator layer under `src/generators/`
is the densest remaining carpet of unmarked constants. Every number is to carry
derived · measured · `[SET]` with unit and population — representative sites:

- `src/generators/Forest.cpp:19`: `2.4494897` is √6, the variance normaliser for the
  two-uniform (Irwin-Hall) height draw — derivable, but nothing says so; a reader cannot
  tell it from a tuning residue.
- `src/generators/draw/TreeGrower.cpp:14-20`: `kEscapeStop 1.10`, `kBendBack 0.55`,
  `kMinBranchSteps 3`, `kCapReach 2.4` — names, no origin. :103 `0.18f` stool spread,
  :109-121 the leader splay/jitter set (0.35, 0.55/0.45, 0.34, 0.94), :266 `4.0f` pull gain,
  :410 `1.6f` first-pass crown guess — all bare.
- `src/generators/draw/TreePrototype.cpp:16`: `kLeafBaseLinear {0.0684, 0.1072, 0.0273}` is
  a measured chlorophyll albedo or it is a guess — the difference is exactly what the mark
  exists to record. :47-49 the leaf-beta shape constants likewise.
- `src/generators/draw/RoofSurface.cpp:131,187`: the sawtooth `0.85/0.15` split appears in
  two files' arithmetic and must not drift apart unmarked.

Not demanded: prose per line. Demanded: each constant either derives visibly (name or one
`[derived]` note with the arithmetic), or wears `[SET]` with what population set it — the
same standard 1693 applied to `src/data/`, and the grown corpus stays green as the proof
that marking changed nothing.
