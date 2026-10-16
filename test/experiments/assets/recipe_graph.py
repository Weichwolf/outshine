"""Isolated versioned recipe DAG pilot; domain operators own values and physical validity."""

from collections.abc import Mapping
from dataclasses import dataclass, field
from types import MappingProxyType

from outshine_ref.contracts import immutable
from recipe_graph_data import Budget, Key, definition, digest, named


def size(value):
    if type(value) is not int or not 0 <= value <= 2**53 - 1:
        raise ValueError(
            "Accounted product bytes require a portable nonnegative integer"
        )


@dataclass(frozen=True, slots=True)
class Emission:
    value: object
    nbytes: int
    metadata: Mapping = field(default_factory=dict)

    def __post_init__(self):
        size(self.nbytes)
        if not isinstance(self.metadata, Mapping):
            raise TypeError("Product metadata must be a finite data mapping")
        digest(self.metadata)
        object.__setattr__(self, "metadata", immutable(self.metadata))


@dataclass(frozen=True, slots=True)
class Product(Emission):
    type_id: str = ""
    fingerprint: str = ""

    def __post_init__(self):
        Emission.__post_init__(self)
        named(self.type_id)
        if len(self.fingerprint) != 64 or any(
            c not in "0123456789abcdef" for c in self.fingerprint
        ):
            raise ValueError("Product fingerprint must be a SHA256 hex string")


def external(type_id, value, content, nbytes, metadata):
    """Caller supplies the complete normalized content/digests, never just an instance ID."""
    return Product(
        value,
        nbytes,
        metadata,
        type_id,
        digest({"type": type_id, "content": content, "metadata": metadata}),
    )


@dataclass(frozen=True, slots=True)
class Operator:
    key: Key
    inputs: Mapping
    outputs: Mapping
    apply: object

    def __post_init__(self):
        if not isinstance(self.key, Key) or not callable(self.apply):
            raise TypeError("Operator requires an exact key and trusted implementation")
        for attr in ("inputs", "outputs"):
            values = getattr(self, attr)
            if not isinstance(values, Mapping):
                raise TypeError("Operator ports must be named mappings")
            for name, kind in values.items():
                named(name)
                named(kind)
            object.__setattr__(self, attr, immutable(values))


def closure(root, loader, capabilities, budget):
    loaded, visiting, order = {}, set(), []
    used_bytes = 0

    def visit(key, depth):
        nonlocal used_bytes
        if key in visiting:
            raise ValueError("Cyclic recipe dependency")
        if key in loaded:
            return
        if depth > budget.max_depth or len(loaded) >= budget.max_definitions:
            raise ValueError("Definition count/depth budget exceeded before loading")
        visiting.add(key)
        raw = loader(key, budget.max_definition_bytes - used_bytes)
        if (
            not isinstance(raw, bytes)
            or len(raw) + used_bytes > budget.max_definition_bytes
        ):
            raise ValueError("Definition byte budget exceeded before decode")
        used_bytes += len(raw)
        value = definition(raw, key)
        if not set(value["requires"]) <= capabilities:
            raise ValueError("Unavailable recipe capabilities")
        loaded[key] = value
        for dep in value["dependencies"].values():
            visit(Key(dep["id"], dep["version"]), depth + 1)
        visiting.remove(key)
        order.append(key)

    visit(root, 1)
    return loaded, order, used_bytes


@dataclass(frozen=True, slots=True)
class Port:
    type_id: str
    node: tuple | None
    name: str
    depth: int = 0


@dataclass(frozen=True, slots=True)
class Node:
    path: tuple
    owner_digest: str
    operator: Operator
    parameters: Mapping
    inputs: Mapping
    max_output_bytes: int
    depth: int


def compile_recipe(key, definitions, registry, external_ports, prefix, nodes, budget):
    value = definitions[key]
    if set(value["inputs"]) != set(external_ports) or any(
        value["inputs"][n] != p.type_id for n, p in external_ports.items()
    ):
        raise ValueError("Dependency external-input names or types disagree")
    deps = {}
    for alias, dep in value["dependencies"].items():
        child_ports = {
            n: external_ports[parent]
            for n, parent in dep["inputs"].items()
            if parent in external_ports
        }
        if len(child_ports) != len(dep["inputs"]):
            raise ValueError("Dependency refers to a missing external input")
        deps[alias] = compile_recipe(
            Key(dep["id"], dep["version"]),
            definitions,
            registry,
            child_ports,
            (*prefix, alias),
            nodes,
            budget,
        )
    visiting, finished = set(), {}

    def link(ref):
        if "input" in ref:
            return external_ports[ref["input"]]
        if "dependency" in ref:
            return deps[ref["dependency"]][ref["output"]]
        return build(ref["node"])[ref["output"]]

    def build(name):
        if name in visiting:
            raise ValueError("Cyclic operator-node reference")
        if name in finished:
            return finished[name]
        visiting.add(name)
        raw = value["nodes"][name]
        op = registry[Key.read(raw["operator"])]
        inputs = {n: link(ref) for n, ref in raw["inputs"].items()}
        if set(inputs) != set(op.inputs) or any(
            p.type_id != op.inputs[n] for n, p in inputs.items()
        ):
            raise ValueError("Operator port name/type mismatch")
        depth = 1 + max((p.depth for p in inputs.values()), default=0)
        if depth > budget.max_depth or len(nodes) >= budget.max_nodes:
            raise ValueError("Operator count/depth budget exceeded")
        path = (*prefix, name)
        nodes[path] = Node(
            path,
            digest(value),
            op,
            raw["parameters"],
            MappingProxyType(inputs),
            raw["max_output_bytes"],
            depth,
        )
        finished[name] = {
            n: Port(kind, path, n, depth) for n, kind in op.outputs.items()
        }
        visiting.remove(name)
        return finished[name]

    for name in value["nodes"]:
        build(name)
    return {name: link(ref) for name, ref in value["outputs"].items()}


@dataclass(frozen=True, slots=True)
class Run:
    outputs: Mapping
    definition_fingerprint: str
    instance_fingerprint: str
    stats: Mapping


def signature(product):
    return {
        "type": product.type_id,
        "fingerprint": product.fingerprint,
        "metadata": product.metadata,
    }


class Prepared:
    def __init__(self, root, loader, operators, capabilities=(), budget=None):
        budget = Budget() if budget is None else budget
        if not isinstance(root, Key) or not isinstance(budget, Budget):
            raise TypeError("An exact recipe key and Budget are required")
        registry = {}
        for operator in operators:
            if not isinstance(operator, Operator) or operator.key in registry:
                raise ValueError("Operators require unique exact keys")
            registry[operator.key] = operator
        self.root, self.budget = root, budget
        self.definitions, order, self.definition_bytes = closure(
            root, loader, frozenset(capabilities), budget
        )
        self.inputs = self.definitions[root]["inputs"]
        self.nodes = {}
        try:
            self.outputs = compile_recipe(
                root,
                self.definitions,
                registry,
                {n: Port(t, None, n) for n, t in self.inputs.items()},
                (),
                self.nodes,
                budget,
            )
        except (KeyError, RecursionError) as error:
            raise ValueError(
                "Missing exact operator version, recipe port or excessive graph depth"
            ) from error
        self.definition_fingerprint = digest(
            [
                self.definitions[k]
                for k in sorted(order, key=lambda k: (k.id, k.version))
            ]
        )
        self.definitions = MappingProxyType(self.definitions)
        self.nodes = MappingProxyType(self.nodes)
        self.outputs = MappingProxyType(self.outputs)

    def required(self, outputs):
        names = tuple(self.outputs) if outputs is None else tuple(outputs)
        if (
            not names
            or len(set(names)) != len(names)
            or any(n not in self.outputs for n in names)
        ):
            raise ValueError("Requested outputs must be distinct declared names")
        needed = set()

        def visit(port):
            if port.node is None or port.node in needed:
                return
            needed.add(port.node)
            for child in self.nodes[port.node].inputs.values():
                visit(child)

        for name in names:
            visit(self.outputs[name])
        return names, [node for path, node in self.nodes.items() if path in needed]

    def evaluate(self, inputs, *, outputs=None):
        if (
            not isinstance(inputs, Mapping)
            or set(inputs) != set(self.inputs)
            or any(
                not isinstance(p, Product) or p.type_id != self.inputs[n]
                for n, p in inputs.items()
            )
        ):
            raise ValueError("Instance input names/types disagree with the recipe")
        names, required = self.required(outputs)
        input_bytes = sum(p.nbytes for p in inputs.values())
        reserved = sum(n.max_output_bytes for n in required)
        if input_bytes + reserved > self.budget.max_product_bytes:
            raise ValueError(
                "Declared product reservation exceeds budget before operator evaluation"
            )
        products = {}

        def get(port):
            return (
                inputs[port.name]
                if port.node is None
                else products[port.node][port.name]
            )

        actual_bytes = 0
        for node in required:
            args = {n: get(p) for n, p in node.inputs.items()}
            node_key = digest(
                {
                    "definition": node.owner_digest,
                    "node": node.path[-1],
                    "operator": {
                        "id": node.operator.key.id,
                        "version": node.operator.key.version,
                        "inputs": node.operator.inputs,
                        "outputs": node.operator.outputs,
                    },
                    "parameters": node.parameters,
                    "inputs": {n: signature(p) for n, p in args.items()},
                }
            )
            result = node.operator.apply(node.parameters, MappingProxyType(args))
            if (
                not isinstance(result, Mapping)
                or set(result) != set(node.operator.outputs)
                or any(type(p) is not Emission for p in result.values())
            ):
                raise ValueError(
                    "Operator result ports/Emission types disagree with its registration"
                )
            measured = sum(p.nbytes for p in result.values())
            if measured > node.max_output_bytes:
                raise ValueError(
                    "Operator exceeded its declared accounted output bytes"
                )
            actual_bytes += measured
            products[node.path] = {
                n: Product(
                    p.value,
                    p.nbytes,
                    p.metadata,
                    node.operator.outputs[n],
                    digest({"node": node_key, "output": n, "metadata": p.metadata}),
                )
                for n, p in result.items()
            }
        return Run(
            MappingProxyType({n: get(self.outputs[n]) for n in names}),
            self.definition_fingerprint,
            digest(
                {
                    "definition": self.definition_fingerprint,
                    "inputs": {n: signature(p) for n, p in inputs.items()},
                }
            ),
            immutable(
                {
                    "loaded_definitions": len(self.definitions),
                    "definition_bytes": self.definition_bytes,
                    "compiled_nodes": len(self.nodes),
                    "evaluated_nodes": len(required),
                    "input_bytes": input_bytes,
                    "reserved_output_bytes": reserved,
                    "accounted_emission_bytes": actual_bytes,
                }
            ),
        )
