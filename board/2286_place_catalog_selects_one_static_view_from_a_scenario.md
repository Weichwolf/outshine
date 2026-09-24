Type: defect
State: active
Architecture: ready
Parent: 2092
Depends:
Priority: P0
Area: client, scenario, integration
Tags: places, camera, hockenheim, capture

# The place catalog accepts a scenario with one static view and other rigs

## Evidence and contract

`outshine-client shots --all` currently refuses the shipped
`Hockenheimring.scenario` before any shot because `ReadPlace` requires exactly
one view. The scenario is valid: `overview` is its fixed geodetic camera and
`lap` is its route rig. `run --view lap` must retain the full declaration.
The place catalog is a static-camera capture catalog, not a validator that
forbids other camera rigs in a general scenario.

## Ownership and implementation

`src/client/PlaceCamera.cpp` selects exactly one geodetic view from each
scenario. Other view modes do not count as static place cameras. If zero or
multiple geodetic views exist, refuse the ambiguous catalog entry with its
path; never choose by file order or an Hockenheim name. Validate the selected
camera with the existing finite pose/projection checks and reject sampled
height for a fixed shot. Store a copy of the declaration with only that view
in `Shots::Place`, preserving world, sources, routes and rendering. The source
scenario and `run` are unchanged. The client owns this selection; the engine
does not acquire a second scenario convention.

## Abnahme

- The shipped place catalog loads; Hockenheim resolves to `overview` and can
  be passed through `shots --stats` without a catalog failure.
- A two-geodetic-view scenario is rejected, even if one appears first. A
  route-only scenario is rejected. An invalid selected camera still fails.
- The original Hockenheim scenario keeps `overview` and `lap` for `run`.
- `make format`; focused place-catalog case; `make lint`.
