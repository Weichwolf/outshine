"""Create fresh experiment output under canonical host temporary directories."""

import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def create_output(output):
    resolved = output.resolve()
    roots = {Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve()}
    if resolved.is_relative_to(ROOT.resolve()) or not any(
        resolved != root and resolved.is_relative_to(root) for root in roots
    ):
        raise ValueError(
            "Fresh explicit system-temp output outside the checkout required"
        )
    output.mkdir(parents=True, exist_ok=False)
