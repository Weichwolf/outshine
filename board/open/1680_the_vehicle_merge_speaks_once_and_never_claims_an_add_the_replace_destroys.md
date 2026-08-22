Type: bug
Area: scenario
Tags: layer merge, trace, dead code, 1676 follow-up

# The vehicle merge speaks once and never claims an add the replace destroys

1676 decided the vehicle element is a SINGLETON: a layer that declares one replaces the
base's, whole (src/scenario/ScenarioLayer.cpp:148-151). But the row-wise merge for Vehicles
was left standing: ScenarioLayer.cpp:111 `MergeRows(into.Vehicles, ..., ByVehicleName{})`
runs FIRST, does work the replace then discards, and writes a trace line the replace
falsifies.

Proven:

    base:  <vehicle name="A"/>
    layer: <vehicle name="B"/>
    trace: "layer 'swap' added vehicle 'B'"    <- A is about to be destroyed, nothing was added-beside
           "layer 'swap' replaced the vehicle"

The base's A vanishes while the first line implies coexistence — a declaration nobody can
trace is a declaration nobody can debug, and this trace actively misleads. ByVehicleName
(ScenarioLayer.cpp:45-48) exists only to feed the dead call.

Demanded: drop the Vehicles row from the MergeRows table and ByVehicleName with it; the
singleton replace at 148-151 is the one truth and its trace the one line. Unit case: a
base vehicle replaced by a layer's produces exactly one vehicle trace entry.
