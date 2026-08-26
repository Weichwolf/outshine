Type: bug
State: open
Area: render
Tags: shadow, plan, residency

# A plan that drops the shadow stage clears the atlas rather than leaving the last one standing

`DeclarePlan` puts `Stage::LightVisibility` in the plan only when the declaration casts
shadows. When it does not, no pass is begun, so `ShadowAtlas_` keeps whatever the previous
declaration wrote. The next subject that reads it reads a caster that is no longer there.

Measured while closing board:1921: a scenario declared with one caster wrote 779 086 texels
above the clear; re-declared with no asset at all, the same read reported 779 086 again. The
light stage never ran, and nothing cleared what it had left.

The frame-graph question underneath: a resource a plan stops writing is either cleared or
declared stale, and today it is neither.

Proving case: an engine that renders with a caster and then re-renders a declaration carrying
none reads an atlas with no texel above the clear. Negative control: the plan as it stands, and
the second read repeats the first read's count exactly.
