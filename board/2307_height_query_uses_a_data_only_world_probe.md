Type: defect
State: open
Architecture: ready
Parent: 2131
Depends:
Priority: P1
Area: include, world, client
Tags: public-api, terrain, headless, camera

# Height query uses a data-only world probe

## Reproduced defect

`outshine-client height 49.35733376 8.58581543` exits 1 with “ground detail
needs a finite camera eye, positive lens and frame height”. The command
declares Earth without a view, then calls `Engine::assemble`/`preload`.
WI 2305 correctly rejects viewless render preparation. The client command
is a numeric terrain query, not a render, and must not invent a camera to
pass that contract. `Engine::sampleHeight` is correctly resident-only.

## Contract and ownership

- Introduce a public, data-only height probe that owns a declared source set,
  bounded tile requests and decoded terrain needed for one coordinate. Share
  source selection, cache identity, DEM decode and interpolation with the
  world; no second height convention or independent rasterizer. It does not
  create SDL video, a renderer, a camera or a scene.
- Longitude/latitude are degrees, output metres above the same datum as
  `Engine::sampleHeight`. Reject nonfinite/out-of-coverage coordinates before
  IO. Errors distinguish undeclared source, absent data, network/refusal,
  timeout and malformed DEM. Offline uses only cache and shipped data.
- The client `height` verb uses the public probe. Keep its two-coordinate
  syntax; add optional offline/cache options only if they follow existing
  client conventions. A query cannot mutate an active Engine scene.

## Acceptance

- The public command returns a finite height for a pinned local DEM fixture
  without SDL video initialization or camera; compare with the world sampler
  on the same decoded tile to numeric tolerance. A cold online source may
  fetch under a finite deadline; warm/offline repeats without network.
- Invalid coordinate, missing cache, 404, 403 and malformed DEM each return
  the correct explicit error, never a made-up zero. The WI 2305 viewless
  Earth render rejection remains unchanged.
- Focused probe/client tests, `make format` and `LINT_JOBS=2 make lint` pass.
