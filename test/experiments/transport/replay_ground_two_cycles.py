"""Same-input adaptive/all-node-LP comparison for actual whole two-loop components."""

from ground_cyclic_checks import audit
from ground_cyclic_inputs import prepare
from ground_cyclic_replay import arguments, replay
from ground_two_cycles import generate


def main():
    replay(
        arguments(__doc__),
        prepare_inputs=prepare,
        producer=generate,
        check=audit,
        schema="outshine/actual-two-cycle-ground-comparison/1",
    )


if __name__ == "__main__":
    main()
