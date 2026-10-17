"""Isolated, bounded independent rational/native audit of empty affine masks."""

import argparse
import importlib.metadata
import json
import resource
import time
from fractions import Fraction
from pathlib import Path

from camera_height_inputs import digest
from stair_occupancy_geometry import AffineOccupancy, OccupancyError
from stair_occupancy_oracle import native_area, native_source, oracle_geometry
from stair_occupancy_precision import exact_band_area
from stair_occupancy_precision_cases import actual_cases, controls
from stair_terrain_experiment import frozen


def check(case):
    from pysfcgal.geometry import Geometry

    name, kind, row, heights, footprint, cells = case
    began = time.perf_counter()
    if cells:
        native, reference = None, Fraction(0)
        for cell in cells:
            part = oracle_geometry(row, heights, domain=cell)
            reference += Fraction(0) if part is None else native_area(part)
    else:
        native = oracle_geometry(row, heights)
        reference = Fraction(0) if native is None else native_area(native)
    native_seconds = time.perf_counter() - began
    began = time.perf_counter()
    exact = exact_band_area(row, heights)
    exact_seconds = time.perf_counter() - began
    if exact != reference:
        raise ValueError("independent native/rational area disagreement: " + name)
    began = time.perf_counter()
    failure = None
    try:
        shape = row.conflict_geometry(heights)
    except OccupancyError as error:
        shape, failure = None, str(error)
    mask_seconds = time.perf_counter() - began
    computed = (
        Fraction(0)
        if shape is None or shape.is_empty
        else native_area(Geometry.from_wkb(shape.wkb))
    )
    if shape is not None and ((computed > 0) != (reference > 0)):
        raise ValueError(
            "floating mask silently changed positive/empty decision: " + name
        )
    result = {
        "case": name,
        "kind": kind,
        "native_reference_domain": "disjoint_integer_control_cells"
        if cells
        else "original_polygon",
        "native_area_m2_rational": str(reference),
        "computed_area_m2_rational": str(computed),
        "deferred": failure,
        "absolute_area_error_m2": float(abs(computed - reference))
        if shape is not None
        else None,
        "native_seconds": native_seconds,
        "always_exact_area_seconds": exact_seconds,
        "guarded_mask_seconds": mask_seconds,
    }
    if footprint is not None:
        reference_cut = (
            Fraction(0)
            if native is None or native.is_empty
            else native_area(native.intersection(native_source(footprint.geometry)))
        )
        if shape is None:
            result["footprint_deferred"] = True
        else:
            cut = shape.intersection(footprint.geometry)
            if (cut.area > 0) != (reference_cut > 0):
                raise ValueError("actual footprint conflict silently changed: " + name)
            result["footprint_positive_conflict"] = reference_cut > 0
    return result


def underflow_case():
    import shapely

    length = 1e-170
    shape = shapely.box(0, 0, length, length)
    exact = native_area(native_source(shape))
    if exact != Fraction(length) ** 2 or exact <= 0 or shape.area != 0:
        raise ValueError("independent positive source-area underflow control failed")
    try:
        AffineOccupancy("underflow", shape, "control", (0, 0), (100, 0, 0), (101, 0, 0))
    except OccupancyError as error:
        return {
            "source_wkb": shape.wkb_hex,
            "native_area_m2_rational": str(exact),
            "binary64_area_m2": shape.area,
            "explicit_rejection": str(error),
        }
    raise ValueError("underflowed positive source domain was silently admitted")


def run(args):
    import pysfcgal
    import pysfcgal._sfcgal as extension

    commit = frozen()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private precision proof outputs must live under /tmp")
    args.output.mkdir(parents=True, exist_ok=False)
    began = time.perf_counter()
    underflow = underflow_case()
    actual, face_count, query_count = actual_cases(
        args.products, facet_limit=args.facet_limit
    )
    cases = [*controls(), *actual]
    records = []
    for i, case in enumerate(cases):
        records.append(check(case))
        if (i + 1) % 1000 == 0:
            print(i + 1, "exact native/rational/mask cases verified", flush=True)
    if len({r["case"] for r in records}) != len(records):
        raise ValueError("precision case identity collision")
    ledger = args.output / "cases.json"
    ledger.write_text(json.dumps(records, indent=2) + "\n")
    native_library = next(
        (Path(pysfcgal.__file__).parent.parent / "pysfcgal.libs").glob("libSFCGAL*.so*")
    )
    report = {
        "commit": commit,
        "products_report_sha256": digest(args.products / "comparison.json"),
        "saved_volume_sha256": digest(args.products / "volumes.json"),
        "source_volume_count": face_count,
        "actual_query_count": query_count,
        "actual_facet_slice_count": min(args.facet_limit, face_count),
        "checked_cases": len(records),
        "case_ledger_sha256": digest(ledger),
        "deferred_positive_cases": [r["case"] for r in records if r["deferred"]],
        "maximum_absolute_mask_area_error_m2": max(
            r["absolute_area_error_m2"] or 0 for r in records
        ),
        "native_seconds": sum(r["native_seconds"] for r in records),
        "always_exact_area_seconds": sum(
            r["always_exact_area_seconds"] for r in records
        ),
        "guarded_mask_seconds": sum(r["guarded_mask_seconds"] for r in records),
        "pysfcgal": importlib.metadata.version("pysfcgal"),
        "sfcgal_full_version": pysfcgal.sfcgal_full_version(),
        "native_library": str(native_library),
        "native_library_sha256": digest(native_library),
        "binding_sha256": digest(Path(extension.__file__)),
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "all_independent_exact_areas_equal": True,
        "all_returned_empty_decisions_verified": True,
        "source_area_underflow_control": underflow,
        "scope": "Own ring integrals vs independent pair-intersection window plus optional SFCGAL rational WKT oracle. Actual-height masks and controlled slices of actual source facets distinguished. Binary64 nonempty mask boundaries/area remain approximate; footprint predicates verified only for actual-height cases. No native/runtime/SDK dependency added.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--facet-limit", type=int, default=8192)
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    arguments = parser.parse_args()
    if not 0 <= arguments.facet_limit <= 8192:
        parser.error("--facet-limit must be between 0 and 8192")
    run(arguments)
