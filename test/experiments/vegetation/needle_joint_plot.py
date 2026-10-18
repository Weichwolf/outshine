"""Common-exposure finite-source direct-light diagnostic, not a beauty render."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def plot(pictures, path):
    fig, axes = plt.subplots(3, 3, figsize=(16, 9), dpi=160)
    for row, (view, light) in enumerate(((0, 0), (0, 3), (1, 0))):
        for column, label in enumerate(
            (
                "unshadowed source",
                "same-contact shadowed source",
                "independent cosine × visibility means",
            )
        ):
            axes[row, column].imshow(
                pictures[view][light, column],
                vmin=0,
                vmax=1,
                cmap="gray",
                interpolation="nearest",
            )
            axes[row, column].set_title(
                f"{label}\nview{view} / light{light} / native64×64"
            )
            axes[row, column].axis("off")
    fig.suptitle(
        "Actual selected twig / opaque unit diffuse / common linear range[0,1]"
    )
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)
