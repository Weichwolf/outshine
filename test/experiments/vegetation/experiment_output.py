"""Require explicit private output locations for research products."""

import tempfile
from pathlib import Path


def private_output(path):
    root = Path(tempfile.gettempdir()).resolve()
    output = Path(path).resolve()
    if output == root or not output.is_relative_to(root):
        raise ValueError("explicit system-temp subdirectory required")
    return output
