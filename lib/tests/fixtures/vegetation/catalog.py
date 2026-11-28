"""Vegetation corpus definitions and pinned Python reference views."""

import json
from pathlib import Path

from ..vienna.catalog import ROOT, available_views

MANIFEST = Path(__file__).with_name("pilot.json")
DEVELOPMENT = MANIFEST.with_name("development.json")
HOLDOUTS = MANIFEST.with_name("holdouts.json")
SOURCE_CACHE = ROOT / "build/vegetation-pilot-sources"

CONTINENTS = (
    "Africa",
    "Asia",
    "Europe",
    "North America",
    "Oceania",
    "South America",
)


def read_site_records(path):
    document = json.loads(path.read_text())
    if document["version"] != 1:
        raise ValueError(f"unsupported vegetation corpus version in {path.name}")
    return document["sites"]


def load_sites(*, pilot_only=False):
    records = read_site_records(MANIFEST)
    if not pilot_only:
        records.extend(read_site_records(DEVELOPMENT))
    sites = {site["id"]: site for site in records}
    if len(sites) != len(records):
        raise ValueError("duplicate vegetation development site ID")
    return sites


def load_holdout_anchors():
    return read_site_records(HOLDOUTS)


def site_views(site, vienna):
    config = vienna
    if "capture" in site:
        path = MANIFEST.parent / site["capture"]
        config = json.loads(path.read_text())
        config["source_pins"] = json.loads(
            path.with_name(config["source_pins"]).read_text()
        )
        config["source_cache"] = str(SOURCE_CACHE)
    by_id = {view["id"]: view for view in available_views(config)}
    return [(by_id[name], config) for name in site["views"]]
