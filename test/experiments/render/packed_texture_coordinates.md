# Packed texture-coordinate stage interface

The shared subject interface carries both texture-coordinate pairs in one
Float32 `vec4` at location 0. Its XY components hold the first pair and ZW hold
the second. Location 1 is unused. This replaces two half-filled export slots
without quantization, normalization or arithmetic on the coordinates.

`writeSubjectVaryings()` packs the vertex values after their existing assignment;
`readSubjectVaryings()` restores the two local `vec2` values before material work.
The lit vertex stage, lit fragment stage, ground lattice vertex stage and ground
fragment stage share those helpers. Generic ground rendering also consumes the
lit vertex producer. Both terrain paths therefore use the same interface.
Constant-coordinate permutations retain their existing zero values and omit
this varying. Flat and shadow stages have their own unchanged interfaces.

Vertex attributes, uniform/storage layouts, descriptor bindings, all other
varying locations, perspective interpolation, material calculations and clip
fields are unchanged. A matching shader package must update producers and
consumers together on both Vulkan and Metal. Packages must not mix the old
location-0 `vec2` consumer with the new `vec4` producer.

The change removes one exported slot wherever both pairs were present. Actual
compiler messages, URB allocation and device times must be measured rather than
inferred from GLSL component counts. No lower precision, depth/culling change or
reduced scene content is part of this interface.

## Correctness and remaining scope

[#900](https://github.com/Weichwolf/outshine/issues/900) owns the native Iris Xe
interpolation defect and current device/world evidence. The two fixed probes in
[#907](https://github.com/Weichwolf/outshine/pull/907) retain independent Float64
motion references and their original bounds. They distinguish an erroneous
native result from a passing diagnostic. Passing them is not full scene or
frame-budget admission.

Validation also includes the complete shader package and resource reflection,
matching lit/terrain producers and consumers, existing GPU material/velocity
cases, and unchanged native world inputs. HDR, depth, velocity, image stability
and costs need separate checks. Old failing captures and shader packages remain
available for negative controls. No test updates an expected image or relaxes a
tolerance to accept this packing.
