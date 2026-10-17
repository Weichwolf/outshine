"""Finite portable recipe data; no callable, pickle or domain geometry semantics."""

import hashlib
import json
from collections.abc import Mapping
from dataclasses import dataclass

from outshine_ref.contracts import identifier, immutable


def plain(value):
    if isinstance(value, Mapping):
        return {key: plain(item) for key, item in value.items()}
    if isinstance(value, tuple):
        return [plain(item) for item in value]
    return value


def canonical(value):
    frozen = immutable(value)

    def numbers(item, depth=0):
        if depth > 64:
            raise ValueError("Recipe JSON nesting exceeds 64 levels")
        if type(item) is int and abs(item) > 2**53 - 1:
            raise ValueError(
                "Portable JSON integers must fit exactly in binary64; use strings for identities"
            )
        if isinstance(item, Mapping):
            for key, child in item.items():
                identifier(key)
                numbers(child, depth + 1)
        elif isinstance(item, tuple):
            for child in item:
                numbers(child, depth + 1)

    numbers(frozen)
    return json.dumps(
        plain(frozen),
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
        ensure_ascii=True,
    ).encode("ascii")


def digest(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def fields(value, required):
    if not isinstance(value, Mapping) or set(value) != set(required):
        raise ValueError("Expected exactly these fields: " + ", ".join(required))


def named(value):
    if not isinstance(value, str):
        raise TypeError("Recipe identities and versions must remain strings")
    identifier(value)
    if len(value.encode("utf-8")) > 256:
        raise ValueError("Recipe identity exceeds 256 UTF-8 bytes")
    return value


@dataclass(frozen=True, slots=True)
class Key:
    id: str
    version: str

    def __post_init__(self):
        named(self.id)
        named(self.version)

    @classmethod
    def read(cls, value):
        fields(value, ("id", "version"))
        return cls(value["id"], value["version"])


@dataclass(frozen=True, slots=True)
class Budget:
    max_definitions: int = 32
    max_definition_bytes: int = 262144
    max_nodes: int = 128
    max_depth: int = 32
    max_product_bytes: int = 268435456

    def __post_init__(self):
        if any(
            type(getattr(self, k)) is not int or getattr(self, k) < 0
            for k in self.__slots__
        ):
            raise ValueError("Recipe budgets require nonnegative integers")


def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON key: " + key)
        result[key] = value
    return result


def reference(value):
    if not isinstance(value, Mapping):
        raise TypeError("A named product reference is required")
    if set(value) not in ({"input"}, {"node", "output"}, {"dependency", "output"}):
        raise ValueError("Unknown product reference form")
    for item in value.values():
        named(item)


def definition(raw, expected):
    if not isinstance(raw, bytes):
        raise TypeError("Recipe loader must return bounded bytes")
    try:
        value = json.loads(raw, object_pairs_hook=unique)
        canonical(value)
    except (UnicodeError, RecursionError) as error:
        raise ValueError("Unportable recipe JSON") from error
    fields(
        value,
        (
            "schema_version",
            "id",
            "version",
            "requires",
            "inputs",
            "dependencies",
            "nodes",
            "outputs",
        ),
    )
    if value["schema_version"] != "1" or Key(value["id"], value["version"]) != expected:
        raise ValueError("Recipe schema or exact identity/version mismatch")
    for name in ("inputs", "dependencies", "nodes", "outputs"):
        if not isinstance(value[name], dict):
            raise TypeError("Recipe " + name + " must be a named mapping")
    if (
        not isinstance(value["requires"], list)
        or any(not isinstance(c, str) for c in value["requires"])
        or len(set(value["requires"])) != len(value["requires"])
    ):
        raise ValueError("Capabilities must be distinct names")
    for capability in value["requires"]:
        named(capability)
    for kind in value["inputs"].values():
        named(kind)
    for dep in value["dependencies"].values():
        fields(dep, ("id", "version", "inputs"))
        Key(dep["id"], dep["version"])
        if not isinstance(dep["inputs"], dict):
            raise TypeError("Dependency inputs require explicit parent-input bindings")
        for parent in dep["inputs"].values():
            named(parent)
    for node in value["nodes"].values():
        fields(node, ("operator", "parameters", "inputs", "max_output_bytes"))
        Key.read(node["operator"])
        if not isinstance(node["parameters"], dict) or not isinstance(
            node["inputs"], dict
        ):
            raise TypeError("Operator parameters and inputs must be mappings")
        if type(node["max_output_bytes"]) is not int or node["max_output_bytes"] < 0:
            raise ValueError("Node output reservation requires nonnegative bytes")
        for item in node["inputs"].values():
            reference(item)
    for item in value["outputs"].values():
        reference(item)
    return immutable(value)


class Files:
    """Explicit selected-key lookup; no directory scan, index or cache policy."""

    def __init__(self, paths):
        self.paths = dict(paths)

    def __call__(self, key, remaining_bytes):
        if key not in self.paths:
            raise ValueError("Missing recipe/version: " + repr(key))
        with self.paths[key].open("rb") as stream:
            raw = stream.read(remaining_bytes + 1)
        if len(raw) > remaining_bytes:
            raise ValueError("Definition byte budget exceeded before JSON decode")
        return raw
