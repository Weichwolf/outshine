"""Inspect user-provided procedural references without importing their assets."""

import argparse
import gzip
import hashlib
import json
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

MODELS = (
    "samples/Games/Broadleaf/Oak_Sapling.spm",
    "samples/Games/Broadleaf/Oak_Desktop_Forest.spm",
    "samples/Games/Broadleaf/Oak_Hero_Forest.spm",
    "samples/Games/Broadleaf/Oak_English_Cluster_Branch.spm",
    "samples/Games/Conifer/Conifer.spm",
    "samples/Games/Palm/Palm_Desktop.spm",
    "samples/Games/Bush/Bush_Desktop.spm",
    "tree_templates/Games/Grass.spm",
)


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def inspect_model(path):
    with gzip.open(path, "rb") as source:
        raw = source.read(128 * 1024 * 1024 + 1)
    if len(raw) > 128 * 1024 * 1024:
        raise ValueError(f"reference XML exceeds 128 MiB: {path}")
    root = ET.fromstring(raw)
    generators = root.findall("Generators/Generator")
    index = {g.findtext("GUID"): i for i, g in enumerate(generators)}
    edges, unresolved = [], 0
    for link in root.findall("Links/Link"):
        source, target = (
            index.get(link.findtext("SourceGUID")),
            index.get(link.findtext("TargetGUID")),
        )
        if source is None or target is None:
            unresolved += 1
        else:
            edges.append([source, target])
    graph = []
    for g in generators:
        properties = {
            p.findtext("Name"): p
            for p in g.find("Properties")
            if p.findtext("Name") is not None
        }
        selected = {}
        for key in (
            "Generation:Mode",
            "Generation:First",
            "Generation:Last",
            "Generation:Phyllotaxy:Arrangement:Style",
            "Generation:Phyllotaxy:Nodes:Internode length",
            "Generation:Interval:Frequency",
            "Generation:Interval:Count",
            "Spine:Length:+ % of parent",
            "Wind:Apply",
        ):
            if key in properties:
                p = properties[key]
                selected[key] = {
                    "value": p.findtext("Value"),
                    "variance": p.findtext("Variance"),
                    "parent_curve_points": len(
                        p.findall(".//CompoundParentSpline/Spline/ControlPoint")
                    ),
                }
        graph.append(
            {"name": g.findtext("Name"), "type": g.get("Type"), "properties": selected}
        )
    meshes = []
    for mesh in root.findall("Assets/Mesh"):
        data = next(
            (
                c
                for c in mesh
                if c.tag.startswith("EmbeddedData") and "NumVertices" in c.attrib
            ),
            None,
        )
        meshes.append(
            {
                "name": mesh.get("Name"),
                "id": mesh.get("ID"),
                "vertices": int(data.get("NumVertices", "0"))
                if data is not None
                else None,
                "triangles": int(data.get("NumIndices", "0")) // 3
                if data is not None
                else None,
                "anchors": int(data.get("NumAnchors", "0"))
                if data is not None
                else None,
                "more_lods_mesh_id": mesh.findtext("MoreLodsMeshID"),
            }
        )
    stats = root.findtext("Statistics/TotalTriangles")
    return {
        "model_sha256": digest(path),
        "scientific_names": [
            item.get("Value", "")
            for item in root.findall('TreeInfo/Item[@Key="ScientificNames"]')
        ],
        "modeler_version": root.get("VersionString", "").strip(),
        "scene_unit_scale_enum": root.findtext("SceneUnitScale"),
        "stored_total_triangles": int(stats) if stats is not None else None,
        "stored_nodes_by_type": dict(
            Counter(n.get("Type") for n in root.findall("Nodes/Node"))
        ),
        "generators": graph,
        "edges": edges,
        "unresolved_edges": unresolved,
        "mesh_assets": meshes,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-directory", type=Path, required=True)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/vegetation/speedtree/reference.json"),
    )
    parser.add_argument(
        "--templates",
        action="store_true",
        help="inspect every bundled Games/VFX generator template",
    )
    args = parser.parse_args()
    models = (
        sorted(
            path.relative_to(args.reference_directory).as_posix()
            for path in (args.reference_directory / "tree_templates").rglob("*.spm")
        )
        if args.templates
        else MODELS
    )
    records = {}
    for name in models:
        records[name] = inspect_model(args.reference_directory / name)
        item = records[name]
        print(
            f"{Path(name).stem}: {len(item['generators'])} generators, {item['stored_total_triangles']} stored triangles"
        )
    report = {
        "reference_directory": str(args.reference_directory.resolve()),
        "kind": "serialized reference metadata, no executed SpeedTree benchmark",
        "node_semantics": "Leaf Mesh nodes may represent clusters rather than individual leaves",
        "models": records,
        "wind_implementation_sha256": digest(
            args.reference_directory / "standard_shaders/SpeedTreeWind.h"
        ),
        "modeler_binary_sha256": digest(
            args.reference_directory / "linux/SpeedTree_Modeler"
        ),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
