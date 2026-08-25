Type: bug
State: open
Area: door, scenario
Tags: declarative

# Render.Declared reaches the door, or it goes

`grep -c 'Render.Declared' src/clients/Engine.cpp` is 0. The render declaration flag is written
by the parser and read by the layer merge and reaches the door not at all, so a scenario that
declares no `<render>` is handed a frame, a fill, a picture region and an fps out of whatever
sits in the struct.

`Lit.Declared` had the same defect and is fixed: `Engine::Declare` gates the lighting on it, a
scenario that declares none leaves the engine's own default standing, and
`harness/outshine/door/ScoreWhatTheKeyLuxDoes` proves it with the picture as the instrument.
Render is the same shape and is not done, because its default is not obvious the way an unlit
scene is: an undeclared render still needs a frame, and the frame the CLIENT handed in through
`DrawsInto` is the candidate.

Proving test when it lands: a scenario with `Render.Declared` false stands at the canvas the
client declared and not at the struct's zeroes. Negative control: the same case against a door
that copies the render section unconditionally.
