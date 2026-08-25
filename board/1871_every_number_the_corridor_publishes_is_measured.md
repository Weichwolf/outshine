Type: bug
State: active
Area: sim, actor/path
Tags: telemetry, measured

# A published number that cannot move is not telemetry

`Fitted` carries five fields that no line in `src/` writes since the alignment rewrite
(b2fbf22d): `size_t Corrected = 0;` (src/actor/path/Fit.h:36), `size_t Strained = 0;` (:37),
`double StrainedWorstM = 0.0;` (:38), `double WorstRadiusM = 0.0;` (:43),
`double WorstExpectedM = 0.0;` (:45).

`CorridorLay` publishes all five as measurements of the corridor it just laid:

- `say.Number("corners the fit had to correct by measuring them", (double)fitted.Corrected, "corners");` (src/sim/CorridorLay.cpp:107)
- `say.Number("the radius it settled on", fitted.WorstRadiusM, "m");` (:116)
- `say.Number("the station the fit expected it at", fitted.WorstExpectedM, "m");` (:117)
- `say.Number("corners the data cannot support at any drivable radius", (double)fitted.Strained,` (:119)
- `say.Number("how far the worst of those leaves its vertex", fitted.StrainedWorstM, "m");` (:121)

Every one of them reports 0 on every route, for every input, forever. A reader sees "corners the
data cannot support: 0" and reads it as a result. `test/unit/actor/path/ACurveIsFittedAtTheRadiusItHas.cpp:71,73`
notes two of them, so the case prints a number that is structurally constant.

A guard that stops guarding goes GREEN, not red — which is why this is filed rather than
noticed.

## What will be true

- [ ] Every field of `Fitted` is written by the fit that produces it, or it is deleted along
      with the line that publishes it.
- [ ] Negative control: force a strained corner -> `Strained` is non-zero and its unit twin sees
      it; the same input against today's tree reads 0.
