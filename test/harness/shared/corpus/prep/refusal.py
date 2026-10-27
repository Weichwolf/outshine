"""The one way this tool says no."""


class Refusal(Exception):
    """A refusal names the context, what was expected and what was observed."""

    def __init__(self, context, expected=None, observed=None, why=None):
        self.context = context
        self.expected = expected
        self.observed = observed
        self.why = why
        parts = ["refused: " + str(context)]
        if why:
            parts.append("  why      : " + str(why))
        if expected is not None:
            parts.append("  expected : " + str(expected))
        if observed is not None:
            parts.append("  observed : " + str(observed))
        super().__init__("\n".join(parts))
