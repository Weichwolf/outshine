Type: bug
Area: scenario
Tags: layer merge, identity, render plan

# A render output or stage merges by its name, never twice

The non-resetting door appends `<render>` children onto the standing lists
(src/scenario/ScenarioRead.cpp:194-199: `Outputs.push_back` / `Stages.push_back` with no
identity check), and ApplyLayer carries the whole Render section back
(src/scenario/ScenarioLayer.cpp:179). A layer that re-declares an output or stage therefore
DUPLICATES the row instead of replacing it.

Proven:

    base:  <render widthPx="1920"><stage name="sky"/><output name="colour"/></render>
    layer: <render fps="30"><stage name="sky"/><output name="colour"/></render>
    after ApplyLayer: Stages.size()==2, Outputs.size()==2 — sky twice, colour twice

`name` is the grammar's Required attribute on both elements (ScenarioRead.cpp:34-35), i.e.
exactly the identity 1655/1676 adjudicated for every other row — MergeRows replaces by it
everywhere else. Named rows inside a singleton section must follow the same rule: same name
replaces, new name adds, and the trace speaks. A layer also cannot REMOVE a stage today;
if removal stays unspellable, that is a decision to write down, not an accident.

Demanded: identity-aware merge for Outputs/Stages in the door (or in ApplyLayer's section
carry), plus a unit case in test/unit/scenario/ALayerOverridesAnEarlierOneById.cpp proving
no duplicate survives.

---

Closed: a render section that redeclares its outputs or stages REPLACES the list -- the
declared list IS the list (an ordered plan has no meaningful per-name merge), and the sky
cannot be drawn twice by a mod that names it once. Proven beside the drive keep.
