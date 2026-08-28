Type: bug
State: active
Area: door, gltf
Tags: measured, spec

# the door says WHICH animation plays, and `Play` means one rather than all

**Benchmark** — Unreal: an animation is an ASSET selected by name into a slot; nothing plays every
sequence in a file at once. RAGE: a clip is named and one plays. **Both agree, and neither has a
spelling for "all of them"** -- because glTF does not either: within one animation a target must
not be used twice, and two animations on one property is left undefined by the format.

`include/Scenario.h:101` offers `AssetAnimation { Play, Ignore, Driven }` and no way to say WHICH.
`src/engine/Asset.cpp:23` reads `Play` as every animation the file carries:

    std::vector<int> all((size_t)File_.Animations().size());
    for (size_t at = 0; at < all.size(); ++at) { all[at] = (int)at; }

For a file with one animation that is right by accident. For Khronos's own **Fox**, which carries
three -- Survey, Walk, Run, all driving the same nodes -- the poser refuses by name and the asset
cannot stand through the door at all:

    REFUSED Fox   animations 0 and 1 both drive the rotation of node 8,
                  and the format states no result for that

**The refusal is CORRECT and this tree already wrote down why.** `test/khronos/glTF/Fox/manifest.json`
states it: "glTF says only that within ONE animation a target must not be used twice, and leaves
two animations on one property undefined. A viewer plays one." The corpus case therefore declares
`animations: [0]` and is green. The door has no way to say that, so `apps/bench` cannot show a
valid Khronos asset that the corpus scores every run.

**AND THE CAPABILITY IS ALREADY IN THE TREE, UNREACHABLE.** `Gltf::Pose::Build` has taken a
`Span<const int>` of animations since it was written, and a single-animation overload beside it.
Only the door cannot reach either. That is CLAUDE.md's second question answering itself.

- [ ] a declared asset names which animation plays, and the engine's own default is the FIRST
- [ ] Fox stands through the door: `apps/bench --scene Fox` draws instead of refusing
- [ ] a named animation the file does not carry is refused, with the count it does carry

**The measurement that would show I am wrong:** if Fox still refuses, the cause is not the
selection. Negative control: point the selection at animation 3 of a file carrying three and the
refusal must name `3 of 3`.
