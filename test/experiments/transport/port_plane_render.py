"""Render unchanged old C0 and new C1 buffers in whole-tile and matching detail views."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport import close_surface
from port_plane_fixture import load_reference
from surface_height_checks import normal_gradients
from surface_solid_showcase import inspection_view, write_case

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OLD = HERE / "fixtures/surface-sampling-vienna-4ea502363.npz"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def detail_view(product, captured, before, after):
    edges = np.sort(
        np.concatenate(
            [product["indices"][:, pair] for pair in ([0, 1], [1, 2], [2, 0])]
        ),
        axis=1,
    )
    lookup = {
        tuple(edge): i for i, edge in enumerate(np.sort(product["port_edges"], axis=1))
    }
    pairs = [[] for _ in range(len(lookup))]
    for edge, face in zip(edges, np.tile(np.arange(len(product["indices"])), 3)):
        owner = lookup.get(tuple(edge))
        if owner is not None:
            pairs[owner].append(int(face))
    gradients = normal_gradients(
        before.vertices[:, :2],
        before.triangles[: len(product["indices"])],
        before.vertices[:, 2],
    )
    jumps = np.linalg.norm(
        gradients[np.array(pairs)[:, 0]] - gradients[np.array(pairs)[:, 1]], axis=1
    )
    port = int(np.argmax(jumps))
    source_vertices = captured["road_vertex_map"][product["port_edges"][port]]
    centre = before.vertices[source_vertices].mean(axis=0)
    centre[2] = 0.5 * (centre[2] + np.median(after.vertices[:, 2]))
    return {
        "eye": (centre + np.array([70, -95, 65])).tolist(),
        "target": centre.tolist(),
        "span_m": 140,
        "profile": [1920, 1080],
        "detail_view": True,
        "selected_original_port": port,
        "original_finite_gradient_jump": float(jumps[port]),
    }


def build(directory, output, *, allow_dirty=False):
    report = json.loads((directory / "report.json").read_text())
    verification = json.loads((directory / "verification.json").read_text())
    if (
        not verification["valid"]
        or verification["mesh_sha256"] != report["mesh_sha256"]
    ):
        raise ValueError(
            "fresh complete geometry verification required before rendering"
        )
    if report["working_tree_dirty"] and not allow_dirty:
        raise ValueError("release rendering requires a clean generation commit")
    path = directory / report.get("mesh_file", "diagnostic.npz")
    if digest(path) != report["mesh_sha256"]:
        raise ValueError("new plan changed")
    old_metadata = json.loads(OLD.with_suffix(".json").read_text())
    if digest(OLD) != old_metadata["fixture_sha256"]:
        raise ValueError("original C0 reference changed")
    product, captured, metadata, _sources = load_reference()
    with np.load(OLD) as data:
        before = Mesh(data["vertices"], data["triangles"])
    np.testing.assert_array_equal(before.vertices[:, :2], captured["ground_xy"])
    np.testing.assert_array_equal(before.triangles, captured["ground_faces"])
    with np.load(path) as data:
        after = Mesh(np.c_[data["xy"], data["heights"]], data["faces"])
        after_parts = data["parts"]
    before_parts = np.r_[
        product["face_owners"],
        np.full(len(before.triangles) - len(product["indices"]), -1),
    ]
    common = inspection_view(np.vstack([before.vertices, after.vertices]))
    detail = detail_view(product, captured, before, after)
    output.mkdir(parents=True, exist_ok=True)
    configurations = []
    for label, mesh, parts in (
        ("before", before, before_parts),
        ("after", after, after_parts),
    ):
        body = close_surface(mesh, 1.0)
        slots = np.full(len(body.triangles), 2, int)
        slots[: len(mesh.triangles)] = np.where(parts >= 0, 0, 5)
        for view_label, view in (("whole", common), ("detail", detail)):
            proof = {
                "valid_surface": True,
                "generation_commit": report["generation_commit"],
                "working_tree_dirty": report["working_tree_dirty"],
                "frame": metadata["frame"],
                "new_plan_sha256": digest(path),
                "old_C0_fixture_sha256": digest(OLD),
                "scope": "Whole captured tile plus original exterior margin, or explicit camera crop of the same complete unmodified buffers. True metric XYZ; 1m display slab only; no full transport/LOD/native proof.",
            }
            write_case(
                f"Vienna-portplanes-{view_label}-{label}",
                body,
                slots,
                proof,
                configurations,
                view,
                output_dir=output,
            )
    return configurations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allow-dirty", action="store_true")
    args = parser.parse_args()
    configurations = build(args.directory, args.output, allow_dirty=args.allow_dirty)
    scenes = args.output / "scenes.json"
    scenes.write_text(json.dumps(configurations, indent=2) + "\n")
    subprocess.run(
        [
            "blender",
            "--background",
            "--factory-startup",
            "--python-exit-code",
            "1",
            "--python",
            str(HERE / "render_transport_solid.py"),
            "--",
            str(scenes),
        ],
        cwd=ROOT,
        check=True,
    )
    records = {
        Path(c["output"]).name: digest(Path(c["output"])) for c in configurations
    }
    (args.output / "images.json").write_text(
        json.dumps(
            {"records": records, "diagnostic_dirty_allowed": args.allow_dirty}, indent=2
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
