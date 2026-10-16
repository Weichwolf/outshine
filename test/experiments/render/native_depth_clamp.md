# Native depth clipping and clamping

Contract owner: [#424](https://github.com/Weichwolf/outshine/issues/424).

`GraphicsDescription.ClampDepth=false` keeps the native clip default.
`Capabilities.DepthClamp` reports an available, enabled device feature, independently
of the selected pipeline mode. Requesting an unavailable mode returns `Unsupported`.
Vulkan enables the supported core feature and selects `depthClampEnable` per pipeline.
Metal advertises the core capability, retains the requested mode in its immutable pipeline
state, and sets `MTLDepthClipModeClip` or `MTLDepthClipModeClamp` on every raster draw.
The Command Line Tools Metal SDK declares `setDepthClipMode:` since macOS 10.11 and
iOS 11; the current Metal backend requires newer SDK/runtime support.

The [rasterization specification](https://docs.vulkan.org/spec/latest/chapters/primsrast.html)
defines the feature requirement and the removal of Z clip planes. The
[fragment specification](https://docs.vulkan.org/spec/latest/chapters/fragops.html)
places the clamp after interpolation, within the viewport depth range. No additional
extension, binding, virtual method, or generated material data is involved.

## Independent GPU regression

`test/harness/render/NativeDepthClampProbe.cpp` consumes the two accompanying
`native_depth_clamp` shaders through the production archive. It draws a fixed triangle
into 64×64 RGBA8 and D32 attachments. Source depth parameters are constant −0.25,
0.25, 0.75, 1.25, or a plane spanning −0.25/0.5/1.25 at its three vertices.
Both forward and reversed projection (`1-z`) are checked. The reversed case also uses
homogeneous W=3; normalized coverage must remain the same.

The CPU expectation derives barycentric coordinates from pixel centres and evaluates
the complete plane independently. Clip removes fragments outside [0,1]; clamp retains
the triangle and clamps the interpolated depth. This distinguishes fragment clamping
from changing vertex depths. A 0.5 depth clear and disabled depth comparison make
writes of both 0 and 1 observable. RGBA coverage is exact; D32 tolerance is 2×10⁻⁶.
Only pixel centres on raster or depth boundaries within 10⁻⁵ are excluded, while a
coverage-count check prevents an empty comparison. Interior controls and the existing
raster/culling probes remain part of the backend suite.

Linux additionally exercises the actual Vulkan validation routine with a disabled
feature context. On an unsupported physical backend the public request must fail;
passing its clip checks does not prove clamp rendering there.

After building the production archive, run:

```sh
python3 test/experiments/render/native_vulkan.py --validation --present \
  --output /tmp/outshine-depth-clamp-check
```

The script records commands, archive/source hashes, capability and terminal results.
It saves optional raw diagnostics under `depth-clamp/`, without changing reference
pins. The Metal bootstrap runs the same probe against its matching production archive and
headers through `native_metal.py --output <system-temp-directory>`. Normal/reverse
projection and complete colour/D32 expectations are shared across both backends.
An intentionally advertised but unwired Metal encoder is the negative control: the
clamp cases must fail, while the clip controls remain exact. A false capability with
unsupported requests is not an implementation proof.

Explicit selection in the native frame-pipeline port and the public offscreen-caster
regression remain separate integration requirements. A working API alone does not
change an existing pipeline's selected mode or prove Place/frame-budget acceptance.
