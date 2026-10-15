"""Compile explicit species requests from a pinned licensed WCVP ZIP snapshot."""

import argparse
import csv
import hashlib
import io
import json
import zipfile
from pathlib import Path

NAME_FIELDS = (
    "plant_name_id",
    "taxon_name",
    "taxon_rank",
    "taxon_status",
    "family",
    "lifeform_description",
    "climate_description",
    "accepted_plant_name_id",
    "ipni_id",
    "powo_id",
    "taxon_authors",
)
DIST_FIELDS = (
    "plant_locality_id",
    "plant_name_id",
    "area_code_l3",
    "introduced",
    "extinct",
    "location_doubtful",
)


def rows(archive, member, required, maximum):
    if archive.namelist().count(member) != 1:
        raise ValueError("one explicit WCVP member required")
    info = archive.getinfo(member)
    if info.file_size > maximum:
        raise ValueError("WCVP member exceeds input budget")
    with archive.open(member) as stream:
        reader = csv.DictReader(
            io.TextIOWrapper(stream, encoding="utf-8-sig", newline=""),
            delimiter="|",
            quoting=csv.QUOTE_NONE,
        )
        if len(reader.fieldnames or ()) != len(set(reader.fieldnames or ())):
            raise ValueError("duplicate WCVP columns")
        if not set(required) <= set(reader.fieldnames or ()):
            raise ValueError("required WCVP columns missing")
        for row in reader:
            if None in row or any(row[k] is None for k in required):
                raise ValueError("malformed WCVP row")
            yield row


def resolve_names(archive, requested, name_ids):
    matches = {name: [] for name in requested}
    for row in rows(archive, "wcvp_names.csv", NAME_FIELDS, 512 << 20):
        if row["taxon_name"] in matches:
            matches[row["taxon_name"]].append(row)
    chosen = {}
    for name, values in matches.items():
        if name in name_ids:
            values = [row for row in values if row["plant_name_id"] == name_ids[name]]
        if len(values) != 1:
            raise ValueError(
                f"exact name must resolve uniquely: {name} ({len(values)})"
            )
        row = values[0]
        if row["taxon_status"] not in ("Accepted", "Synonym"):
            raise ValueError(f"unaccepted request: {name}: {row['taxon_status']}")
        accepted = row["accepted_plant_name_id"]
        if not accepted:
            raise ValueError(f"accepted identifier missing: {name}")
        chosen[name] = row
    needed = {row["accepted_plant_name_id"] for row in chosen.values()}
    accepted = {
        row["plant_name_id"]: row
        for row in chosen.values()
        if row["taxon_status"] == "Accepted"
    }
    if needed - accepted.keys():
        for row in rows(archive, "wcvp_names.csv", NAME_FIELDS, 512 << 20):
            if row["plant_name_id"] in needed:
                accepted[row["plant_name_id"]] = row
    if needed != accepted.keys() or any(
        row["taxon_status"] != "Accepted"
        or row["taxon_rank"] != "Species"
        or row["accepted_plant_name_id"] != key
        for key, row in accepted.items()
    ):
        raise ValueError("requests need self-consistent accepted species records")
    return chosen, accepted


def distributions(archive, accepted):
    groups = {key: {} for key in accepted}
    for row in rows(archive, "wcvp_distribution.csv", DIST_FIELDS, 256 << 20):
        if row["plant_name_id"] not in groups:
            continue
        fields = (row["introduced"], row["extinct"], row["location_doubtful"])
        if any(x not in ("0", "1") for x in fields):
            raise ValueError("explicit binary distribution flags required")
        code = row["area_code_l3"]
        if (
            len(code) != 3
            or not code.isascii()
            or not code.isupper()
            or not code.isalpha()
        ):
            raise ValueError("explicit WGSRPD level3 region required")
        key = (code, *map(int, fields))
        groups[row["plant_name_id"]].setdefault(key, []).append(
            row["plant_locality_id"]
        )
    return {
        key: [
            dict(
                zip(("region", "introduced", "extinct", "doubtful"), flags),
                source_ids=sorted(values),
            )
            for flags, values in sorted(group.items())
        ]
        for key, group in groups.items()
    }


def compile_catalog(source, specification):
    if source.stat().st_size > 128 << 20:
        raise ValueError("WCVP ZIP exceeds input budget")
    with source.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != specification["source"]["sha256"]:
        raise ValueError("WCVP source differs from explicit pinned snapshot")
    requested = specification["requested_names"]
    if not 1 <= len(requested) <= 2048 or len(set(requested)) != len(requested):
        raise ValueError("bounded unique exact species requests required")
    name_ids = specification.get("name_ids", {})
    if not isinstance(name_ids, dict) or not name_ids.keys() <= set(requested):
        raise ValueError("disambiguation must refer to requested exact names")
    with zipfile.ZipFile(source) as archive:
        matched, accepted = resolve_names(archive, requested, name_ids)
        regions = distributions(archive, accepted)
    records = []
    for key, row in sorted(accepted.items(), key=lambda item: item[1]["taxon_name"]):
        records.append(
            {
                "id": "outshine:taxon:wcvp:" + key,
                "scientific_name": row["taxon_name"],
                "family": row["family"],
                "lifeform": row["lifeform_description"],
                "climate": row["climate_description"],
                "wcvp_id": key,
                "ipni_id": row["ipni_id"],
                "powo_id": row["powo_id"],
                "authors": row["taxon_authors"],
                "distribution": regions[key],
                "recipe_id": None,
                "visually_accepted": False,
            }
        )
    return {
        "format": "outshine-species-pilot-1",
        "source": specification["source"],
        "scope": "Explicit pilot species, not full flora or ecological/visual acceptance. Empty traits mean missing source data. Distribution flags are independent evidence, not abundance or cultivated planting instructions.",
        "requests": [
            {
                "requested_name": name,
                "source_name_id": row["plant_name_id"],
                "source_status": row["taxon_status"],
                "accepted_id": "outshine:taxon:wcvp:" + row["accepted_plant_name_id"],
            }
            for name, row in sorted(matched.items())
        ],
        "species": records,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--specification", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    catalog = compile_catalog(args.source, json.loads(args.specification.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n")
    print(
        json.dumps(
            {
                "species": len(catalog["species"]),
                "requests": len(catalog["requests"]),
                "bytes": args.output.stat().st_size,
            }
        )
    )
