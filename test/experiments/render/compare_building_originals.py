"""Compare complete native source emission with separately archived original meshes."""

import argparse
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from field_background_inputs import ATTRIBUTES, DATA
from native_building_cell_reference import load_cells, native_parts
from parent_surface_inputs import open_parent

PROFILES = Path(__file__).resolve().parents[3] / "src/content/shade/FacadeProfiles.inc"
PROFILE_COUNT = sum(
    line.startswith("FACADE_PROFILE(") for line in PROFILES.read_text().splitlines()
)


def native_normals(words, basis):
    xy = np.c_[words >> 16, words & 65535].astype(np.float32)
    xy = xy / np.float32(65535) * np.float32(2) - np.float32(1)
    x, y = xy.T.copy()
    z = (np.float32(1) - abs(x)) - abs(y)
    below, before = z < 0, x.copy()
    x[below] = (1 - abs(y[below])) * np.where(
        before[below] >= 0, np.float32(1), np.float32(-1)
    )
    y[below] = (1 - abs(before[below])) * np.where(
        y[below] >= 0, np.float32(1), np.float32(-1)
    )
    length = np.sqrt((x * x + y * y) + z * z)
    unit = (np.c_[x, y, z] * (np.float32(1) / length)[:, None]).astype(np.float64)
    return np.column_stack(
        [
            (unit[:, 0] * axis[0] + unit[:, 1] * axis[1]) + unit[:, 2] * axis[2]
            for axis in basis
        ]
    )


def reference(cells, request):
    basis = np.asarray([request[key] for key in ("east", "north", "up")])
    with open_parent(DATA / "original-inputs.tar.gz") as parent:
        selected = np.flatnonzero(np.isin(parent.cells, cells))
    original, provenance = load_cells(
        DATA / "native-cells/manifest.json",
        DATA / "original-inputs.tar.gz",
        set(cells),
        {
            "basis": basis,
            "anchor": np.asarray(request["origin"]) @ basis.T,
            "selected_sources": selected,
        },
    )
    original["vertex_normal"] = native_normals(original["normal"], basis)
    return native_parts(original), provenance


def exported(root):
    description = json.loads((root / "geometry/0/input.json").read_text())
    parts = []
    for at, row in enumerate(description["parts"]):
        part = {
            key: np.fromfile(root / f"geometry/0/{at}/{key}.bin", "<f4").reshape(
                row["vertices"], width
            )
            for key, width in (
                ("positions", 3),
                ("normals", 3),
                ("uv", 2),
                ("uv1", 2),
                ("colours", 4),
            )
        }
        part["indices"] = np.fromfile(
            root / f"geometry/0/{at}/indices.bin", "<u4"
        ).reshape(row["triangles"], 3)
        part["material"] = row["material"]
        parts.append(part)
    return parts


def triangles(parts):
    rows = []
    for at, part in enumerate(parts):
        if not len(part["indices"]):
            continue
        vertices = np.concatenate(
            [part[key] for key in ATTRIBUTES if key != "indices"], axis=1
        )
        faces = vertices[part["indices"]].copy()
        faces[faces == 0] = 0
        keys = np.ascontiguousarray(faces[:, :, :3]).view("V12").reshape(-1, 3)
        first = np.argsort(keys, axis=1)[:, 0]
        order = (np.arange(3)[None, :] + first[:, None]) % 3
        faces = np.take_along_axis(faces, order[:, :, None], axis=1)
        role = np.full(len(faces), part.get("material", at), dtype=np.float32)
        texture = part["uv"][part["indices"]][:, :, 0]
        code = np.floor(texture / 256).astype(np.int64)
        masonry = (
            (texture >= 0)
            & (code // (PROFILE_COUNT * 3) == 6)
            & (code % PROFILE_COUNT != 7)
            & ((code // PROFILE_COUNT) % 3 != 1)
        )
        role[(role == 2) & ~np.any(masonry, axis=1)] = 0
        rows.append(np.c_[faces.reshape(len(faces), -1), role])
    joined = np.ascontiguousarray(np.concatenate(rows), dtype="<f4")
    return np.sort(joined.view(f"V{joined.shape[1] * 4}").ravel())


def stage(root, request):
    root.mkdir()
    with open_parent(DATA / "original-inputs.tar.gz") as parent:
        (root / "input.json").write_text(json.dumps(parent.metadata))
        for name in [
            parent.metadata["index"],
            *(parent.forms[c] for c in request["cells"]),
        ]:
            path = root / name
            if not path.resolve().is_relative_to(root.resolve()):
                raise ValueError("unsafe source package path")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(parent.member(name))
        return parent.cells.copy(), parent.plans["summaries"]


def run(binary, source, root, request, expected):
    request_path = root.with_suffix(".json")
    request_path.write_text(json.dumps(request, indent=2) + "\n")
    command = [str(binary), str(source / "input.json"), str(request_path), str(root)]
    with root.with_suffix(".log").open("w") as log:
        result = subprocess.run(
            command, check=False, stdout=log, stderr=subprocess.STDOUT, timeout=60
        )
    report = json.loads((root / "originals-result.json").read_text())
    if result.returncode != expected or report["device_creations"] != 0:
        raise ValueError("unexpected native result or GPU work: " + root.name)
    if expected and (root / "geometry").exists():
        raise ValueError("failed source group exported partial geometry")
    return report


def compare(args):
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        raise ValueError("fresh explicit system-temp output required")
    output.mkdir(parents=True)
    request = json.loads((DATA / "source-plane-request.json").read_text())
    cells, summaries = stage(output / "source", request)
    manifest = json.loads((DATA / "native-cells/manifest.json").read_text())
    complete = [row["cell"] for row in manifest["cells"] if not row["unsupported"]]
    selected = np.flatnonzero(np.isin(cells, complete))
    count = sum(
        row["triangles"] for row in manifest["cells"] if row["cell"] in complete
    )
    limits = {
        "sources": len(selected),
        "faces": sum(summaries[i]["faces"] for i in selected),
        "triangle_attempts": 4 * count,
    }
    base = request | {"cells": complete, "limits": limits}
    records = {}
    for name, chosen in [(f"cell-{cell}", [cell]) for cell in complete] + [
        ("complete-group", complete)
    ]:
        query = base | {"cells": chosen}
        report = run(args.binary.resolve(), output / "source", output / name, query, 0)
        actual = exported(output / name)
        original, provenance = reference(chosen, query)
        if not np.array_equal(triangles(actual), triangles(original)):
            raise ValueError("original triangles or native attributes differ: " + name)
        if report["form_reads"] != len(chosen):
            raise ValueError("native source forms were not loaded exactly once")
        records[name] = {
            "native": report,
            "reference": provenance,
            "triangles_and_attributes_exact": True,
        }
    cases = [
        ("unsupported-group", request | {"limits": limits}, 3),
        ("cancelled", base | {"cancelled": True}, 4),
    ]
    for key in limits:
        cases.append(("zero-" + key, base | {"limits": limits | {key: 0}}, 1))
    cases.append(
        ("one-triangle", base | {"limits": limits | {"triangle_attempts": 1}}, 1)
    )
    for name, query, kind in cases:
        report = run(args.binary.resolve(), output / "source", output / name, query, 1)
        if report["originals"].get("error_kind") != kind:
            raise ValueError("wrong source failure: " + name)
        if name != "one-triangle" and report["form_reads"] != 0:
            raise ValueError("source refusal performed lazy form IO: " + name)
        records[name] = report
    (output / "comparison.json").write_text(json.dumps(records, indent=2) + "\n")
    print(
        json.dumps(
            {
                "native_cases": len(records),
                "complete_sources": len(selected),
                "original_triangles": count,
                "terminal": 0,
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    compare(parser.parse_args())
