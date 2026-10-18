"""Portable loading, typed bounded preflight and selective product invalidation."""

import copy
import json
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

from recipe_graph import Emission, Operator, Prepared, external
from recipe_graph_data import Budget, Files, Key, canonical, definition, digest

FORM, SITE, SCALAR = "test.form/1", "test.site/1", "test.scalar/1"
BASE, ROOT = Key("test.form", "01"), Key("test.placement", "1")


def data():
    base = {
        "schema_version": "1",
        "id": BASE.id,
        "version": BASE.version,
        "requires": [],
        "inputs": {"seed": SCALAR},
        "dependencies": {},
        "nodes": {
            "form": {
                "operator": {"id": "test.form", "version": "1"},
                "parameters": {"scale": 2},
                "inputs": {"seed": {"input": "seed"}},
                "max_output_bytes": 8,
            }
        },
        "outputs": {"form": {"node": "form", "output": "form"}},
    }
    root = {
        "schema_version": "1",
        "id": ROOT.id,
        "version": ROOT.version,
        "requires": ["test.placement"],
        "inputs": {"seed": SCALAR, "site": SITE},
        "dependencies": {
            "shape": {
                "id": BASE.id,
                "version": BASE.version,
                "inputs": {"seed": "seed"},
            }
        },
        "nodes": {
            "place": {
                "operator": {"id": "test.place", "version": "1"},
                "parameters": {},
                "inputs": {
                    "form": {"dependency": "shape", "output": "form"},
                    "site": {"input": "site"},
                },
                "max_output_bytes": 8,
            }
        },
        "outputs": {
            "form": {"dependency": "shape", "output": "form"},
            "placed": {"node": "place", "output": "placed"},
        },
    }
    return {BASE: base, ROOT: root}


def products(seed=3, site=10):
    return {
        "seed": external(SCALAR, seed, seed, 8, {"representation": "binary64"}),
        "site": external(
            SITE,
            site,
            site,
            8,
            {
                "frame": "test-enu",
                "units": "m",
                "height_datum": "local-ground0",
                "representation": "binary64",
            },
        ),
    }


def registered(calls):
    def form(params, inputs):
        calls.append("form")
        return {
            "form": Emission(
                inputs["seed"].value * params["scale"],
                8,
                {"frame": "plant-local", "units": "m", "representation": "binary64"},
            )
        }

    def place(params, inputs):
        calls.append("placed")
        return {
            "placed": Emission(
                inputs["form"].value + inputs["site"].value, 8, inputs["site"].metadata
            )
        }

    return [
        Operator(Key("test.form", "1"), {"seed": SCALAR}, {"form": FORM}, form),
        Operator(
            Key("test.place", "1"),
            {"form": FORM, "site": SITE},
            {"placed": FORM},
            place,
        ),
    ]


def prepare(rows=None, calls=None, budget=None, capabilities=("test.placement",)):
    rows = data() if rows is None else rows
    calls = [] if calls is None else calls

    def load(key, remaining):
        if key not in rows:
            raise ValueError("Missing exact recipe version")
        return json.dumps(rows[key]).encode()

    return Prepared(ROOT, load, registered(calls), capabilities, budget)


class RecipeGraph(unittest.TestCase):
    def test_json_roundtrip_exact_ids_immutable_data_and_requested_subgraph(self):
        calls = []
        rows = data()
        p = prepare(rows, calls)
        self.assertEqual(p.definitions[BASE]["version"], "01")
        with self.assertRaises(TypeError):
            p.definitions[BASE]["nodes"]["form"]["parameters"]["scale"] = 5
        plan = p.evaluate(products(), outputs=("form",))
        self.assertEqual(calls, ["form"])
        calls.clear()
        whole = p.evaluate(products())
        self.assertEqual(calls, ["form", "placed"])
        self.assertEqual(plan.outputs["form"].value, 6)
        self.assertEqual(whole.outputs["placed"].value, 16)
        self.assertEqual(
            plan.outputs["form"].fingerprint, whole.outputs["form"].fingerprint
        )
        self.assertEqual(plan.instance_fingerprint, whole.instance_fingerprint)
        roundtrip = {k: json.loads(canonical(v)) for k, v in rows.items()}
        replay = prepare(roundtrip).evaluate(products())
        self.assertEqual(whole.definition_fingerprint, replay.definition_fingerprint)
        self.assertEqual(whole.instance_fingerprint, replay.instance_fingerprint)
        self.assertEqual(
            [p.fingerprint for p in whole.outputs.values()],
            [p.fingerprint for p in replay.outputs.values()],
        )

    def test_site_only_invalidates_placement_seed_invalidates_both(self):
        p = prepare()
        before = p.evaluate(products())
        other = p.evaluate(products(site=20))
        seed = p.evaluate(products(seed=4))
        self.assertEqual(
            before.outputs["form"].fingerprint, other.outputs["form"].fingerprint
        )
        self.assertNotEqual(
            before.outputs["placed"].fingerprint, other.outputs["placed"].fingerprint
        )
        self.assertNotEqual(before.instance_fingerprint, other.instance_fingerprint)
        for name in before.outputs:
            self.assertNotEqual(
                before.outputs[name].fingerprint, seed.outputs[name].fingerprint
            )
        self.assertEqual(before.definition_fingerprint, other.definition_fingerprint)

    def test_exact_dependency_version_and_same_version_content_changes_invalidate(self):
        a = prepare().evaluate(products())
        rows = data()
        rows[BASE]["nodes"]["form"]["parameters"]["scale"] = 3
        b = prepare(rows).evaluate(products())
        self.assertNotEqual(a.definition_fingerprint, b.definition_fingerprint)
        self.assertNotEqual(
            a.outputs["placed"].fingerprint, b.outputs["placed"].fingerprint
        )
        rows[ROOT]["dependencies"]["shape"]["version"] = "02"
        with self.assertRaises(ValueError):
            prepare(rows)

    def test_invalid_ports_types_versions_and_node_cycles_reject_before_work(self):
        for mutation in (
            lambda r: r[ROOT]["nodes"]["place"]["operator"].update(version="missing"),
            lambda r: r[ROOT]["nodes"]["place"]["inputs"]["form"].update(
                output="missing"
            ),
            lambda r: r[ROOT]["nodes"]["place"]["inputs"].update(
                form={"input": "site"}
            ),
            lambda r: r[ROOT]["nodes"]["place"]["inputs"].update(
                form={"node": "place", "output": "placed"}
            ),
            lambda r: r[ROOT]["dependencies"]["shape"]["inputs"].update(seed="missing"),
            lambda r: r[ROOT]["inputs"].update(seed=SITE),
        ):
            rows = data()
            mutation(rows)
            calls = []
            with self.assertRaises(ValueError):
                prepare(rows, calls)
            self.assertFalse(calls)

    def test_recipe_cycle_missing_capability_and_structural_budgets_reject_before_work(
        self,
    ):
        rows = data()
        rows[BASE]["dependencies"]["cycle"] = {
            "id": ROOT.id,
            "version": ROOT.version,
            "inputs": {},
        }
        with self.assertRaisesRegex(ValueError, "Cyclic"):
            prepare(rows)
        calls = []
        with self.assertRaisesRegex(ValueError, "capabilities"):
            prepare(calls=calls, capabilities=())
        for budget in (
            Budget(max_definitions=1),
            Budget(max_definition_bytes=20),
            Budget(max_nodes=1),
            Budget(max_depth=1),
        ):
            with self.assertRaises(ValueError):
                prepare(calls=calls, budget=budget)
        self.assertFalse(calls)

    def test_product_reservation_input_types_and_unknown_output_fail_before_work(self):
        calls = []
        p = prepare(calls=calls, budget=Budget(max_product_bytes=24))
        self.assertEqual(
            p.evaluate(products(), outputs=("form",)).outputs["form"].value, 6
        )
        calls.clear()
        with self.assertRaisesRegex(ValueError, "reservation"):
            p.evaluate(products())
        for inputs in (
            {},
            products() | {"wind_phase": products()["seed"]},
            {"seed": products()["site"], "site": products()["site"]},
        ):
            with self.assertRaises(ValueError):
                p.evaluate(inputs)
        with self.assertRaises(ValueError):
            p.evaluate(products(), outputs=("missing",))
        self.assertFalse(calls)

    def test_operator_result_accounting_and_ports_are_checked(self):
        for result in ({"form": Emission(0, 9)}, {}, {"form": 0}):
            p = prepare()
            node = p.nodes[("shape", "form")]
            bad = replace(
                node.operator, apply=lambda params, inputs, result=result: result
            )
            rows = data()
            ops = registered([])
            ops[0] = bad
            p = Prepared(
                ROOT,
                lambda k, n, rows=rows: json.dumps(rows[k]).encode(),
                ops,
                ("test.placement",),
            )
            with self.assertRaises(ValueError):
                p.evaluate(products())

    def test_portable_numbers_duplicate_keys_and_schema_reject(self):
        rows = data()
        value = rows[BASE]
        for bad in (float("nan"), float("inf"), 2**53):
            changed = copy.deepcopy(value)
            changed["nodes"]["form"]["parameters"]["bad"] = bad
            with self.assertRaises(ValueError):
                definition(json.dumps(changed).encode(), BASE)
        for raw in (b'{"id":"a","id":"b"}', b"[]", b"null"):
            with self.assertRaises(ValueError):
                definition(raw, BASE)
        with self.assertRaises((ValueError, TypeError)):
            Key("x", 1)
        with self.assertRaises(ValueError):
            Budget(max_nodes=True)

    def test_finite_metadata_and_binary64_safe_integer_identity(self):
        with self.assertRaises(ValueError):
            external(SCALAR, 0, 0, 8, {"bad": float("nan")})
        with self.assertRaises(ValueError):
            Emission(0, True)
        self.assertNotEqual(
            digest({"id": "9007199254740993"}), digest({"id": "9007199254740992"})
        )

    def test_file_loader_only_reads_selected_closure_and_enforces_byte_cap(self):
        with tempfile.TemporaryDirectory(prefix="outshine-recipe-tests-") as tmp:
            paths = {}
            for index, (key, row) in enumerate(data().items()):
                path = Path(tmp) / f"{index}.json"
                path.write_text(json.dumps(row))
                paths[key] = path
            paths[Key("unused", "1")] = Path(tmp) / "must-not-open.json"
            p = Prepared(ROOT, Files(paths), registered([]), ("test.placement",))
            self.assertEqual(p.evaluate(products()).stats["loaded_definitions"], 2)
            with self.assertRaises(ValueError):
                Files(paths)(ROOT, 5)


if __name__ == "__main__":
    unittest.main()
