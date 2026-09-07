"""WHAT IS BUILT ONCE IS BUILT ONCE. RAGE's toolchain bakes a sector; the frame draws it.

`include/generate/Generate.h` already states the rule this file exists to keep: "every level is
BAKED when the geometry is built, never in a frame, which is what keeps a frame from meshing."
The lab did the opposite -- it solved a town's whole vertical alignment for EVERY picture, 367 s
at OldTown before a triangle existed, and again the moment the camera moved a metre.

TWO DEFECTS, AND THE SLOWER ONE IS NOT THE WORSE ONE. Cost is the visible half. The other is that
a camera-shaped domain is not DETERMINISTIC: walk one metre, the extract moves, the solve changes,
and the road under your feet moves with it. So a bake is addressed by the GROUND -- a place, a
tile, an extract -- and never by where anybody stands.

THE KEY CARRIES EVERYTHING THE ANSWER DEPENDS ON, INCLUDING THE CODE. A cache keyed on the inputs
alone serves yesterday's answer from today's solver, which is the stale-gate trap this tree has
already paid for once: a case passed against an API that no longer existed because the freshness
check could not see headers. So the digest of the SOURCE that computes the answer is part of the
key, and editing the solver invalidates every bake it ever wrote, with nothing to remember.

    key = sha256(name, source digests, inputs)   ->   build/lab/bake/<name>/<key>.npz
"""
import hashlib
import os
import pathlib

import numpy as np

# THE BAKE HAS AN OFF SWITCH BECAUSE ITS PROOF NEEDS ONE. `OUTSHINE_NOBAKE=1` makes every read a
# miss, which is how "the baked answer equals the solved one" is stated as a comparison rather
# than as a hope -- and how a suspected stale answer is settled in one run.
OFF = os.environ.get("OUTSHINE_NOBAKE", "") not in ("", "0")

ROOT = pathlib.Path(__file__).resolve().parents[2] / "build" / "lab" / "bake"


def digest_of(*paths):
    """The SHA-256 of the files that compute an answer, so a change to any of them is a miss."""
    got = hashlib.sha256()
    for path in sorted(str(p) for p in paths):
        try:
            got.update(pathlib.Path(path).read_bytes())
        except OSError:
            got.update(b"\0")
    return got.hexdigest()


def key(*parts):
    """One digest over everything the answer depends on. Arrays go in by their bytes, so a DEM
    that moved by a millimetre is a different key and not a rounding argument."""
    got = hashlib.sha256()
    for part in parts:
        if isinstance(part, np.ndarray):
            got.update(np.ascontiguousarray(part).tobytes())
            got.update(str(part.dtype).encode())
        elif isinstance(part, (list, tuple)):
            got.update(key(*part).encode())
        elif isinstance(part, dict):
            got.update(key(*sorted(f"{k}={v}" for k, v in part.items())).encode())
        else:
            got.update(str(part).encode())
        got.update(b"\x1f")
    return got.hexdigest()


def read(name, digest):
    """The baked arrays, or None. A bake that cannot be read is a MISS and never an error: the
    answer is recomputable by definition, which is what makes it a cache rather than a store."""
    if OFF:
        return None
    got = ROOT / name / f"{digest}.npz"
    if not got.exists():
        return None
    try:
        with np.load(got, allow_pickle=False) as held:
            return {k: held[k] for k in held.files}
    except Exception:
        return None


def write(name, digest, **arrays):
    here = ROOT / name
    here.mkdir(parents=True, exist_ok=True)
    out = here / f"{digest}.npz"
    tmp = here / f".{digest}.part.npz"          # np.savez APPENDS .npz unless the name has it
    np.savez(tmp, **arrays)
    tmp.replace(out)
    return out


def ledger():
    n = bytes_ = 0
    for got in ROOT.rglob("*.npz"):
        n += 1
        bytes_ += got.stat().st_size
    return {"bakes": n, "bytes": bytes_, "where": str(ROOT)}
