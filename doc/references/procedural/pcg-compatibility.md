# Plant recipe random streams

The native PlantRandom reproduces the two existing plant-recipe seed domains:
NumPy SeedSequence(seed) and SeedSequence((seed, axis_id)), default four-word
pool, followed by PCG64 XSL-RR128/64. No spawning, general entropy API, alternate
bit generator or cryptographic security is claimed. State uses two portable
64-bit words; high multiplication uses32-bit limbs. There is no new runtime
dependency or vendored implementation. Homebrew has no pcg-cpp formula on the
checked host. Recipe seeds, IDs and source semantics are retained.

Primary algorithm sources inspected locally at pinned NumPy2.2.0 commit
[e7a123b2d3eca9897843791dd698c1803d9a39c2](https://github.com/numpy/numpy/tree/e7a123b2d3eca9897843791dd698c1803d9a39c2):
[SeedSequence](https://github.com/numpy/numpy/blob/e7a123b2d3eca9897843791dd698c1803d9a39c2/numpy/random/bit_generator.pyx),
[PCG64](https://github.com/numpy/numpy/blob/e7a123b2d3eca9897843791dd698c1803d9a39c2/numpy/random/src/pcg64/pcg64.h),
[state-word order](https://github.com/numpy/numpy/blob/e7a123b2d3eca9897843791dd698c1803d9a39c2/numpy/random/src/pcg64/pcg64.c).
The finite native comparison uses installed NumPy2.5.4:602single/paired cases,
including zero,32-bit boundaries and full64-bit seeds/axis IDs;38528raw words
agree exactly. Four subsequent uniforms per case differ by at most4.441e-16
on [-pi, pi]; the native interval arithmetic disables contraction like other
plant math. This does not prove plant morphology, world integration or a LOD bound.
Reproduction: test/experiments/render/native_plant_random_reference.py with
--binary pointing at the compiled NativePlantRandomReference consumer and an
explicit system-temp --output. Module suite: generators/flora/PlantRandom.

## Algorithm acknowledgments and permission

The published entropy mixer and PCG algorithms are credited to their authors.
The corresponding permissive notices are retained here for the compatibility
implementation; no upstream code is bundled.

The MIT License (MIT)

Copyright (c) 2015 Melissa E. O'Neill
Copyright (c) 2019 NumPy Developers
Copyright (c) 2014 Melissa O'Neill
Copyright (c) 2015 Robert Kern

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
