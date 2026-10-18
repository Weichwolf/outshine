"""Cold bounded cloud inputs from the saved spatial crown, without whole leaf expansion."""

import hashlib
import json
from dataclasses import asdict, replace
from functools import cached_property
from pathlib import Path

import numpy as np
from botanical_profiles import plant_recipe
from crown_shoots import CrownShoots


class SpatialGroupSource:
    def __init__(self, crown, blades, material_inputs=None):
        if not isinstance(crown, CrownShoots) or not 1 <= len(blades) <= 64:
            raise ValueError(
                "prepared spatial crown and bounded blade catalog required"
            )
        if np.any(crown.variants >= len(blades)):
            raise ValueError("complete source variant catalog required")
        self.crown, self.blades = crown, tuple(blades)
        self.material_json = json.dumps(material_inputs or {}, sort_keys=True)
        self.prototypes = {}
        self.areas = {}

    @cached_property
    def identity(self):
        recipe = {
            "format": "cold-spatial-group-source-1",
            "leaf_plan_identity": self.crown.identity,
            "blade_mesh": [8, 1],
            "blades": [asdict(b) for b in self.blades],
            "material_inputs": json.loads(self.material_json),
        }
        return hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()

    def summary(self):
        return self.crown.summary() | {
            "catalog_identity": self.identity,
            "prepared_blade_prototypes": len(self.prototypes),
            "retained_pose_groups": len(self.crown._groups),
            "material_inputs": json.loads(self.material_json),
        }

    def geometry(self, node, max_leaves=128):
        if type(max_leaves) is not int or not 0 <= max_leaves <= 128:
            raise ValueError("explicit source group capacity up to128 required")
        index = self.crown._host_index(node)
        count = int(self.crown.counts[index])
        if count > max_leaves:
            return None
        if node not in self.crown._groups:
            # This adapter owns its plan: stream hosts without retaining all leaf poses.
            self.crown._groups.clear()
        poses = self.crown.group(node, max_leaves)
        origin, frame = self.crown.stem(node)
        vertices, faces, normals, parameters, variants, owners, physical = (
            [],
            [],
            [],
            [],
            [],
            [],
            [],
        )
        offset = 0
        for organ, (point, rotation, variant) in enumerate(
            zip(poses["origins"], poses["frames"], poses["variants"], strict=True)
        ):
            variant = int(variant)
            if variant not in self.prototypes:
                mesh = self.blades[variant].mesh(8, 1)
                for value in mesh.values():
                    value.setflags(write=False)
                self.prototypes[variant] = mesh
                self.areas[variant] = self.blades[variant].area()
            mesh = self.prototypes[variant]
            vertices.append((mesh["vertices"] @ rotation.T + point - origin) @ frame)
            faces.append(mesh["faces"] + offset)
            normals.append(mesh["normals"] @ rotation.T @ frame)
            parameters.append(mesh["parameters"])
            variants.append(np.full(len(mesh["vertices"]), variant, np.uint8))
            owners.append(np.full(len(mesh["faces"]), organ, np.int32))
            physical.append(self.areas[variant])
            offset += len(mesh["vertices"])
        if not count:
            raise ValueError("nonempty requested foliage group required")
        return {
            "vertices": np.concatenate(vertices),
            "faces": np.concatenate(faces),
            "normals": np.concatenate(normals),
            "parameters": np.concatenate(parameters),
            "variants": np.concatenate(variants),
            "organ_faces": np.concatenate(owners),
            "ids": poses["ids"],
            "leaf_variants": poses["variants"],
            "physical_leaf_area_m2": np.asarray(physical),
            "organ_anchors": (poses["nodes"] - origin) @ frame,
            "anchor_origin": origin,
            "anchor_frame": frame,
            "plan_identity": self.crown.identity,
            "host_node": node,
            "catalog_identity": self.identity,
            "material_inputs_json": self.material_json,
        }


def load_source(inputs):
    inputs = Path(inputs)
    metadata = json.loads((inputs / "comparison.json").read_text())
    if metadata["recipe"]["leaves"] > 100000:
        raise ValueError("bounded prepared mature-crown recipe required")
    with np.load(inputs / "spatial/skeleton.npz", allow_pickle=False) as z:
        positions, parents = z["positions"], z["parents"]
    # NPZ members are lazy: only90k variant bytes, no complete saved pose arrays.
    with np.load(inputs / "spatial/tables.npz", allow_pickle=False) as z:
        variants = z["leaf_variants"]
    crown = CrownShoots(
        positions,
        parents,
        variants,
        metadata["recipe"]["seed"],
        length_m=0.35,
        bend_rad=0.4,
        foliage_depth_m=1.1,
    )
    if crown.identity != metadata["products"]["spatial"]["identity"]:
        raise ValueError("saved source identity does not match spatial crown")
    blades = tuple(
        replace(b, length=b.length * 7 / 6, width=b.width * 7 / 6)
        for b in plant_recipe("quercus", metadata["recipe"]["seed"]).blades
    )
    folder = inputs / "spatial"
    material = json.loads((folder / "fern-material.json").read_text())
    paths = [
        folder / "fern-material.json",
        folder / material["colour"],
        folder / material["height"],
    ]
    adapter = Path(__file__).with_name("fern_material.py")
    material_inputs = {
        "original_recipe": material,
        "original_files_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "qualitative_adapter": "fern_material.leaf_material",
        "adapter_sha256": hashlib.sha256(adapter.read_bytes()).hexdigest(),
        "comparison_colour": "leaf_sheet.organ_colour; coverage/normal probes do not evaluate original material",
        "scope": "Original procedural inputs/adapter retained by content identity. No calibrated species or aggregate R/T is present in this source.",
    }
    return SpatialGroupSource(crown, blades, material_inputs)
