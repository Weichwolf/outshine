# Longitudinal finite strips

Owner: #1955 / #915. Decision experiment; no native integration or runtime policy.
Input: unchanged [native road parts](native_road_parts.md), archive SHA
`5aafb7b17b2820135b45ed26ca57a7e48d575e84ee6262ead81fc438713810fb`.

Each recognized surface has four vertices per longitudinal ring and six triangles
per span. Retain selected whole source rings and all piece endpoints. Before mesh
emission, a native implementation could choose stations by the same construction;
the fine meshes in this experiment are only the fixed offline opponent.

Both surfaces map one strip parameter domain. Their difference is affine on the
overlay of the source/candidate triangulations. Its norm reaches its maximum at
overlay vertices: source vertices plus intersections of the candidate diagonal with
source rows and diagonals. Evaluate those directly, without constructing a fine VBO
or searching triangle pairs. The correspondence bounds symmetric finite-mesh distance.
Float64 evaluation is a probe, not an outward-rounded native guarantee. It bounds
neither continuous CAD, normal/material interpolation, terrain contacts nor shadows.

Balanced subdivision accepts a whole run or splits its parameter sequence in half.
Worst work is O(N log N), accepted straight runs O(N), pending stack O(log N).
Zero-length centre steps use an index parameter domain; unsafe normalized intervals
retain their original spans. Original final Float32 contacts remain exact.

Complete fixed corpus: 321,472 triangles; 5,579 recognized surface pieces. Other
geometry stays unchanged in every total, including closed bodies and junctions.

| Diagnostic finite position budget | Total candidate triangles |
|---|---:|
| 0.01 m |293,212|
| 0.05 m |265,198|
| 0.25 m |234,166|
| 1 m |218,074|
| 4 m |212,446|

Even a six-triangle span per surface piece leaves 210,568 triangles:
`(321472 - 210568) / 321472 = 34.50%` maximum reduction **under these fixed boundaries**.
This is a conditional count floor, not an optimal renderer or permission to use 4 m.
The five NumPy comparisons took about 1.7 s together on A18 Pro. This is not a native
generator/device benchmark. Independent triangle-distance sample tests support the
correspondence and exercise curved/warped/nonuniform and degenerate inputs.

Decision: do not port another isolated strip simplifier to chase the required large
World gain. Far representations must aggregate whole net sections, including their
junction/body contribution, instead of retaining every feature boundary. Reuse the
shared spatial product/coverage/residency and material/depth-field path where suitable;
precise local functional products remain separate. Pose, passes, incomplete children,
terrain contact and native image/cost qualification remain part of that work.

```sh
python -m unittest discover -s test/experiments/transport -p test_longitudinal_strip.py
python test/experiments/transport/compare_longitudinal_strip.py --output /tmp/outshine-longitudinal.json
```

Requires NumPy and Python 3.10+ through the fixed-corpus loader.
