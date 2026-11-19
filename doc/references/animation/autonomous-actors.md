# Established navigation, steering and physical control

Reviewed 2026-11-19. Scope: the [actor delivery packages under #1978](https://github.com/Weichwolf/outshine/issues/1978).

| Primary source | Named method / use | Boundary |
|---|---|---|
| Reynolds, GDC 1999: [Steering Behaviors for Autonomous Characters](https://www.red3d.com/cwr/papers/1999/gdc99steer.pdf), [local PDF](gdc/1999-steering-autonomous-characters.pdf), [author HTML](https://www.red3d.com/cwr/steer/gdc99/) | Separate action selection, steering and locomotion; path following, arrival and local avoidance | Steering is locomotion-agnostic; it does not solve a walking gait, joint controller, aerodynamics or all conversational behavior. |
| [Recast/Detour](https://github.com/recastnavigation/recastnavigation), [Detour queries](https://recastnav.com/group__detour.html) | Tiled walkable navigation meshes, path corridors, filtering and local movement | Reference implementation; no automatic dependency addition. A 2D navmesh is not a complete road-traffic or flight-navigation model. |
| Macklin et al., MIG 2016: [XPBD](../physics/mig/2016-xpbd.pdf) | Compliance-aware constrained dynamics | Reference numerical method, not evidence that a chosen full vehicle/character solver is adequate. |
| Shoemake, SIGGRAPH 1985: [Quaternion Curves](siggraph/1985-rotation-quaternion-curves.pdf) | Rotation interpolation | Interpolation alone does not establish physically correct locomotion or contact. |
| [PhysX Vehicle mechanical model](https://nvidia-omniverse.github.io/PhysX/physx/5.4.2/docs/Vehicles.html#mechanical-model) | Sprung masses, suspension queries and tire forces | Select parameters by vehicle class and ground contact. Rail constraints need a separate model. |
| [JSBSim reference manual](https://jsbsim-team.github.io/jsbsim-reference-manual/) | Six-degree-of-freedom equations, frames, aerodynamic forces and flight control | Validate aircraft parameter ranges and takeoff or landing behavior in our world. |
| [Jolt Physics architecture](https://jrouwe.github.io/JoltPhysics/) | Rigid bodies, constraints and lifecycle for streamed collision objects | Compare integration and measured costs on the target profiles. |

- Existing methods cover navigation/control building blocks; select established locomotion and vehicle/flight dynamics when each category is activated.
- Bipeds, quadrupeds, ground vehicles and aircraft share intent/state/persistence boundaries but use appropriate physical controllers.
- The scenario's LLM game master directs NPC intent and story. Steering and locomotion turn validated intentions into world actions and report the outcome.
- Genuine generation research concerns plausible reproducible body/rig/equipment recipes from sparse world/context inputs; controller integration is not automatically new research.
- Every selected algorithm first runs as a Python reference; use shared GLSL in Python-driven GPU experiments where relevant. Require contact/stability checks and adequate total work before native implementation.
- Inspection: Reynolds PDF pages 2-3 (hierarchy/limits) opened; other sections read for path-following/avoidance. This is source selection, not an accepted Outshine control implementation.
