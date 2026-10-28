"""Measure sampled whole-contour selection on the fixed original source cells."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
import shapely
from parent_surface_inputs import open_parent
from source_contour_selection import project_contours, select_contours
from source_plane_mesh import triangulate
from source_surface_patches import original_patches


def compare(patches, projection, pixels, half_extent, complete_area):
    started = time.perf_counter()
    selected, owners, work = select_contours(projection, pixels, half_extent)
    seconds = time.perf_counter() - started
    contours = projection.tree.geometries[np.isin(projection.identities, selected)]
    area = float(shapely.union_all(contours).area)
    return (
        {
            "pixels": pixels,
            "selected_contours": len(selected),
            "selected_sources": len({patches[i].source for i in selected}),
            "triangles": sum(len(triangulate(patches[i].domain)) for i in selected),
            "selected_projected_union_area_m2": area,
            "complete_projected_union_area_m2": complete_area,
            "coverage_ratio": area / complete_area,
            "selection_seconds": seconds,
            **work,
        },
        selected,
        owners,
    )


def run(args):
    request = json.loads(args.request.read_text())
    source = json.loads(args.source_report.read_text())
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with open_parent(args.input) as parent:
        if parent.parent != request["parent"] or parent.parent != source["parent"]:
            raise ValueError("source/request/capture parent mismatch")
        patches, basis, anchor, accepted, needs = original_patches(
            parent, set(request["cells"])
        )
        report = {
            "source_archive_sha256": parent.archive_sha256,
            "form_reads": parent.form_reads,
            "form_bytes": parent.form_bytes,
            "source_patches": len(patches),
            "accepted_sources": len(accepted),
            "needs": needs,
        }
    report["source_domain_seconds"] = time.perf_counter() - started
    native = np.array([request["east"], request["up"], -np.array(request["north"])])
    centre = (
        np.array(request["origin"])
        + np.mean([source["least"], source["most"]], axis=0) @ native
    ) @ basis.T - anchor
    rotation = native @ basis.T
    cases = []
    for view in (0, 1):
        toward = np.array(request["directions"][view])
        right = np.array([toward[2], 0, -toward[0]])
        right /= np.linalg.norm(right)
        up = np.cross(toward, right)
        started = time.perf_counter()
        projection = project_contours(
            patches, centre, toward @ rotation, right @ rotation, up @ rotation
        )
        seconds = time.perf_counter() - started
        complete_area = float(shapely.union_all(projection.tree.geometries).area)
        for pixels in (16, 32, 64):
            case, selected, owners = compare(
                patches, projection, pixels, source["half_extent_m"], complete_area
            )
            case.update(
                view=view,
                projection_seconds=seconds,
                collapsed_contours=projection.collapsed.tolist(),
            )
            cases.append(case)
            np.savez_compressed(
                args.output / f"view-{view}-{pixels}.npz",
                selected=selected,
                owners=owners,
            )
    report.update(
        cases=cases,
        scope="Source domains before triangulation. Subpixel omissions, projection "
        "collapses and Python source remainders remain explicit. No native material, "
        "World, SSE or device frame-budget admission.",
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            [{k: v for k, v in c.items() if k != "collapsed_contours"} for c in cases]
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--request", type=Path, required=True)
    parser.add_argument("--source-report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
