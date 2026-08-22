Type: bug
Area: sim

**Lay refuses a declaration of two vehicles the way it refuses none**

src/sim/Journey.cpp:233-235:

    say.Claim(declared.Vehicles.size() == 1, "declaring one vehicle");
    if (declared.Vehicles.empty()) { return false; }

The claim asserts exactly one vehicle; the guard only refuses zero. A scenario declaring two
vehicles logs a failed claim into the Sink and then Lay RETURNS TRUE and rides `Vehicles[0]` —
a library consumer that does not audit the sink drives on a declaration the engine itself
called wrong. Claim and refusal must agree: `size() != 1` returns false, with the count in the
refusal text.

Residue of the same hour: `src/scenario/ScenarioRead.h:1-2` still guards itself as
`OUTSHINE_CLIENTS_SCENARIOREAD_H` after the move out of src/clients (board:1598) — the header
names a layer it no longer lives in.
