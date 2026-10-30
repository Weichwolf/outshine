# Native perspective-world opponents

Frozen A18 Pro inputs for #1831 / #1797. Two 1280×720 perspective poses and their
viewer-lit 128² source captures; whole unchanged 1558-source original group,
terrain and an opening opponent. Six fresh native processes, eighteen query renders.

Read/reproduce with
[native_world_field.md](../../../../experiments/render/native_world_field.md).
`manifest.json` binds all executed requests, native reports, source/binary hashes,
cropped arrays and full raw-buffer hashes. `samples.npz` is lossless, 67967 bytes;
do not load with pickle. Empty pixels outside each declared crop reconstruct to
exact zero. All 36 logical arrays reconstruct their native full-buffer hashes.

Both original unions and identical repeats are exact in this saved input. This
does not close intermittent #1806 or qualify reconstructed fields. The geometric
Python bounds are 0.0763/2.2747 px; they exclude coverage, PBR and GPU arithmetic.
Runtime World revision/WorldFromField remain absent, rather than fabricated.

[ss2/](ss2/) preserves the separate #1838 quarter-pixel World reference;
the point inputs above remain unchanged.
