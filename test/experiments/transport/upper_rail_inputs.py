"""Explicit original subway bridge axes and the verified actual upper deck."""

import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from capture_tunnel_portals import base_surfaces
from rail_graph_inputs import safe_pieces
from wien_support_experiment import BRIDGE_PIN

HERE = Path(__file__).resolve().parent


def inputs(reference):
    path = (
        reference / "bridge-continuations/Vienna-continuation-whole-rebuild-solid.npz"
    )
    (deck, _old_ground), binding = base_surfaces(path)
    fixture = HERE / "fixtures/bridge-continuations-vienna.json"
    if digest(fixture) != BRIDGE_PIN:
        raise ValueError("unchanged original bridge source fixture required")
    document = json.loads(fixture.read_text())
    sources = [r for r in document["sources"] if r["role"] == "deck"]
    if not sources or any(
        r["properties"].get("class") != "transit"
        or r["properties"].get("subclass") != "subway"
        for r in sources
    ):
        raise ValueError("explicit chosen upper subway source axes required")
    raw = {r["source_id"]: r for r in sources}
    chains = [{"xy": np.array(r["xy"]), "sources": [r["source_id"]]} for r in sources]
    pieces, ledger, work = safe_pieces(chains, np.empty((0, 2)), [], 1.35)
    return (
        deck,
        pieces,
        raw,
        ledger,
        work,
        dict(
            binding,
            fixture_sha256=BRIDGE_PIN,
            origin_lat_lon=document["origin_lat_lon"],
            original_source_count=len(sources),
            original_axis_length_m=sum(
                np.linalg.norm(np.diff(c["xy"], axis=0), axis=1).sum() for c in chains
            ),
        ),
    )
