"""Pilot site definitions backed by pinned Python reference views."""

import json
from pathlib import Path

from ..vienna.catalog import ROOT, available_views

MANIFEST = Path(__file__).with_name("pilot.json")


def load_sites():
    document = json.loads(MANIFEST.read_text())
    if document["version"] != 1:
        raise ValueError("unsupported vegetation corpus version")
    sites = {site["id"]: site for site in document["sites"]}
    if len(sites) != len(document["sites"]):
        raise ValueError("duplicate vegetation corpus site ID")
    return sites


def site_views(site, vienna):
    config = vienna
    if "capture" in site:
        path = MANIFEST.parent / site["capture"]
        config = json.loads(path.read_text())
        config["source_pins"] = json.loads(
            path.with_name(config["source_pins"]).read_text()
        )
        config["source_cache"] = str(ROOT / "build/vegetation-pilot-sources")
    by_id = {view["id"]: view for view in available_views(config)}
    return [(by_id[name], config) for name in site["views"]]
