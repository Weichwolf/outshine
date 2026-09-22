Type: proof
State: ready
Architecture: ready
Parent: 2175
Depends: 2260
Priority: P1
Area: simulation, physics, navigation, scenario
Tags: driving, contacts, vehicle, acceptance

# A simulated vehicle completes the Hockenheimring lap

## Contract

Replace the kinematic camera carrier of WI 2260 with an actual vehicle using
the same logical route IDs and certified 3D alignment. The scenario follows
the simulated body through the existing first-/third-person camera modes.
Vehicle dynamics/contact are a separate claim from camera motion. Current
scenario `Slip` parameters explicitly have no runtime tyre-force consumer;
do not present the old `f31.scenario` as a working autopilot proof.
Apply WI 2259's route/contact proof obligations to this vehicle class; the
entire all-feature proof inventory need not close before this focused lap.

Build a bounded controller for a declared vehicle class: target speed from
curvature and stopping distance, steering from lookahead/pose error, throttle
and braking from speed error. Feed wheel/contact queries from the engine's
collision product, not render triangles. Preserve deterministic fixed-tick
integration and interpolate the camera. Keep route topology independent of
mesh LOD and allow the same track to be traversed after renderer eviction.

## Acceptance

- The vehicle completes the entire route without teleporting, penetrating
  ground/bridge, leaving the drivable surface, wrong-level shortcuts or
  losing contact at tile/edge seams. Check suspension travel, wheel heights,
  steering, lateral acceleration, braking and clearance against the declared
  vehicle limits at every tick; a failed constraint names edge and station.
- Intentional missing contact, broken seam, closed underpass and impossible
  curve-speed variants fail. Repeated runs with the same source revision and
  seed reproduce a bounded trajectory; different render LOD does not alter
  the logical route.
- Open continuous first-/third-person captures and sample PNGs. Measure
  p50/p95/p99 CPU/GPU frame time, physics tick cost and memory during motion.
  A plausible image alone is insufficient; a successful lap is the functional
  acceptance. No structural load certification is implied.
