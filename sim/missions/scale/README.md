# `missions/scale/` — the actor-scaling measurement casts

GENERATED, not authored. Every file here is written by `sim/tools/fb_scale_bench.py` and is reproducible
byte for byte:

```
python3 tools/fb_scale_bench.py gen  PROFILE N [--spacing DEG]
python3 tools/fb_scale_bench.py sweep PROFILE N,N,… --threads 1,2,4 --reps 3
```

They live in their own directory so they do **not** match the regression glob `missions/*.fbm`: a
measurement cast has no reading rule, no verdict worth quoting and no place in the byte-identity gate.
Every run here ends at exit 3 (TIMEOUT) after exactly its declared `timeout`, which is the point — a
constant duration is what makes wall clocks comparable.

The profiles, the numbers they produced and what they mean: [`doc/actor-scale.md`](../../../doc/actor-scale.md).

| profile | cast |
|---|---|
| `blind` / `quiet` / `fcr` / `fcrdl` / `sensors` | N F-16, a chain of single additions: no walker, + eye, + FCR, + datalink, + RWR |
| `combat` | N/2 F-16 vs N/2 MiG-29, armed, `task intercept`, kill + survive |
| `ground` / `missile` | N air-defence positions / N AIM-120 in the air |
| `bare_target` / `bare_mover` | N inert ground targets / N kinematic movers, with nothing that looks at them |
| `mix_none` | the base load alone: 24 modules (12 F-16 + 12 MiG-29), every box on |
| `mix_target` / `mix_site` / `mix_missile` / `mix_mover` | that base load + N of one cheap kind |
| `mix_theatre` | that base load + N cheap actors, 40 % positions / 40 % targets / 20 % movers |
