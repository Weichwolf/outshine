"""Held-out views and fixed world lights, common linear response scale."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def plot(arrays, path):
    fig, axes = plt.subplots(3, 4, figsize=(16, 9), dpi=160)
    for view in range(3):
        for column, (order, light) in enumerate(((2, 2), (4, 2), (2, 3), (4, 3))):
            response = arrays[f"view-{view}-q{order}"][2 + 2 * light]
            axes[view, column].imshow(
                response, vmin=0, vmax=1, cmap="gray", interpolation="nearest"
            )
            axes[view, column].set_title(
                f"view{view} / world-light{light - 2} / q{order}"
            )
            axes[view, column].axis("off")
    fig.suptitle("Held-out source-area views/lights / native16×16 / unchanged geometry")
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)
