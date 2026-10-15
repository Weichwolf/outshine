# Direct cloud shadows in clear-air scattering

Issue: #320; goal: #190. This reference isolates the direct air-source error in
the mixed medium from #193. It does not solve cloud multiple scattering or its
effect on the diffuse/Psi input, and does not claim runtime or Place acceptance.

At a camera-ray point `x(s)`, the direct air source contains the clear solar
transfer and the additional cloud solar transfer. The diffuse source is a
separate prescribed input. Integrate their sum through the total camera-path
extinction:

```text
j_air(s) = j_direct_clear(s) * T_cloud_to_sun(x(s)) + j_diffuse(s)
L(q) = integral[0,q] T_eye_to_s(s) * j_air(s) ds
```

Cloud extinction on the camera path belongs in `T_eye_to_s`. Solar shadowing
belongs at the source point, before fitting or integrating that source. A cloud
outside the camera ray can still block the source's sunlight. Multiplying all
source emission by the shadow factor dims the prescribed diffuse contribution;
multiplying the resulting eye radiance cannot account for varying solar paths.

The independent plane-parallel control uses homogeneous air between 0 and 10 km
and a purely absorbing cloud slab between 1 and 2 km. Air scattering coefficients
and the prescribed diffuse input are declared in `air_cloud_source_reference.py`.
The cloud extinction is 1.5 km⁻¹. Distances in that reference are kilometres;
the future renderer/provider contract uses metres at its outer boundary.

Inside each interval bounded by cloud/air crossings, every optical depth is
affine in camera distance. The reference integrates each exponential exactly
using `expm1` and a stable zero-slope limit. An independent Gauss quadrature of
the physical integrand is refined separately. Sun directions below the horizon
remove the direct term; a prescribed diffuse input can remain. The unit tests
cover eyes below, inside and above the cloud, upward/downward/horizontal rays,
positive and negative solar cosines, repeated query endpoints, zero distance,
null cloud and affine-transfer composition.

In the default upward case, leaving the direct source unshadowed produces
199.80% maximum RGB error at 12 km. Dimming the entire source produces 41.05%;
dimming the eye radiance afterwards produces 58.27%. Null-cloud alternatives
are identical. These are controlled counterexamples, not estimates of an
unrendered Place's error. The report also includes a cloud above a horizontal
camera ray, an eye within the cloud and a downward view from above it.

Run from the repository root with Python 3 and NumPy. PDF plotting additionally
uses Matplotlib; private outputs must be outside the checkout:

```sh
python3 -m unittest discover -s test/experiments/render -p test_air_cloud_source_reference.py
python3 test/experiments/render/air_cloud_source_reference.py --output /tmp/outshine-light-source/reference.json
python3 test/experiments/render/plot_air_cloud_source_reference.py --output /tmp/outshine-light-source/comparison.pdf
```

The next native step consumes the actual metric source point in Weather's
density coordinate frame and applies its existing solar transfer to Direct
before the fixed physical-ray partition's source fit. The clear prefix, its
query invariance and shared bindings must remain verified. The concrete provider
and coordinate-anchor agreement live in #320; this reference introduces no
public shader/provider API. Native execution, work counts and full-resolution
images remain separate acceptance evidence there.

Research basis: [Hillaire, *A Scalable and Production Ready Sky and Atmosphere
Rendering Technique*, EGSR 2020](../../../doc/references/atmosphere/egsr/2020-production-ready-atmosphere.pdf), section 7,
figure 14 and page 9 (local PDF inspected visually). Its shadowed atmosphere
uses source-point shadow information while ray marching. This experiment does
not inherit an unmeasured temporal reconstruction or quality claim.
