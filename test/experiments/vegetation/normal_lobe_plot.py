"""Identical unshadowed finite source, old mean and nonnegative first-moment lobes."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def plot(arrays, path):
    fig, axes = plt.subplots(3, 5, figsize=(16, 9), dpi=160)
    names = ("exact", "normalized-mean", "4", "unit-6", "24")
    for row, light in enumerate((0, 3, 8)):
        for column, name in enumerate(names):
            axes[row, column].imshow(
                arrays[f"16-{name}"][light].reshape(16, 16),
                vmin=0,
                vmax=1,
                cmap="gray",
                interpolation="nearest",
            )
            axes[row, column].set_title(f"{name} / light{light}")
            axes[row, column].axis("off")
    fig.suptitle(
        "Same visible source / no shadows / unit diffuse / native16×16 / no fitted lights"
    )
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)
