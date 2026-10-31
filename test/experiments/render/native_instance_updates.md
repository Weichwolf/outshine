# Native instance update cost

Build `test_experiments_render_NativeInstanceUpdates` with CMake, then run the
executable with a new report path in system temp. It creates8192 actual piece
handles and records CPU update-call times at512…8192 pieces, unchanged/moving
rows and a repeated-handle rejection. Setup, one GPU frame and teardown are
outside those timings. This is an explicit experiment, not a normal CTest gate.

The report contains the actual handles, device, piece slots and CPU slot bytes.
Compare the same inputs and timed procedure, including errors followed by valid
updates. Do not infer whole-frame, GPU, physical peak-memory or720p60 results.
Source payloads and the normal World remain unchanged by this experiment.

Duplicate validation previously made `n*(n-1)/2` handle comparisons. Temporary
piece visit marks require at most two visits per input; clear the validated
prefix on every validation exit before calling the renderer. Single-piece
updates need no visit mark. Measure slot bytes on each target; padding reuse
is an observed layout property, not a new CPU/GPU ABI contract.
