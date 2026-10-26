"""Original-water adapter for the finite update experiment, not a world recipe."""

import dataclasses

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.places.camera import ecef
from outshine_ref.places.sources import tile_geography
from physical_boundary import from_world
from water_colored_updates import ColoredUpdates, UpdatedNode, preflight
from water_contact_band import physical_audit
from water_nodal_height import Node, audit_face, evaluate_node


class ReferenceUpdates:
    def __init__(
        self, receipts, epoch, snapshots, data, order, *, parent_level_m, parent_version
    ):
        if not np.isfinite(parent_level_m):
            raise ValueError("finite explicit laboratory parent level required")
        self.receipts, self.epoch = receipts, epoch
        self.snapshots, self.data, self.order = snapshots, data, order
        if not isinstance(parent_version, str) or not parent_version:
            raise ValueError("explicit parent recipe version required")
        self.parent_level_m = float(parent_level_m)
        self.parent_recipe_version = parent_version
        self.parent_version = f"{parent_version}:ASL:{self.parent_level_m.hex()}"
        self.targets, self.faces, self.plans = {}, {}, {}
        self.field = ColoredUpdates(self.target, self.parent, self.admissible)
        self.work = {
            "source_face_preparations": 0,
            "physical_audits": 0,
            "preflight_receipt_lookups": 0,
        }

    def parent(self, key):
        z, x, y = key
        lat, lon = tile_geography([[x / 2, y / 2]], 1, (z, 0, 0))
        return ecef(lat[0], lon[0], self.parent_level_m)

    def target(self, key):
        if key not in self.targets:
            if len(self.targets) >= 512:
                return Node(key, "budget_exhausted", None, None, None, (), {})
            self.targets[key] = evaluate_node(
                key, self.receipts, self.epoch, self.snapshots, self.data, self.order
            )
        return self.targets[key]

    def _source_face(self, keys):
        if keys not in self.faces:
            if len(self.faces) >= 2048:
                return "budget_exhausted", None
            self.work["source_face_preparations"] += 1
            nodes = {key: self.target(key) for key in keys}
            if any(n.state != "ready" for n in nodes.values()):
                self.faces[keys] = (
                    next(n.state for n in nodes.values() if n.state != "ready"),
                    None,
                )
            else:
                baseline = {
                    key: dataclasses.replace(n, world=self.parent(key))
                    for key, n in nodes.items()
                }
                _, arrays = audit_face(keys, baseline, self.data)
                self.faces[keys] = (
                    "ready",
                    (arrays["anchor"], arrays["rows"], arrays["bounds"]),
                )
                self.work["physical_audits"] += 1
        return self.faces[keys]

    def admissible(self, keys, world):
        state, constraint = self._source_face(keys)
        if state != "ready":
            return state
        anchor, rows, bounds = constraint
        mesh = Mesh(from_world(anchor, world), np.array([[0, 1, 2]], np.uint32))
        self.work["physical_audits"] += 1
        return (
            "admitted"
            if physical_audit(mesh, anchor, rows, bounds)["admitted"]
            else "physical_rejection"
        )

    def resolve(self, key):
        if key not in self.plans:
            self.plans[key] = preflight(
                key, self.receipts, self.epoch, self.parent_version
            )
            self.work["preflight_receipt_lookups"] += self.plans[key]["receipt_lookups"]
        plan = self.plans[key]
        if plan["state"] != "ready":
            return UpdatedNode(plan["state"], None, None)
        return self.field.node(key)
