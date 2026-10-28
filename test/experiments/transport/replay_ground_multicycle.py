"""Same-bound whole original multi-loop ground comparison and retained countercases."""

from ground_cyclic_checks import audit
from ground_cyclic_inputs import prepare
from ground_cyclic_replay import arguments, replay
from ground_multicycle import generate


def prepare_inputs():
    return prepare(minimum_cycles=3, maximum_cycles=8, maximum_spans=32)


def check(result):
    geometry = result["geometry"]
    cycles = len(geometry["source_edges_local"]) - len(geometry["source_node_ids"]) + 1
    return audit(result, expected_holes=cycles, prefix="multicycle")


def main():
    replay(
        arguments(__doc__),
        prepare_inputs=prepare_inputs,
        producer=generate,
        check=check,
        schema="outshine/actual-multicycle-ground-comparison/1",
        plot_title="Whole original multi-loop component: all25 spans, six free regions and54 shared sections\nNo original-qualified ends, canonical Source/World/terrain/device admission",
    )


if __name__ == "__main__":
    main()
