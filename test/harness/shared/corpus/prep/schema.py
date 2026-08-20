"""The one declaration of what a render manifest may contain, read.

The declaration is `test/harness/shared/corpus/manifest-schema.json` and the runner reads the same file. Nothing
here states a key, a type or an allowed value: this module is the reader, and a fact it knows that
the file does not is the defect it exists to remove.
"""

import json
import os
import re

from .refusal import Refusal

_PATTERNS = {
    "filename": ("a relative path inside the case directory",
                 r"^(?!/)(?!.*(^|/)\.\.?(/|$))(?!.*//)[^/].*[^/]$|^[^/.][^/]*$"),
    "sha256": ("64 lowercase hex digits", r"^[0-9a-f]{64}$"),
    "sha1": ("40 lowercase hex digits", r"^[0-9a-f]{40}$"),
    "release": ("a release version, e.g. 5.2.0", r"^\d+\.\d+(\.\d+)?$"),
}

_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                     "manifest-schema.json")

def _load():
    with open(_PATH, "r") as f:
        return json.load(f)

_DECLARATION = _load()
SCHEMA = _DECLARATION["schema"]
SCHEMA_VERSION = _DECLARATION["schemaVersion"]
_OBJECTS = _DECLARATION["objects"]

def enumeration(object_name, key):
    """The allowed values of an enumerated key, for a caller that has to branch on one.

    A branch is a reader's business; WHICH values exist is this file's, so a reader asks rather
    than restating the list beside its own arms.
    """
    node = _OBJECTS[object_name]
    while "variants" in node:
        node = next(iter(node["variants"].values()))
    for group in ("required", "optional"):
        declared = node.get(group, {}).get(key)
        if isinstance(declared, dict) and "enum" in declared:
            return tuple(declared["enum"])
    raise KeyError(object_name + "." + key + " is not an enumerated key of this schema")

def variants(object_name):
    """The discriminator values an object declares, in the file's own order."""
    node = _OBJECTS[object_name]
    if "variants" not in node:
        raise KeyError(object_name + " declares no variants")
    return tuple(node["variants"])

def check(object_name, where, value):
    """Validate `value` against the named object and return it."""
    if object_name not in _OBJECTS:
        raise KeyError(object_name + " is not declared in " + _PATH)
    return _object(_OBJECTS[object_name], where, value, ())

def _object(node, where, value, inherited):
    if not isinstance(value, dict):
        raise Refusal(where, expected="an object", observed=_seen(value))
    while "variants" in node:
        key = node["discriminator"]
        chosen = value.get(key, node.get("default"))
        if chosen not in node["variants"]:
            raise Refusal(where + "." + key,
                          expected="one of " + ", ".join(node["variants"]),
                          observed=repr(value.get(key)))
        inherited = inherited + (key,)
        node = node["variants"][chosen]
    required = node.get("required", {})
    optional = node.get("optional", {})
    known = set(required) | set(optional) | set(inherited)
    for key in sorted(value):
        if key not in known:
            raise Refusal(
                where + "." + key,
                expected="one of " + ", ".join(sorted(known)),
                observed=key,
                why="a key nobody reads is a setting that silently did not apply",
            )
    for key in sorted(required):
        if key not in value:
            raise Refusal(where, expected="the field " + repr(key), observed="absent")
    for key, declared in list(required.items()) + list(optional.items()):
        if key in value:
            _value(declared, where + "." + key, value[key])
    return value

def _value(declared, where, value):
    if isinstance(declared, str):
        _scalar(declared, where, value)
        return
    if "enum" in declared:
        if value not in declared["enum"]:
            raise Refusal(where, expected="one of " + ", ".join(repr(v) for v in declared["enum"]),
                          observed=repr(value))
        return
    if "object" in declared:
        _object(_OBJECTS[declared["object"]], where, value, ())
        return
    if "array" in declared:
        if not isinstance(value, list) or not value:
            raise Refusal(where, expected="a non-empty list", observed=_seen(value))
        for index, item in enumerate(value):
            _value(declared["array"], "%s[%d]" % (where, index), item)
        return
    if "oneOrMore" in declared:
        items = value if isinstance(value, list) else [value]
        if not items:
            raise Refusal(where, expected="one, or a non-empty list", observed=_seen(value))
        for index, item in enumerate(items):
            at = "%s[%d]" % (where, index) if isinstance(value, list) else where
            _value(declared["oneOrMore"], at, item)
        return
    if "map" in declared:
        if not isinstance(value, dict) or not value:
            raise Refusal(where, expected="a non-empty object", observed=_seen(value))
        for name in sorted(value):
            _value(declared["map"], where + "." + name, value[name])
        return
    if "quantity" in declared:
        _quantity(declared["quantity"], where, value)
        return
    raise KeyError("the schema declares a type this reader does not know: " + repr(declared))

def _quantity(inner, where, value):
    if not isinstance(value, dict):
        raise Refusal(where, expected="value, unit and origin", observed=_seen(value))
    known = {"value", "unit", "origin", "derivation", "note"}
    for key in sorted(value):
        if key not in known:
            raise Refusal(where + "." + key, expected="one of " + ", ".join(sorted(known)),
                          observed=key)
    for key in ("value", "unit", "origin"):
        if key not in value:
            raise Refusal(where, expected="the field " + repr(key), observed="absent")
    _value(inner, where + ".value", value["value"])
    _scalar("text", where + ".unit", value["unit"])
    origins = ("SET", "derived", "measured")
    if value["origin"] not in origins:
        raise Refusal(where + ".origin", expected="one of " + ", ".join(origins),
                      observed=repr(value["origin"]))
    if value["origin"] == "derived" and not value.get("derivation"):
        raise Refusal(where, expected="a derivation beside a derived number", observed="none",
                      why="derived without its derivation is a bare number wearing a label")

def _scalar(kind, where, value):
    if kind == "string":
        if not isinstance(value, str):
            raise Refusal(where, expected="a string", observed=_seen(value))
    elif kind == "text":
        if not isinstance(value, str) or not value:
            raise Refusal(where, expected="a non-empty string", observed=_seen(value))
    elif kind == "boolean":
        if not isinstance(value, bool):
            raise Refusal(where, expected="true or false", observed=_seen(value))
    elif kind == "number":
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            raise Refusal(where, expected="a number", observed=_seen(value))
    elif kind in ("integer", "index"):
        if not isinstance(value, int) or isinstance(value, bool):
            raise Refusal(where, expected="a whole number", observed=_seen(value))
        if kind == "index" and value < 0:
            raise Refusal(where, expected="zero or above", observed=repr(value))
    elif kind == "vector3":
        if not isinstance(value, list) or len(value) != 3:
            raise Refusal(where, expected="three numbers", observed=_seen(value))
        for component in value:
            if not isinstance(component, (int, float)) or isinstance(component, bool):
                raise Refusal(where, expected="three numbers", observed=repr(value))
    elif kind == "opaque":
        if not isinstance(value, dict) or not value:
            raise Refusal(where, expected="a non-empty object", observed=_seen(value))
    elif kind in _PATTERNS:
        expected, pattern = _PATTERNS[kind]
        if not isinstance(value, str) or not re.match(pattern, value):
            raise Refusal(where, expected=expected, observed=_seen(value))
    else:
        raise KeyError("the schema declares a scalar this reader does not know: " + kind)

def _seen(value):
    return type(value).__name__ + " " + repr(value)[:80]
