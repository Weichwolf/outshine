# Established image-based distance rendering

Reviewed 2026-11-19. Scope: presentation of already generated assets; not new world-generation research.

| Primary reference | Named method / useful component | Transfer boundary |
|---|---|---|
| Shade, Lischinski, Salesin, DeRose, Snyder, SIGGRAPH 1996: [Hierarchical Image Caching](https://grail.cs.washington.edu/projects/hic/walkthru/www/Shade96.pdf), [local PDF](siggraph/1996-hierarchical-image-caching.pdf), [author project](https://pages.cs.huji.ac.il/danix-lab/cglab/research/wa/) | Hierarchical cached cluster images; angular validity; amortized capture-versus-draw cost; reuse through coherent movement | Static, prepartitioned geometry and historical hardware. Do not copy its BSP layout, fixed threshold or speedup as an Outshine result. |
| Shade et al., SIGGRAPH 1998: [Layered Depth Images](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/Shade-SG98.pdf), [local PDF](../vegetation/siggraph/1998-layered-depth-images.pdf) | Multiple depth samples for image reprojection and newly exposed surfaces | Layer storage, coverage, holes and disocclusion still require engineering; not a guarantee for unrestricted translation. |
| Epic: [Impostor Baker](https://dev.epicgames.com/documentation/en-us/unreal-engine/impostor-baker-plugin-in-unreal-engine) | Full-sphere/hemisphere view capture; nearest-view interpolation; baked material channels | Production example, not an atlas size or mobile-cost mandate. Choose group size/views from actual image and update cost. |
| Risser, GPU Gems 3, chapter 21: [True Impostors](https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-21-true-impostors) | Layered depth represented on quads; view-ray intersection | Optional comparison for depth complexity. Fragment/ray-march/overdraw cost can outweigh geometry savings; not the default for every tree or far pixel. |
| Decoret et al., SIGGRAPH 2003: [Billboard Clouds](../vegetation/siggraph/2003-billboard-clouds.pdf) | A small set of image planes approximates complex groups | Plane count, alpha coverage and lighting must be checked; no general zero-cost representation. |

## Application

- Near meshes, grouped impostors and far panoramas are established representation choices.
- Hierarchical image caches, depth reprojection and cubemap layouts are candidates for the far view.
- Keep material/depth and illumination conventions consistent. Static captures do not freeze all future lighting, wind or world changes.
- A view cache can be pose-dependent and bounded; durable recipe/LOD assets remain reusable. Keep those lifetimes separate.
- Test standing, rotation, translation, changing altitude, occlusion/reveal and refresh transitions. Include capture/update cost and all live buffers in comparisons.
- A simple reference and paired images help compare alternatives. Python and shared GLSL can support those experiments.
- Open engineering: layout, filtering, update scheduling, accepted softness and device tuning. New foundational rendering research is not a prerequisite.

## Inspection

- 1996 paper: key text sections read; local PDF pages 3, 4 and 6-8 inspected for algorithm, validity/error model, image comparison and limitations.
- Retain its image-space quality/cost idea, not its historical constants or a claim of global optimality.
- Its practical corner-based angular-error estimate is explicitly non-conservative; it is not a universal geometric certificate.
- Other local papers retain their previous library review status. Vendor chapters/documentation were read as implementation references, not reproduced performance experiments.
