"""Account for unlike original-triangle and complete-domain representations.

These reported payloads are not interchangeable complete assets or device memory.
"""

import argparse
import hashlib
import json
from pathlib import Path


def load(path):
    return json.loads(path.read_text())


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args):
    if args.output.exists():
        raise ValueError("Fresh explicit output required")
    attribute, domain = load(args.attribute_report), load(args.domain_report)
    fixture, binding = load(args.native_selection), load(args.source_binding)
    if (
        fixture["source_geometry_manifest_sha256"]
        != binding["original_geometry"]["sha256"]
        or fixture["world_manifest_sha256"] != attribute["input_manifest_sha256"]
        or digest(args.source_binding)
        != domain["input_provenance"]["source_binding_sha256"]
        or attribute["original_geometry"]["source_count"]
        != domain["summary"]["whole_sources"]
        or attribute["original_geometry"]["triangles"]
        != domain["input_provenance"]["triangles"]
        or not attribute["variants"]["adaptive"]["whole_words_equal"]
    ):
        raise ValueError(
            "Same bound original source/geometry and exact attributes required"
        )
    geometry = attribute["shared_geometry_payload_bytes"]
    fields = attribute["variants"]["adaptive"]["attribute_payload_bytes"]
    recipe = domain["summary"]["retained_raw_recipe_and_source_id_bytes"]
    report = {
        "source_count": domain["summary"]["whole_sources"],
        "original_triangles": domain["input_provenance"]["triangles"],
        "source_geometry_manifest_sha256": binding["original_geometry"]["sha256"],
        "input_sha256": {
            str(path): digest(path)
            for path in (
                args.attribute_report,
                args.domain_report,
                args.native_selection,
                args.source_binding,
            )
        },
        "exact_triangle_query": {
            "geometry_and_acceleration_bytes": geometry,
            "adaptive_attribute_bytes": fields,
            "numeric_payload_without_recipes_or_source_bindings_bytes": geometry
            + fields,
            "plus_same_enriched_recipe_bytes_lower_bound": geometry + fields + recipe,
        },
        "constructive_domains": {
            "domain_arrays_bytes": domain["summary"]["domain_array_bytes"],
            "raw_recipes_and_membership_bytes": recipe,
            "three_original_fallback_arrays_bytes": domain["summary"][
                "fallback_array_bytes"
            ],
            "encoded_payload_bytes": domain["summary"]["total_encoded_payload_bytes"],
        },
        "triangle_query_native_geometry_mask_differences": {
            pose: row["native_geometry_mask_differences"]
            for pose, row in attribute["poses"].items()
        },
        "scope": "Both reports use the same bound original geometry, but represent different data and semantics. Triangle query retains original fine geometry and its column acceleration plus exact attributes, yet excludes raw enriched recipes and per-face source binding; adding the other case's recipe bytes is only a lower bound for a complete comparable asset. Domain payload includes recipes and three original fallbacks but lacks a qualified complete native-equivalent geometry/material/raster query or its acceleration. Existing triangle field itself has finite native mask differences. No same-quality winner, physical IO, allocations, peak memory, CPU/GPU/device or World acceptance follows from these sums.",
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in (
        "attribute-report",
        "domain-report",
        "native-selection",
        "source-binding",
        "output",
    ):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())
