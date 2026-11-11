"""Import the owner's PNG as an XMB icon; preserve artwork and aspect ratio.

This only fits the supplied image in the required 320x176 PNG canvas.
No artwork is generated or retouched. The original source remains untouched.
"""
import argparse
import shutil
from pathlib import Path
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    assets = ROOT / "app" / "assets"
    source_copy = assets / "icon-source.png"
    if args.source.resolve() != source_copy.resolve():
        shutil.copyfile(args.source, source_copy)
    with Image.open(source_copy) as source:
        fitted = ImageOps.contain(source.convert("RGBA"), (320, 176), Image.Resampling.LANCZOS)
    icon = Image.new("RGBA", (320, 176), (0, 0, 0, 0))
    icon.alpha_composite(fitted, ((320-fitted.width)//2, (176-fitted.height)//2))
    output = assets / "ICON0.PNG"
    icon.save(output, optimize=True)
    print(f"Imported owner artwork: {output} (320x176 RGBA PNG, original alpha preserved)")

if __name__ == "__main__":
    main()
