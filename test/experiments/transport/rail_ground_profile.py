"""Explicit constructive rail profiles bound to untouched native/source facts."""

import hashlib
import math
from dataclasses import dataclass
from types import MappingProxyType

from mvt_source_profile_inputs import profile_properties
from outshine_ref.transport.canonical_ports._end_candidates import float_word

RECIPE = "outshine/constructive-ground-rail-crossover/1"


@dataclass(frozen=True)
class RailGroundProfile:
    members: tuple
    node_positions: tuple
    source_signature: tuple
    feature_contents: tuple
    maximum_gradient: float

    def __post_init__(self):
        object.__setattr__(self, "members", tuple(self.members))
        object.__setattr__(
            self, "node_positions", tuple((n, tuple(p)) for n, p in self.node_positions)
        )
        object.__setattr__(self, "source_signature", tuple(self.source_signature))
        object.__setattr__(self, "feature_contents", tuple(self.feature_contents))
        if (
            not 1 <= len(self.members) <= 32
            or not 1 <= len(self.node_positions) <= 32
            or not math.isfinite(self.maximum_gradient)
            or not 0 < self.maximum_gradient <= 0.025
            or len(self.source_signature) != 3
            or any(
                not isinstance(s, str) or len(s) != 64 for s in self.source_signature
            )
            or not 1 <= len(self.feature_contents) <= 32
        ):
            raise ValueError("Bound immutable constructive rail recipe required")

    def grades(self):
        return MappingProxyType(
            {m.identifier: self.maximum_gradient for m in self.members}
        )

    def check_index(self, index):
        nodes, spans, _ = index
        if any(spans.get(m.identifier) is not m for m in self.members) or any(
            nodes.get(n) != position for n, position in self.node_positions
        ):
            raise ValueError("rail_construction_original_object_binding")

    def record(self):
        return {
            "version": RECIPE,
            "source_input_sha256": self.source_signature[0],
            "native_snapshot_sha256": self.source_signature[1],
            "source_facts_digest": self.source_signature[2],
            "feature_sha256": self.feature_contents,
            "members": tuple(m.identifier for m in self.members),
            "original_maximum_gradient_words": tuple(
                m.attributes["recipe"]["native_maximum_gradient_word"]
                for m in self.members
            ),
            "constructed_gradient_cap": self.maximum_gradient,
            "scope": "Constructive geometry only; original missing profiles/Unknown/Source/EndContact/World authority are unchanged.",
        }


def bind_profile(
    index, selected, source_binding, raw_source, *, maximum_gradient=0.025
):
    selected = tuple(sorted(selected))
    if not 1 <= len(selected) <= 32 or len(set(selected)) != len(selected):
        raise ValueError("rail_construction_member_budget")
    if not math.isfinite(maximum_gradient) or not 0 < maximum_gradient <= 0.025:
        raise ValueError("rail_construction_grade_budget")
    if not isinstance(raw_source, bytes) or len(raw_source) > 4 * 1024 * 1024:
        raise ValueError("rail_construction_source_byte_budget")
    capture = source_binding["native_capture"]
    digest = hashlib.sha256(raw_source).hexdigest()
    if digest != capture["input_sha256"]:
        raise ValueError("rail_construction_source_content_mismatch")
    properties, _ = profile_properties(
        raw_source,
        property_names=("class", "subclass", "service", "indoor", "level", "brunnel"),
    )
    nodes, spans, _ = index
    if any(e not in spans for e in selected):
        raise ValueError("rail_construction_missing_original_member")
    members = tuple(spans[e] for e in selected)
    features, crossovers = [], []
    for member in members:
        attrs = member.attributes
        recipe = attrs["recipe"]
        feature = bytes(attrs["native_path"]["LocalWayBinding"]["FeatureContent"])
        source = attrs["native_source_reference"]["Source"]
        if source != 0 or len(source_binding["sources"]) != 1:
            raise ValueError("rail_construction_single_source_binding_required")
        original = bytes(source_binding["sources"][source]["Content"])
        tags = properties.get(feature, {})
        if (
            original.hex() != digest
            or len(feature) != 32
            or any(
                tags.get(k) != v for k, v in (("class", "rail"), ("subclass", "rail"))
            )
            or tags.get("brunnel") not in (None, "")
            or tags.get("indoor") not in (None, False, 0, "no")
            or tags.get("level") not in (None, 0, "0")
            or member.bridge
            or member.tunnel
            or member.layer != 0
            or attrs.get("ground_height_recipe_qualified") is not True
            or member.maximum_gradient is not None
            or recipe["native_maximum_gradient_word"] != 0
            or not 0 < member.width_m <= 30
            or 2 * float_word(recipe["native_half_width_word"]) != member.width_m
        ):
            raise ValueError("rail_construction_original_profile_or_class_required")
        features.append(feature.hex())
        if tags.get("service") == "crossover":
            crossovers.append(feature)
    if not crossovers:
        raise ValueError("rail_construction_crossover_component_required")
    used = sorted({n for m in members for n in m.nodes})
    if len(used) > 32 or any(n not in nodes for n in used):
        raise ValueError("rail_construction_node_budget_or_missing_node")
    signature = (
        digest,
        capture["files"]["native.json.gz"]["raw_sha256"],
        source_binding["source_facts_digest"],
    )
    return RailGroundProfile(
        members,
        tuple((n, tuple(nodes[n])) for n in used),
        signature,
        tuple(sorted(set(features))),
        float(maximum_gradient),
    )
