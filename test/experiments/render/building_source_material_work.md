# Reuse source material work without changing response measures

Owner #1167. The measured parent-material phase repeatedly evaluates identical
source-material states and dispatches small NumPy cross products per patch.
Improve this producer work with the same source selection, patch areas, material
palette, accumulation order and strict whole-source failure behaviour.

`source_measure` owns an ephemeral dictionary for one invocation/source. Its key
contains material category, stream, exact paint dtype/shape/bytes and every supplied
material keyword/value. The policy is fixed throughout that source invocation.
Only successful pure material responses are reused; no nearby-state quantization,
palette reduction, material averaging or additional cache lifetime is introduced.
Repeated calls, other sources and policy changes do not share dictionary contents.
Consumers read response arrays without modifying them.

The third `source_measure` result remains the actual material evaluation count:
now distinct evaluated states, rather than all repeated patch requests. All
existing producer counters labelled `material_evaluations` therefore count calls
that actually execute the material program. Patch count/geometry/source selection
are unchanged. No new native asset schema, cache or reader contract follows.

`patch_normal_areas` batches the two cross-product families per generated shape,
including the necessary axis-stack copies. Cross-product row arithmetic remains
the same; each row still uses the scalar norm arithmetic, preserving the measured
word identity instead of substituting a different batched reduction. Temporary
stacks end with the source measurement, not a persistent retained patch attribute.

`patch_measure` retains its default scalar calculation/material lookup for clipped
matte domains and as the independent comparison control. Source measurement passes
prepared local normals/area Jacobians and its local material lookup explicitly.
The final positive finite wall/roof checks remain unchanged. No tolerance is added.

Compare actual source measures in float64 before group packing, all tree/node
arrays by dtype/shape/bytes, unchanged native reference images, material-program
call counts and complete producer CPU/peak. Kernel speed alone is not the full
producer result. Cross-product rows stay the same even when NumPy dispatches fall;
reused material evaluations are a separate reduction in real program work.

Use the existing single-cell and regional producers with project `.venv`, one
BLAS/OMP thread and `PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`.
Fresh processes and fresh system-Temp outputs are required for independent peaks.
Before/after commits and the explicit original scalar source control belong in
the PR. Current controls are `test_building_source_material_work`; larger real
counterexamples and measured costs remain delivery evidence, not new test pins.
Python/reference costs do not admit native A18/Iris, GPU, world memory or p99.
