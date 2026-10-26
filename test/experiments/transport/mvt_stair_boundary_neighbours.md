# Observed neighbours of the local stair plan

The three original parts in `mvt_stair_t_junction.json` are not a closed repair
region. Their four outer path endpoints include three observed contacts:

| Local part / end | Observed neighbour | Required interpretation |
|---|---|---|
| 431 / first | 535 / last | Shared endpoint; no prepared junction is not a free end |
| 431 / last | 537 / first | Shared endpoint; the other source continues |
| 432 / first | 535 / interior | Existing three-leg junction outside the small stair crop |
| 432 / last | No match in this retained subset | Unknown, not a certified free end |

The new fixture retains seven original parts: the three parents, paths 535/537,
and their observed neighbours 239/402. It stores provider feature IDs, tiles,
all supplied tags, original axes, generated height priors, derived native nodes,
complete recorded profile spans and prepared edge/junction products. Generated
heights are observations, not automatically fixed constraints. Native indices
identify this frozen capture; they are not algorithm rules or provider node IDs.

```sh
python3 test/experiments/transport/mvt_stair_boundary_neighbours.py \
  --output "${TMPDIR:-/tmp}/outshine-stair-neighbours/contacts.json"
```

The probe verifies exact shared original coordinates and native node identities.
It does not establish missing topology from proximity, nor turn an absent local
match into a free end. This is an input inventory, not a solver or geometry fix.
Before native replacement, preserve an actual unchanged outer interface or
jointly reconstruct its adjoining region. Mesh, navigation and terrain must
consume the same decision. The existing conditional contact solver in #1008 is
a possible consumer; synthetic self-binding is not neighbour admission.

## Capture and limits

Native commit `8e12cbb2f16c314506af92a95d7eb560c05f1376`, ground-region recipe 91.
A private diagnostic copy of `Corridors.cpp` emits observed source/profile data;
the product archive, equations and normal rendering remain unchanged. A new
private asset database uses existing local raw-source hits. The complete offline
1280x720/60-frame/360-degree process terminates with status 0. Its final PNG is
byte-identical to the ordinary current world; both hashes are in the fixture.
The original three source axes and sampled design priors match the parent
fixture exactly, establishing the same local frame. Seven source parts are a
retained subset, not a complete network or provider completeness claim.

The diagnostic took 86 seconds including uncached asset creation, validation,
rendering and JSON emission. It is not a solver, cache-hit or frame-cost result.
Normal before/after costs remain in #1007; this capture changes no runtime assets.
The instrumented CPP hash and original source/input provenance are retained.
Private commands, binary, full log and JSON records live in the system temp
directory recorded by `/tmp/outshine-motion-integration-844/stair-neighbours-task.txt`.
Only the reusable input and probe are versioned. Ownership and next work: #1003.
