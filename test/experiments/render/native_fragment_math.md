# Metal fragment arithmetic

Owner: [#328](https://github.com/Weichwolf/outshine/issues/328).

Metal's `MTLMathModeSafe` disables unsafe floating-point optimizations.
`MTLMathModeRelaxed` permits reassociation and other unsafe optimizations while
preserving infinities and NaNs. These are the contracts in `MTLLibrary.h` in the
Command Line Tools SDK. Fast math functions are a separate option and unchanged.

Only fragment compilation uses Relaxed. Vertex and compute compilation retain
Safe: positions, culling, depth bounds and conservative LOD decisions must not
silently acquire different arithmetic. SPIRV-Cross translates explicit GLSL
`precise` operations through non-optimizable multiplication/addition helpers.
The native Metal math probe checks their actual GPU output in all three stages,
vertex/compute cancellation and classification of finite values, signed NaNs
and signed infinities supplied as runtime uniform bits. This is not a promise
of identical ordinary shading across compilers or hardware.
Vulkan's compiler policy is unchanged; this Metal-specific cancellation check
does not impose a new arithmetic contract on another backend.

A18 Pro, macOS, 1280×720/30, complete fresh offline processes, normal priority:

| GPU maximum, ms | Safe before / after | Fragment Relaxed, two runs |
|---|---:|---:|
| Koerbersee | 41.081 / 41.114 | 36.241 / 36.123 |
| Rosenheim | 36.653 / 36.010 | 29.810 / 28.890 |

Order is Safe / Relaxed / Relaxed / Safe, each with both Places. Every process
records exactly 30 frames and 30 valid GPU timestamps; no first frame is omitted.
For Koerbersee, the mean of the two maxima changes by
`(1 - (36.241 + 36.123) / (41.081 + 41.114)) * 100 = 11.96%`.
Rosenheim changes by 19.22%. Means vary more; this small experiment establishes
neither sustained whole-game performance nor GPU invocation counts.

Each variant reproduces its own PNG hash twice. Compared with Safe, the final
Koerbersee image has mean RGB channel error 0.008804/255 and maximum 11/255;
Rosenheim has 0.008607/255 and maximum 32/255. The comparison uses
`test/scripts/pixels.py`. The images require visual review; a changed hash alone
does not establish a regression or an improvement. Geometry, source inputs,
visibility, material complexity, profile and view distance are unchanged.

Relaxing vertex arithmetic as well changes more contour pixels and provides no
useful additional gain in the earlier paired trial; it is not adopted. Skipping
coarse-patch vertex normal construction preserves the two final images byte for
byte but has no consistent timing benefit in its paired trial; it is not adopted.

The complete production Metal probe passes with validation, including 154 math
checks. A private negative control that also relaxes vertex arithmetic fails the
five cancellation checks. All eight native Terrain/GroundLattice cases pass.
Focused clang-tidy covers the changed Metal implementation and the new test
translation unit/native headers; format and documentation checks pass.

Twelve fresh public-client offline Places then record 30/30 valid samples each.
Ten hold all measured limits. Wien remains at GPU maximum 33.592 ms and turn
1004.638 ms; Koerbersee at GPU maximum 36.693 ms. All CPU-p99 are below 7 ms.
All final images were inspected in three overview sheets; Tokyo, Wien and the
two paired comparisons were opened separately. Across twelve Safe comparisons,
mean RGB error is at most 0.018897/255, maximum at most 34/255, with no newly
observed structural image defect. Existing fragmented near vegetation, crude
shore/terrain transitions and flat material/lighting remain real deficiencies.
These captures do not establish temporal or contact stability in a moving game.

Koerbersee still exceeds `1000 / 30 = 33.333` ms. All-Place measurements,
temporal/contact checks and complete native integration remain separate
requirements. This change neither raises a budget nor admits the native port
to master by itself.
