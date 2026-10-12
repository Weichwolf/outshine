"""Compare optimized patch kernels against the previous checked implementation."""

import hashlib
import runpy
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch

import numpy as np
from fork_adaptive import adaptive_mesh

REFERENCE_COMMIT = "76dab3498a1b225195c44e167326ca74a5b7bdec"


def kernel_checks(fields):
    source = subprocess.check_output(
        [
            "git",
            "show",
            f"{REFERENCE_COMMIT}:test/experiments/vegetation/fork_adaptive.py",
        ],
        text=True,
    )
    with TemporaryDirectory(prefix="outshine-adaptive-reference-") as directory:
        path = Path(directory) / "previous.py"
        path.write_text(source)
        previous = runpy.run_path(str(path))
    cases = 0
    for field in fields.values():
        actual, work = adaptive_mesh(field, 0.004, max_depth=9)
        with patch.multiple(
            "fork_adaptive",
            patch_points=previous["patch_points"],
            conforming_triangles=previous["conforming_triangles"],
        ):
            expected, previous_work = adaptive_mesh(field, 0.004, max_depth=9)
        for name in actual:
            np.testing.assert_array_equal(actual[name], expected[name])
        assert work == previous_work
        cases += 1
    return {
        "exact_previous_patch_kernel_cases": cases,
        "patch_kernel_reference_commit": REFERENCE_COMMIT,
        "patch_kernel_reference_sha256": hashlib.sha256(source.encode()).hexdigest(),
        "patch_kernel_comparison_scope": "same depth-nine driver and inputs, previous point-generation and conforming-triangulation kernels; no claim that the previous driver accepted depth nine",
    }
