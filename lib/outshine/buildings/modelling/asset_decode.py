"""Decode optional OBJ surfaces with source materials and encoded resources."""

import hashlib
import json
from pathlib import Path

import trimesh
from trimesh.exchange import obj

from .metadata import Material
from .asset_resources import ImageResource, Resources


def flattened(values, prefix=""):
    result = {}
    for key, value in values.items():
        name = prefix + key
        if value is None:
            continue
        if isinstance(value, dict):
            result.update(flattened(value, name + "."))
        elif isinstance(value, list) and any(isinstance(item, dict) for item in value):
            result[name] = json.dumps(value, sort_keys=True, separators=(",", ":"))
        else:
            result[name] = value
    return result


def decode_obj(path, resolver):
    with path.open("rb") as stream:
        data = obj.load_obj(
            stream,
            resolver=resolver,
            group_material=False,
            split_objects=True,
            split_groups=True,
        )
    materials = []
    ids = {}
    images = []
    textures = []
    for name, geometry in data["geometry"].items():
        visual = geometry.get("visual")
        source = getattr(visual, "material", None)
        if source is None:
            ids[name] = None
            continue
        values = dict(getattr(source, "kwargs", {}))
        values = {key: value for key, value in values.items() if key != "image"}
        image = getattr(source, "image", None)
        if image is not None:
            image_name = image.info.get("file_path")
            if not image_name:
                raise ValueError("OBJ material image has no source reference")
            values["baseColorTexture.index"] = len(images)
            images.append(
                ImageResource(
                    image_name,
                    "image/" + str(image.format).lower(),
                    resolver.get(image_name),
                )
            )
            textures.append((len(images) - 1, None))
        materials.append(Material(getattr(source, "name", name), flattened(values)))
        ids[name] = len(materials) - 1
    return data, ids, materials, (), Resources(tuple(images), tuple(textures), ())


def scene(data):
    result = trimesh.Scene(base_frame=data.get("base_frame", "world"))
    for name, values in data["geometry"].items():
        if "faces" not in values:
            raise ValueError("Asset decoder returned non-surface geometry")
        if "vertex_normals" in values:
            import numpy as np

            normal = np.asarray(values["vertex_normals"])
            if (
                normal.shape != np.asarray(values["vertices"]).shape
                or not np.isfinite(normal).all()
                or np.any(np.linalg.norm(normal, axis=1) == 0)
            ):
                raise ValueError("Asset declares inconsistent vertex normals")
        result.geometry[name] = trimesh.Trimesh(**(values | {"process": False}))
    for edge in data.get("graph", ()):
        result.graph.update(**edge)
    return result


class Resolver(trimesh.resolvers.FilePathResolver):
    def __init__(self, path):
        super().__init__(str(path))
        self.dependencies = {}
        self.missing = []

    def get(self, name):
        try:
            value = super().get(name)
        except OSError:
            self.missing.append(str(name))
            raise
        self.dependencies[str(name)] = hashlib.sha256(value).hexdigest()
        return value


def decode(path):
    path = Path(path).resolve()
    suffix = path.suffix.lower()
    if suffix != ".obj":
        raise NotImplementedError("Asset file format is not implemented: " + suffix)
    resolver = Resolver(path)
    data, ids, materials, omitted, resources = decode_obj(path, resolver)
    if resolver.missing:
        raise ValueError(
            "Asset references missing resources: " + ", ".join(resolver.missing)
        )
    return (
        scene(data),
        data,
        ids,
        tuple(materials),
        omitted,
        resolver.dependencies,
        resources,
    )
