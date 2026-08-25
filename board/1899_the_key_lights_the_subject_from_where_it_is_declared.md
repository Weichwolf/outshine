Type: bug
State: open
Area: render, clients
Tags: measured, lighting
Regresses: 1893

# The key lights the subject from where it is declared

Measured 2026-08-25 through the front door, one triangle with declared NORMALs, 64x64, captured
with `Engine::Capture` and compared byte for byte:

| what was declared | PNG |
|---|---|
| no subject at all | 96 bytes |
| key 40000 lux, environment 0.20, elevation +42 deg | 233 bytes |
| key 80000 lux, environment 0.40, elevation +42 deg | 233 bytes |
| key 40000 lux, environment 5.00 -- twenty-five fold | 233 bytes |
| **key 40000 lux, elevation MINUS 80 deg** -- below the horizon | **233 bytes** |
| no `<lighting>` declared at all | 233 bytes |
| key 400 lux, environment 0.20 | 234 bytes |

So the subject IS drawn -- 96 against 233 proves it -- and the exposure the key compiles to DOES
reach the frame, because a hundredfold drop in illuminance moves the pixels. What does not reach
it is the key's DIRECTION: a light 80 degrees below the horizon makes the same picture as one 42
degrees above it, and so does declaring no light at all.

A subject lit by something with no direction has no surface that can turn toward or away from
the light. That is the same area board:1893 measured from the other side -- luminance 12.8 to
45.3 with course alone, which said key-up and body-up are not the same axis -- and this is the
simpler statement of it: through `Engine::Declare` the key has no axis at all.

The exposure arithmetic, for what it is worth, is sound and the case proves it: Live compiles
`Exposure = 2.5 / (1.2 * KeyLux)` and passes `Environment` through as an absolute radiance, so
scaling key and environment TOGETHER is exactly invariant -- 40000/0.20 and 80000/0.40 are byte
identical. That also says the declaration carries one truth twice: the picture depends on the
ratio and nothing else.

Proving test: `harness/outshine/door/ScoreWhatTheKeyLuxDoes`, standing RED and declared in
`EXPECT_FAIL` with its count. Negative control: the same case's invariance CHECK, which passes,
and its hundredfold-drop control, which passes -- so the case can tell a real invariance from a
renderer that ignores lighting altogether, and it is the DIRECTION check alone that is red.
