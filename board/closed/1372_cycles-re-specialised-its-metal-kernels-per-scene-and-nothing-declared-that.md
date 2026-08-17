Type: bug
Area: corpus
Tags: perf, oracle, instrument

**Cycles re-specialised its Metal kernels per scene, and nothing declared that**

**The owner saw `MTLCompilerService` burning CPU in `ps`.** My first measurement said it was not us and
that measurement was right about the wrong process: `/usr/bin/time` reports the wall clock of the child,
and the compiler service is a **separate daemon**, so its work is invisible there.

[MEASURED] with `ps` sampled around a run:

| | |
|---|---|
| our own shader suite, 24 arms | **zero** `MTLCompilerService` processes, zero CPU. Our MSL is warm-cached by macOS |
| five case preparations, forced | **9.66 s of compiler CPU over 24.7 s of wall — 39 %** |

**So it is Blender, and it is not a cache that fails.** `bpy.context.preferences.addons['cycles']
.preferences.kernel_optimization_level` defaults to **`FULL`**, which specialises the render kernel to
the SCENE'S FEATURE SET. A corpus of many small cases with different materials, lights and extensions is
therefore a corpus of many different kernels, and each one is a fresh Metal compile.

## The measurement, same five cases, one variable

| level | wall |
|---|---|
| `FULL` (Blender's default) | **24.0 s** |
| `INTERSECT` | 22.1 s |
| **`OFF`** | **16.5 s** |

**`OFF` is 31 % faster.** *The compiler-CPU column that belongs beside this is NOT quoted, and the reason
is that the instrument is broken: `ps` sums only living processes, so a service that exits between two
samples takes its time with it and the delta comes back negative. The wall clock is the number that
survived scrutiny.*

## And it changes no pixel, which is the claim that mattered

A faster oracle that rendered something else would be worthless. [MEASURED] `FULL` against `OFF`, the
same case forced twice, comparing `oracle.raw` byte for byte:

| case | what it carries | |
|---|---|---|
| `Suzanne` | a closed body, one material, no light | **bit-identical**, 14 745 600 bytes |
| `NormalTangentTest` | a delta sun, normal maps, five rows of pairs | **bit-identical** |
| `SpecularTest` | 24 materials, an extension, a declared environment | **bit-identical** |

**Three cases and not one**, chosen for different features rather than for being handy — a kernel
specialisation that changed a picture would show first where the kernel does most.

## What is declared now

`OUTSHINE_CYCLES_KERNELS`, defaulting to `OFF`. **The level is stated by the preparer rather than
inherited from a Blender preference**, which is the same rule the rest of the recipe already follows:
a picture is a function of the declaration, and a setting nobody declared is a setting that can change
under a run.

- [ ] **It is NOT in the oracle's cache key, and that is a decision rather than an oversight.** Three
  cases say it changes nothing; if a fourth ever disagrees, the level belongs in the key and this box is
  the place that says so.
