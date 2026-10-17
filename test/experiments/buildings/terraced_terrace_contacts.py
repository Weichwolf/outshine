"""Compare contact resolution on original source setback polygons at equal bounds."""

import argparse
import json
import time
from pathlib import Path

import manifold3d as manifold
import numpy as np
import shapely
from evidence import revision
from outshine_ref.buildings.modelling import PrismVolume
from outshine_ref.buildings.modelling.prism_union import primitive_mesh
from structural_shell_geometry import native
from terraced_facade_surface import polygon_parts
from terraced_mass_plan import plan as mass_plan
from terraced_mass_sources import sources


def cross_section(polygon):
    rings = [
        np.asarray(r.coords)[:-1]
        for p in polygon_parts(polygon)
        for r in (p.exterior, *p.interiors)
    ]
    result = manifold.CrossSection(rings, manifold.FillRule.EvenOdd).simplify(1e-7)
    output = shapely.Polygon()
    for ring in sorted(result.to_polygons(), key=lambda r: -shapely.Polygon(r).area):
        output = output.symmetric_difference(shapely.Polygon(ring))
    return output


def methods(polygon, method):
    if method == "raw":
        return polygon
    value = shapely.set_precision(polygon, 1e-10)
    if method == "grid":
        return value
    if method == "open-mitre":
        return value.buffer(-1e-6, join_style="mitre").buffer(1e-6, join_style="mitre")
    return cross_section(polygon)


def compare():
    rows = []
    for index, row in enumerate(sources()):
        for kind in ("cascade", "paired-wings"):
            mass = mass_plan(row, kind)
            for i, s in enumerate(mass.storeys):
                for j, original in enumerate(
                    polygon_parts(shapely.from_wkb(s.terrace_wkb))
                ):
                    if original.area <= 1e-8:
                        continue
                    for method in ("raw", "grid", "open-mitre", "cross-section"):
                        start = time.perf_counter()
                        polygon = methods(original, method)
                        vertices, faces = primitive_mesh(
                            PrismVolume("contact", polygon, 0, 0.16)
                        )
                        solid = native({"vertices": vertices, "faces": faces})
                        expected = 2 * polygon.area + polygon.length * 0.16
                        rows.append(
                            {
                                "source": index,
                                "programme": kind,
                                "storey": i,
                                "component": j,
                                "method": method,
                                "source_id": mass.identifier,
                                "source_area_xor_m2": original.symmetric_difference(
                                    polygon
                                ).area,
                                "volume_error_m3": abs(
                                    solid.volume() - polygon.area * 0.16
                                ),
                                "surface_error_m2": abs(
                                    solid.surface_area() - expected
                                ),
                                "triangles": len(faces),
                                "input_coordinates": len(
                                    shapely.get_coordinates(polygon)
                                ),
                                "seconds": time.perf_counter() - start,
                            }
                        )
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        parser.error("Freeze contact controls before evidence")
    rows = compare()
    summaries = []
    for method in ("raw", "grid", "open-mitre", "cross-section"):
        values = [r for r in rows if r["method"] == method]
        summaries.append(
            {
                "method": method,
                "cases": len(values),
                "failures": sum(
                    r["source_area_xor_m2"] > 1e-7
                    or r["surface_error_m2"] > 1e-7
                    or r["volume_error_m3"] > 1e-7
                    for r in values
                ),
                "max_xor_m2": max(r["source_area_xor_m2"] for r in values),
                "max_surface_error_m2": max(r["surface_error_m2"] for r in values),
                "seconds": sum(r["seconds"] for r in values),
            }
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(proof | {"records": rows, "summary": summaries}, indent=2) + "\n"
    )
    print(json.dumps(summaries))


if __name__ == "__main__":
    main()
