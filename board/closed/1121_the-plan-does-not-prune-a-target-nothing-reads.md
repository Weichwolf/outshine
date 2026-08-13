Type: bug
Area: render
Tags: perf, instrument
Depends: 1123

**The plan does not prune a contribution whose target nothing reads**

`CLAUDE.md` cites Frostbite's FrameGraph for *lifetime, transitions and allocation falling out of the
graph rather than being hand-ordered*, and the plan is compiled backwards from a requested output. It
does not have that property.

Measured: two plans compiled and compared, one where a temporal stage reads velocity and one where
nothing reads it. **Identical** — `passes=2`, `sceneVelocity held=YES`, `attached-to-passes=1` in both.
`Pull` calls `Want()` on every `Contributes` target of any held stage, so a contribution makes its target
held by construction, and the attachment loop adds it with no reachability test.

Consequence today: **`render/Parity`'s own 2-pass plan allocates, clears and writes a full-screen
`Rg16Float` velocity target every frame that nothing reads.**

Consequence tomorrow: any diagnostic attachment costs bandwidth in every plan that draws geometry, which
is why the shading-normal readback is blocked rather than built.

**Done when** a target no held stage reads is neither allocated nor attached, the two compiled plans
above differ, and no case moves in the picture bound.
