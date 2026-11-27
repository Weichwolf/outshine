# Established lighting and shadow presentation

Primary online references checked 2026-11-19; implementation: [#1965](https://github.com/Weichwolf/outshine/issues/1965).

| Reference | Named method / use | Boundary |
|---|---|---|
| [Frostbite PBR, revision 3](siggraph/2014-frostbite-pbr-course-notes.pdf) | Consistent material, light, exposure and camera response | Select useful existing components; historical buffers/pass budgets do not transfer automatically. |
| [Clustered Deferred and Forward Shading](hpg/2012-clustered-shading.pdf) | Spatial/depth clusters bound per-fragment local-light lists | Include list construction, overflow and memory in measurements. Far emission does not require a full local light per window. |
| Zhang, Sun, Nyman, GPU Gems 3 chapter 10: [Parallel-Split Shadow Maps](https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus) | Distance-dependent shadow sampling with split shadow maps | Splits/resolution/filtering are engineering choices; the old DirectX pass setup is not a backend requirement. |
| Epic: [Virtual Shadow Maps](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine) | Cache demanded regions and invalidate for changed light/casters | Use the caching principle only. Full VSM/Nanite is not mandated and Epic explicitly warns that VSM may not suit non-Nanite targets. |

Use established PBR, cascaded/clipmapped shadow sampling, PCF/contact techniques and dirty-region reuse as appropriate. There is no need for new lighting theory to remove unnecessary redraws or obtain a coherent game look.

Python and shared GLSL can prototype sampling, selection and invalidation against a full-update baseline. Useful comparisons include moving cameras and casters, newly visible areas, changing light, total render cost and memory. Use the visual and device results to choose an application approach.
