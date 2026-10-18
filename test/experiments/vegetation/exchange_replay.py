"""Fresh geometric-product hit with forbidden generation and separate current fields."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from exchange_basis import finite_mesh
from exchange_io import read_exchange
from exchange_light import basis_optics
from exchange_operator import mix_sides, prepare_csr
from exchange_project import project
from scatter_scene import ScatterScene


def run(prepared, inputs, output):
    report = json.loads((prepared / "result.json").read_text())
    fingerprint = hashlib.sha256(
        (prepared / "product/geometry.npz").read_bytes()
    ).hexdigest()
    began = time.perf_counter()
    with (
        patch(
            "exchange_basis.prepare_stand", side_effect=AssertionError("basis rebuilt")
        ),
        patch(
            "exchange_operator.build_links",
            side_effect=AssertionError("transfer retraced"),
        ),
        patch(
            "scatter_scene.ScatterScene.__init__",
            side_effect=AssertionError("BVH rebuilt"),
        ),
    ):
        basis, links = read_exchange(prepared / "product", report["geometry_sha256"])
        decode = time.perf_counter() - began
        began = time.perf_counter()
        matrix = prepare_csr(links)
        prepare = time.perf_counter() - began
        with np.load(prepared / "frame-diagnostics.npz", allow_pickle=False) as f:
            frames = {k: f[k] for k in f.files}
        times = {}
        hashes = {}
        for name, state in report["states"].items():
            optical = basis_optics(basis, state)
            began = time.perf_counter()
            value = mix_sides(matrix @ frames[name + "_direct"], optical)
            times[name] = time.perf_counter() - began
            np.testing.assert_array_equal(value, frames[name + "_first"])
            hashes[name] = hashlib.sha256(value.tobytes()).hexdigest()
        np.testing.assert_array_equal(
            frames["back-bright_first"], 2 * frames["back_first"]
        )
    _, vertices, faces, identity = finite_mesh(inputs)
    if identity != basis["geometry_sha256"]:
        raise ValueError("pose/source geometry changed")
    scene = ScatterScene(vertices, faces)
    with (
        patch(
            "exchange_basis.prepare_stand", side_effect=AssertionError("basis rebuilt")
        ),
        patch(
            "exchange_operator.build_links",
            side_effect=AssertionError("transfer retraced"),
        ),
    ):
        views = {}
        for name, view in (
            ("east", (np.sqrt(3) / 2, 0, 0.5)),
            ("north", (0, np.sqrt(3) / 2, 0.5)),
        ):
            values = project(
                scene, basis, {"back": frames["back_first"]}, 64, 36, view=view
            )["back"]
            if not np.isfinite(values).all():
                raise ValueError("invalid cached camera response")
            views[name] = hashlib.sha256(values.tobytes()).hexdigest()
    assert views["east"] != views["north"]
    assert (
        fingerprint
        == hashlib.sha256((prepared / "product/geometry.npz").read_bytes()).hexdigest()
    )
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "identity": basis["identity"],
        "geometry_npz_sha256": fingerprint,
        "decode_s": decode,
        "prepare_csr_s": prepare,
        "current_exchange_s": times,
        "field_hashes": hashes,
        "camera_query_hashes": views,
        "camera_query_work": scene.work,
        "scope": "Fresh operator hit without basis/transfer/BVH generation for application. Given current direct fields are separate frame diagnostics. Numerical 64x36 two-view geometry queries separately build one primary-only BVH; not a full angular image gate.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("prepared", "inputs", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    a = p.parse_args()
    run(a.prepared, a.inputs, a.output)
