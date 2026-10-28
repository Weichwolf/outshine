"""Historical identical facade producer and a local footprint-reuse alternative."""

import ast
import inspect
import subprocess
from collections import Counter
from types import CodeType, FunctionType
from unittest.mock import patch

import terraced_facade_plan as facade
from shapely.geometry.base import BaseGeometry

BASELINE = "aad2bd6df4d9aa6b7a928cfe4c936dae0b7269fd"


def procedures():
    source = subprocess.check_output(
        [
            "git",
            "show",
            BASELINE + ":test/experiments/buildings/terraced_facade_plan.py",
        ],
        text=True,
    )
    node = next(
        n
        for n in ast.parse(source).body
        if isinstance(n, ast.FunctionDef) and n.name == "candidates"
    )
    historical = ast.get_source_segment(source, node)
    current = inspect.getsource(facade.candidates)
    footprint = (
        current.replace(
            "    accepted, rejected = [], []",
            "    accepted, rejected = [], []\n    margins = {}",
        )
        .replace(
            "        margin = None",
            "        margin = margins.get(storey.footprint_wkb)",
        )
        .replace(
            "                        margin = polygon.buffer(1e-8)",
            "                        margin = polygon.buffer(1e-8)\n"
            "                        margins[storey.footprint_wkb] = margin",
        )
    )
    controls = {"storey": facade.candidates}
    for name, text in (("baseline", historical), ("footprint", footprint)):
        compiled = compile(text, f"historical-facade-margin-{name}.py", "exec")
        code = next(
            value
            for value in compiled.co_consts
            if isinstance(value, CodeType) and value.co_name == "candidates"
        )
        controls[name] = FunctionType(code, vars(facade).copy(), name)
    return controls


def margin_work(call):
    original, counts = BaseGeometry.buffer, Counter()

    def buffer(polygon, distance, *args, **kwargs):
        if distance == 1e-8:
            counts["margin_buffer_calls"] += 1
        return original(polygon, distance, *args, **kwargs)

    with patch.object(BaseGeometry, "buffer", buffer):
        result = call()
    return result, dict(counts)
