Type: bug
Area: clients
Tags: refusal at assembly, 1678 follow-up

# A drive whose ends coincide refuses at assembly by name

1678 fixed the KEEP case: a zoom-only layer over a declared route keeps the route. The ADD
case still sails silently: a layer (or a base) declaring `<drive zoom="15"/>` with no route
anywhere sets Driven.Declared with the struct's zeros — from (0,0) to (0,0) — because the
grammar (ScenarioRead.cpp:96) requires no coordinate and the reader's keep-semantics keeps
defaults when there is no base to keep from. Assembly.cpp:238-252 then seats the drive,
checking only that a mind stands; the zero-length voyage to the Gulf of Guinea is assembled
without a word.

Required attributes cannot carry this (the zoom-only DELTA is 1678's decided right). The
refusal belongs at assembly, the house's chosen gate: a drive whose two ends coincide
routes nothing — refuse by name ("the drive's ends coincide at (0.0000, 0.0000) -- a route
needs two places") at Assemble, beside the existing "no mind stands to take it".

Proof: unit test — base without a drive, layer `<drive zoom="15"/>`, Assemble refuses with
the coordinate in the message; a declared identical from/to pair refuses the same way.
