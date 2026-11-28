"""Present the historical native image and both generated images at the same scale."""

from PIL import Image, ImageDraw

from .catalog import ROOT

frames = (
    ("Outshine native", ROOT / "doc/images/wien.png"),
    ("Python / Blender EEVEE", ROOT / "build/shots/places/Wien.png"),
    ("Python / Outshine GLSL", ROOT / "build/shots/glsl/Wien.png"),
)
if __name__ == "__main__":
    sheet = Image.new("RGB", (960, 3 * (540 + 25)), "#202020")
    draw = ImageDraw.Draw(sheet)
    for row, (name, path) in enumerate(frames):
        with Image.open(path) as image:
            if image.size != (1280, 720):
                raise ValueError(f"{path}: expected 1280x720")
            y = row * 565
            draw.text((8, y + 5), name, fill="white")
            sheet.paste(image.resize((960, 540), Image.Resampling.LANCZOS), (0, y + 25))
    output = ROOT / "build/shots/comparison/Wien.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
