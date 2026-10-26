"""One finite, immutable three-colour sweep from an admitted parent field.

Each face contains all three colours. A phase can therefore change at most one
of its vertices. Checking every incident face before accepting a move preserves
admission without convergence or simultaneous-neighbour assumptions. This does
not construct the globally owned parent or prove that its soft targets are true.
"""

import hashlib
import json
from dataclasses import dataclass
from functools import lru_cache

import numpy as np
from water_nodal_height import cell_faces, node_plan


@lru_cache(maxsize=4096)
def incident_faces(key):
    z, x, y = key
    cells = {(z, a // 2, b // 2) for a in (x - 1, x) for b in (y - 1, y)}
    return tuple(sorted({f for c in cells for f in cell_faces(c) if key in f}))


@lru_cache(maxsize=4096)
def dependencies(key, phase=2):
    if type(phase) is not int or not -1 <= phase <= 2:
        raise ValueError("one fixed three-colour sweep required")
    if phase == -1:
        return frozenset()
    if (key[1] + key[2]) % 3 != phase:
        return dependencies(key, phase - 1)
    neighbours = {p for face in incident_faces(key) for p in face}
    return frozenset(neighbours).union(
        *(dependencies(p, phase - 1) for p in neighbours)
    )


def preflight(key, receipts, epoch, parent_version):
    """Only metadata, before target projection/profile construction or updates."""
    if not isinstance(parent_version, str) or not parent_version:
        raise ValueError("explicit immutable parent version required")
    if (
        not isinstance(key, tuple)
        or len(key) != 3
        or any(type(v) is not int for v in key)
    ):
        raise ValueError("integer world node key required")
    z, x, y = key
    if not 0 <= z <= 22 or min(x, y) < 0 or max(x, y) > 2 * (1 << z):
        raise ValueError("bounded world node key required")
    if min(x, y) < 3 or max(x, y) > 2 * (1 << z) - 4:
        return {
            "state": "needs_projection_contacts",
            "version": None,
            "dependency_nodes": (),
            "required": (),
            "missing": (),
            "receipt_lookups": 0,
        }
    plans = {p: node_plan(p, receipts, epoch) for p in sorted(dependencies(key))}
    missing = sorted({k for p in plans.values() for k in p.missing})
    state = next((p.state for p in plans.values() if p.state != "ready"), "ready")
    version = None
    if state == "ready":
        version = hashlib.sha256(
            json.dumps(
                [
                    "water-coloured-sweep-v1",
                    key,
                    parent_version,
                    [(p, v.version) for p, v in plans.items()],
                ],
                sort_keys=True,
            ).encode()
        ).hexdigest()
    return {
        "state": state,
        "version": version,
        "dependency_nodes": tuple(plans),
        "required": tuple(sorted({k for p in plans.values() for k in p.required})),
        "missing": tuple(missing),
        "receipt_lookups": sum(p.work["receipt_lookups"] for p in plans.values()),
    }


@dataclass(frozen=True)
class UpdatedNode:
    state: str
    world: np.ndarray | None
    denominator: int | None
    reason: str | None = None


class ColoredUpdates:
    """Callbacks and parent are immutable for this calculation's lifetime."""

    def __init__(self, target, parent, admissible, *, state_limit=4096):
        if type(state_limit) is not int or not 1 <= state_limit <= 65536:
            raise ValueError("bounded state limit required")
        self.target, self.parent, self.admissible = target, parent, admissible
        self.state_limit = state_limit
        self.states = {}
        self.work = {
            "states": 0,
            "face_checks": 0,
            "candidate_moves": 0,
            "explicit_parent_copy_bytes": 0,
            "explicit_face_gather_bytes": 0,
        }

    def node(self, key, phase=2):
        if type(phase) is not int or phase not in (-1, 0, 1, 2):
            raise ValueError("one fixed three-colour sweep required")
        address = (key, phase)
        if address in self.states:
            return self.states[address]
        if self.work["states"] >= self.state_limit:
            return UpdatedNode("budget_exhausted", None, None)
        self.work["states"] += 1
        if phase == -1:
            world = np.asarray(self.parent(key), dtype=float).copy()
            self.work["explicit_parent_copy_bytes"] += world.nbytes
            if world.shape != (3,) or not np.isfinite(world).all():
                raise ValueError("finite ECEF parent node required")
            world.setflags(write=False)
            result = UpdatedNode("ready", world, None)
        elif (key[1] + key[2]) % 3 != phase:
            result = self.node(key, phase - 1)
        else:
            result = self._advance(key, phase)
        self.states[address] = result
        return result

    def _check(self, faces, positions):
        for face in faces:
            self.work["face_checks"] += 1
            world = np.array([positions[p] for p in face])
            self.work["explicit_face_gather_bytes"] += world.nbytes
            state = self.admissible(face, world)
            if state != "admitted":
                return state
        return "admitted"

    def _advance(self, key, phase):
        target = self.target(key)
        if target.state != "ready":
            return UpdatedNode(target.state, None, None)
        if (
            target.world is None
            or target.world.shape != (3,)
            or not np.isfinite(target.world).all()
        ):
            raise ValueError("ready target must contain a finite ECEF node")
        faces = incident_faces(key)
        previous = {p: self.node(p, phase - 1) for face in faces for p in face}
        if any(p.state != "ready" for p in previous.values()):
            return UpdatedNode("pending_predecessor", None, None)
        positions = {p: v.world for p, v in previous.items()}
        check = self._check(faces, positions)
        if check != "admitted":
            return UpdatedNode("parent_refused", None, None, check)
        prior = positions[key]
        for exponent in range(16):
            self.work["candidate_moves"] += 1
            denominator = 1 << exponent
            candidate = prior + (target.world - prior) / denominator
            positions[key] = candidate
            if self._check(faces, positions) == "admitted":
                candidate.setflags(write=False)
                return UpdatedNode("ready", candidate, denominator)
        return UpdatedNode("ready", prior, 0)
