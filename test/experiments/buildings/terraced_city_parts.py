"""Keep the exact same-camera previous draw parts for a paired offline comparison."""

import hashlib

import numpy as np
from outshine_ref.buildings.modelling.radial_mesh import radial_mesh
from tile_buildings import prism


def fingerprint(mesh):
    digest = hashlib.sha256()
    for name, value in sorted(mesh.items()):
        digest.update(name.encode())
        digest.update(value.dtype.str.encode())
        digest.update(str(value.shape).encode())
        digest.update(np.ascontiguousarray(value).view(np.uint8))
    return digest.hexdigest()


def render_part(result):
    mesh, info = result
    keys = ("vertices", "faces", "normals", "uv", "materials", "facade_extent")
    return {key: mesh[key] for key in keys if key in mesh}, info


class PreviousParts:
    def __init__(self, owner):
        self.owner = owner
        self.parts = {}
        self.materials = {}
        self.hashes = {}

    def __call__(self, row, parameters, material):
        identifier = row["id"]
        if row is not self.owner.source_rows[identifier]:
            raise ValueError("Paired previous draw requires the same prepared source")
        if identifier not in self.parts:
            result = self.owner(row, parameters, material)
            self.parts[identifier] = render_part(
                result if result is not None else prism(row, parameters, material)
            )
            self.materials[identifier] = material
            self.hashes[identifier] = fingerprint(self.parts[identifier][0])
        elif self.materials[identifier] != material:
            raise ValueError("Paired previous source changed its wall binding")
        return self.parts[identifier]

    def radial(self, row, plan, info, error):
        identifier = row["id"]
        if identifier not in self.parts:
            result = radial_mesh(plan, error_m=error)
            info = info | {
                "profile_knots": plan.knots,
                "profile_indices": result.profile_indices,
                "sides": result.sides,
                "radial_error_m": result.error_m,
                "logical_profile": "complete camera-independent profile",
                "triangles": len(result.mesh["faces"]),
            }
            self.parts[identifier] = render_part((result.mesh, info))
            self.hashes[identifier] = fingerprint(self.parts[identifier][0])
        return self.parts[identifier]
