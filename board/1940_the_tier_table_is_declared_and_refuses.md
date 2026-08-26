Type: feature
State: open
Progress: layers
Area: architecture
Tags: benchmark, target

# Every module declares what it reaches, and the gate refuses a crossing and a cycle

Unreal declares this per module in `Build.cs` and the build refuses a dependency nobody declared.
RAGE separates `rage::` from the game layer by convention and reconstruction says the boundary
holds. outshine declares 13 tiers in `test/run.sh` and audits them. This is the area where the
tree is level with the benchmark, so the list is short and mostly held.

- [x] every source includes only what its tier declares it reaches
      proof: --audit-layers
- [x] a CYCLE between two modules inside one tier refuses, which the tier table alone cannot see
      proof: --audit-layers
- [x] the layer table is spelled ONCE and the gate reads that spelling
      proof: harness/claims/TheLayeringIsDeclaredOnce
- [x] no two headers guard under one basename, because both walks resolve by basename
      proof: harness/claims/EveryGuardSpellsItsFolder
- [ ] the tier a source belongs to is derivable from its PATH alone, with no table to keep --
      today a new directory needs a row added by hand
- [ ] a module declares its own reach beside itself, the way `Build.cs` sits in its module,
      rather than every reach living in one file at the root
