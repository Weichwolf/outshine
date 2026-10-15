"""Independently replay saved hardware dimensions and complete finite-bed footprints."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import outshine_ref.transport.rail_hardware as rail_library
import shapely
from outshine_ref import contracts
from outshine_ref.contracts import Mesh
from rail_graph_inputs import read_window
from rail_hardware_checks import closed_volume


def actual_bed(root, source_name, pinned_domain):
    pins = json.loads((root / "generation.json").read_text())["files"]
    triangles = []
    for relative, digest in pins.items():
        if relative.startswith(f"results/{source_name}/components/product-"):
            path = root / relative
            if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                raise ValueError("finite bed product changed")
            with np.load(path) as data:
                triangles.extend(shapely.polygons(data["vertices"][data["indices"]]))
    bed = shapely.coverage_union_all(triangles)
    error = float(bed.symmetric_difference(pinned_domain).area)
    if error > 1e-7:
        raise ValueError("finite bed triangles disagree with domain pin")
    shapely.prepare(bed)
    return bed, error


def independent_sleepers(transforms):
    unit = np.array(
        [
            [-0.5, -0.5, 0],
            [0.5, -0.5, 0],
            [0.5, 0.5, 0],
            [-0.5, 0.5, 0],
            [-0.5, -0.5, 1],
            [0.5, -0.5, 1],
            [0.5, 0.5, 1],
            [-0.5, 0.5, 1],
        ]
    )
    return np.concatenate([unit @ t[:, :3].T + t[:, 3] for t in transforms])


def verify_piece(vertices, faces, transforms, row, datum):
    nv, nf, sleepers = row["rail_vertices"], row["rail_triangles"], row["sleepers"]
    rails = Mesh(vertices[:nv], faces[:nf])
    boxes = Mesh(vertices[nv:], faces[nf:] - nv)
    rings = rails.vertices.reshape(2, -1, 12, 3)
    foot_centres = (rings[:, :, 0, :2] + rings[:, :, 1, :2]) / 2
    axis = foot_centres.mean(axis=0)
    delta = np.diff(axis, axis=0)
    lengths = np.linalg.norm(delta, axis=1)
    normals = np.c_[-delta[:, 1], delta[:, 0]] / lengths[:, None]
    gap = rings[1, :, 7, :2] - rings[0, :, 6, :2]
    gauge_error = max(
        float(abs(np.einsum("ij,ij->i", g, normals) - row["gauge_m"]).max())
        for g in (gap[:-1], gap[1:])
    )
    expected = independent_sleepers(transforms)
    box_error = float(abs(expected - boxes.vertices).max(initial=0))
    if gauge_error > 1e-9 or box_error > 1e-9:
        raise ValueError("saved gauge or instance geometry differs")
    if (
        abs(rails.vertices[:, 2].min() - datum - 0.18) > 1e-9
        or abs(rails.vertices[:, 2].max() - datum - 0.352) > 1e-9
    ):
        raise ValueError("saved rail section/contact height changed")
    if (
        abs(boxes.vertices[:, 2].min() - datum) > 1e-9
        or abs(boxes.vertices[:, 2].max() - datum - 0.18) > 1e-9
    ):
        raise ValueError("saved sleeper bottom/top is not the chosen finite contact")
    rail_volume, sleeper_volume = closed_volume(rails), closed_volume(boxes)
    if (
        abs(rail_volume - 2 * 0.00892425 * lengths.sum()) > 1e-7
        or abs(sleeper_volume - sleepers * 2.6 * 0.24 * 0.18) > 1e-7
    ):
        raise ValueError("saved closed hardware volume differs from physical recipe")
    shapes = [
        shapely.Polygon(np.vstack((rail[:, 0, :2], rail[::-1, 1, :2])))
        for rail in rings
    ]
    shapes.extend(shapely.polygons(boxes.vertices.reshape(-1, 8, 3)[:, :4, :2]))
    return np.array(shapes, object), shapely.LineString(axis), gauge_error, box_error


def cross_piece_audit(shapes, owners, kinds, graphs):
    tree = shapely.STRtree(shapes)
    visited = 0
    for begin in range(0, len(shapes), 64):
        pairs = tree.query(shapes[begin : begin + 64], predicate="intersects")
        visited += pairs.shape[1]
        if visited > 1000000:
            raise ValueError("independent footprint pair budget")
        a, b = pairs[0] + begin, pairs[1]
        select = (a < b) & (owners[a] != owners[b]) & (kinds[a] == kinds[b])
        overlap = shapely.intersection(shapes[a[select]], shapes[b[select]])
        areas = shapely.area(overlap)
        if np.any(areas > 1e-9):
            bad = int(np.flatnonzero(areas > 1e-9)[0])
            raise ValueError(
                f"hardware pieces {owners[a[select][bad]]}/{owners[b[select][bad]]} overlap by {areas[bad]} m2 at {shapely.get_coordinates(overlap[bad]).tolist()}"
            )
    land_count = 0
    for graph in graphs:
        if graph["rail_graph"]:
            continue
        lines, widths = graph["lines"], graph["half_widths"]
        for begin in range(0, len(lines), 64):
            pairs = tree.query(
                lines[begin : begin + 64],
                predicate="dwithin",
                distance=widths[begin : begin + 64],
            )
            land_count += pairs.shape[1]
            if land_count:
                raise ValueError("hardware intrudes into a source land crossing band")
    return {"footprint_pair_candidates": visited, "hardware_land_band_hits": land_count}


def verify_case(name, directory, camera_products, row):
    source_name = row.get("source_name", "Vienna" if name == "Wien" else name)
    source, _raw, graphs, domain, pins = read_window(camera_products, source_name)
    if pins != row["pins"] or source["origin_lat_lon"] != row["origin_lat_lon"]:
        raise ValueError("hardware source/frame binding changed")
    bed, bed_error = actual_bed(camera_products, source_name, domain)
    lines = [g["lines"][g["eligible"]] for g in graphs if g["rail_graph"]]
    axis_lines = np.concatenate(lines) if lines else np.empty(0, object)
    supported_axes = shapely.union_all(axis_lines).buffer(1e-8, quad_segs=4)
    path = directory / (name + ".npz")
    if hashlib.sha256(path.read_bytes()).hexdigest() != row["mesh_sha256"]:
        raise ValueError("saved rail export changed")
    with np.load(path) as data:
        vertices, faces, transforms = (
            data["vertices"],
            data["triangles"],
            data["sleeper_transforms"],
        )
        materials = data["materials"]
    shapes, owners, kinds, maximum_gauge, maximum_box = [], [], [], 0.0, 0.0
    v, f, t = 0, 0, 0
    accepted = [r for r in row["pieces"] if r["valid"]]
    for index, piece in enumerate(accepted):
        nv, nf, count = (
            piece["rail_vertices"],
            piece["rail_triangles"],
            piece["sleepers"],
        )
        nvertices, nfaces = nv + 8 * count, nf + 12 * count
        shapes_here, axis, gauge_error, box_error = verify_piece(
            vertices[v : v + nvertices],
            faces[f : f + nfaces] - v,
            transforms[t : t + count],
            piece,
            row["horizontal_bed_datum_m"],
        )
        if (
            not shapely.covers(bed, shapes_here).all()
            or axis.difference(supported_axes).length > 1e-7
        ):
            raise ValueError("whole hardware footprint or original rail axis lost")
        if not (
            np.all(materials[f : f + nf] == 6)
            and np.all(materials[f + nf : f + nfaces] == 7)
        ):
            raise ValueError("rail/sleeper material binding changed")
        shapes.extend(shapes_here)
        owners.extend([index] * len(shapes_here))
        kinds.extend([0, 0] + [1] * (len(shapes_here) - 2))
        maximum_gauge, maximum_box = (
            max(maximum_gauge, gauge_error),
            max(maximum_box, box_error),
        )
        v, f, t = v + nvertices, f + nfaces, t + count
    if (v, f, t) != (len(vertices), len(faces), len(transforms)):
        raise ValueError("saved hardware export incomplete or has extra payload")
    audit = cross_piece_audit(
        np.array(shapes, object), np.array(owners), np.array(kinds), graphs
    )
    return {
        "valid": True,
        "accepted_pieces": len(accepted),
        "complete_footprints": len(shapes),
        "sleeper_instances": t,
        "actual_bed_domain_error_m2": bed_error,
        "maximum_gauge_error_m": maximum_gauge,
        "maximum_independent_instance_vertex_error_m": maximum_box,
        **audit,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--camera-products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allow-dirty", action="store_true")
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh external verification output required")
    receipt = json.loads((args.products / "generation.json").read_text())
    if receipt["dirty"] and not args.allow_dirty:
        raise ValueError("clean frozen hardware generation required")
    if not args.allow_dirty:
        files = {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("rail_*.py")
        }
        if files != receipt["implementation_sha256"]:
            raise ValueError("fresh complete implementation provenance required")
        libraries = {
            "rail_hardware/" + p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(rail_library.__file__).parent.glob("*.py")
        }
        libraries["contracts.py"] = hashlib.sha256(
            Path(contracts.__file__).read_bytes()
        ).hexdigest()
        if libraries != receipt["library_sha256"]:
            raise ValueError("actual imported library differs from hardware generation")
    cases = {
        name: verify_case(name, args.products, args.camera_products, row)
        for name, row in receipt["cases"].items()
    }
    args.output.write_text(
        json.dumps(
            {
                "generation_commit": receipt["generation_commit"],
                "generation_sha256": hashlib.sha256(
                    (args.products / "generation.json").read_bytes()
                ).hexdigest(),
                "diagnostic_dirty_allowed": args.allow_dirty,
                "cases": cases,
            },
            indent=2,
        )
        + "\n"
    )
    print(json.dumps(cases), flush=True)


if __name__ == "__main__":
    main()
