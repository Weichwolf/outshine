Type: bug
State: open
Parent: 1882
Area: core, host, scene, data
Tags: guards, measured

# Four headers guard under a name that is not their folder, and one is a name that moved

`harness/claims/EveryGuardSpellsItsFolder` walked 239 headers at a3ebe3e0 and named four:

```
src/host/Fetching.h   guards as CURLTRANSPORT_H      where its folder spells OUTSHINE_HOST_FETCHING_H
src/scene/Traits.h    guards as OUTSHINE_TRAITS_H    where its folder spells OUTSHINE_SCENE_TRAITS_H
src/scene/Assembled.h guards as OUTSHINE_ASSEMBLED_H where its folder spells OUTSHINE_SCENE_ASSEMBLED_H
src/world/data/Transport.h  guards as OUTSHINE_TRANSPORT_H where its folder spells OUTSHINE_DATA_TRANSPORT_H
```

`src/host/Fetching.h` is the sharp one: `CURLTRANSPORT_H` is the guard of a file that no longer
exists under that name, carried through the move at 4d4981ec. A second header called
`CurlTransport.h` anywhere in the tree would silently empty one of the two translation units,
and nothing would say so — the failure mode this rule was written for.

## What will be true

- [ ] All four spell `OUTSHINE_<FOLDERS>_<NAME>_H` and the claim is green.
- [ ] Negative control: rename one guard back -> the claim names exactly that header.
