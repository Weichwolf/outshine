Type: bug
Area: scenario

**A drag coefficient is named as one, and multiplied by the area it belongs to**

`<body dragArea="0.66" frontalM2="2.19">` declared a drag COEFFICIENT under the name of an area, with
the area it needs to be multiplied by sitting unused beside it. Everything downstream took 0.66 m2 as
the whole of `0.5 rho A v^2`.

**The F31 did 344 km/h.** With the coefficient multiplied by its 2.19 m2 frontal area it does 232.7,
which is what the car does.

Now `dragCoefficient` and `frontalM2` are declared as the two measurable quantities they are, and
whoever needs the area multiplies them. Neither is stored a second time as a product: a derived value
kept beside its inputs is the same statement in two places, and the two drift.

Proven by `TheDriversScenarioLoadsAndItsNumbersArePhysical.cpp`, which now derives the top speed from
the declaration and checks it lands where an F31's does, and by
`ARigCarriesItsBodyOnEveryMountItHas.cpp`, where the rig neither gains nor loses speed at exactly
`sqrt(F / (rho A / 2))` = 64.645 m/s.

## Comments

**A name that lies about a unit passes every other check.** The declaration parsed, the grammar
accepted it, the speed profile computed a top speed, the physics balanced against it, and the two
agreed with each other -- because both were wrong in the same way. What caught it was asking whether
the NUMBER was one a real car produces, which no invariant can ask and only a person can.

That is a small live example of `board:1518`: mathematics proves the geometry well formed and the
halves consistent; a person decides whether the model of the world was right.
