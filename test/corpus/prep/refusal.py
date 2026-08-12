"""The one way this tool says no."""


class Refusal(Exception):
    """A refusal names the subject, what was expected and what was observed."""

    def __init__(self, subject, expected=None, observed=None, why=None):
        self.subject = subject
        self.expected = expected
        self.observed = observed
        self.why = why
        parts = ["refused: " + str(subject)]
        if why:
            parts.append("  why      : " + str(why))
        if expected is not None:
            parts.append("  expected : " + str(expected))
        if observed is not None:
            parts.append("  observed : " + str(observed))
        super().__init__("\n".join(parts))
