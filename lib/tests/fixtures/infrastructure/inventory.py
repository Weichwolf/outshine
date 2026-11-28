"""Audit the layers and attributes delivered with the pinned infrastructure corpus."""

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path

from outshine.places.sources import acquire, decode_vector

from .catalog import family

LAYER_OWNERS = {
    "aerodrome_label": ("label", "#2220"),
    "aeroway": ("aviation geometry", "#2220"),
    "boundary": ("mapped context", "#2211"),
    "building": ("building geometry", "#2212"),
    "housenumber": ("label", "#2212"),
    "landcover": ("cover and material context", "#2211"),
    "landuse": ("ground and facility context", "#2211"),
    "mountain_peak": ("terrain reference and label", "#2211"),
    "park": ("mapped cover and access context", "#2211"),
    "place": ("label", "#2210"),
    "poi": ("mapped objects and use evidence", "#2211"),
    "transportation": ("route geometry and access", "#2216"),
    "transportation_name": ("route label and designation", "#2216"),
    "water": ("water geometry and contact", "#2211"),
    "water_name": ("label", "#2211"),
    "waterway": ("water courses and drainage", "#2211"),
}

FAMILY_OWNERS = {
    "bridge": "#2219",
    "cableway": "#2218",
    "path": "#2217",
    "port": "#2221",
    "rail": "#2218",
    "road": "#2217",
    "runway": "#2220",
    "station": "#2218",
    "steps": "#2217",
    "taxiway": "#2220",
    "tunnel": "#2219",
    "waterway": "#2211",
}


def audit(catalog, cache):
    counts = Counter()
    keys = defaultdict(set)
    vocabulary = defaultdict(Counter)
    roles = defaultdict(Counter)
    for source in catalog["sources"]["vector"]:
        record = acquire(("vector", source["tile"], source["url"]), cache)
        if record["sha256"] != source["sha256"]:
            raise ValueError(f"changed provider tile: {source['tile']}")
        for layer, content in decode_vector(record).items():
            for feature in content["features"]:
                props = feature["properties"]
                counts[layer] += 1
                keys[layer].update(props)
                for label in ("class", "subclass"):
                    if label in props:
                        vocabulary[layer][f"{label}={props[label]}"] += 1
                if kind := family(layer, props):
                    roles[layer][kind] += 1
    return {
        "snapshot": catalog["source_snapshot"],
        "vector_tiles": len(catalog["sources"]["vector"]),
        "layers": {
            layer: {
                "features_including_tile_buffer_duplicates": counts[layer],
                "attributes": sorted(keys[layer]),
                "values": dict(sorted(vocabulary[layer].items())),
                "use_and_issue": LAYER_OWNERS.get(layer, ("review required", "#2210")),
                "infrastructure_families": {
                    kind: [count, FAMILY_OWNERS[kind]]
                    for kind, count in sorted(roles[layer].items())
                },
            }
            for layer in sorted(counts)
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", type=Path)
    parser.add_argument("--cache", type=Path, default=Path("build/reference-sources"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(json.loads(args.catalog.read_text()), args.cache)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n")
    print(result["vector_tiles"], "tiles;", len(result["layers"]), "layers")


if __name__ == "__main__":
    main()
