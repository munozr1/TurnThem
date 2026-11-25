#!/usr/bin/env python3
"""Pack the generated 3-column, 2-row artillery sheet into the existing atlas.

Run from any directory with Python 3 and Pillow installed. This is an asset
preparation tool, not a build dependency. All frames keep the same canvas and
pivot: column 0 is rest, 1 is recoil, and 2 is recovery.
"""
import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
SOURCE = ROOT / "art" / "artillery_source.png"


def main():
    source = Image.open(SOURCE).convert("RGBA")
    if source.width % 3 or source.height % 2:
        raise ValueError("Source must contain three equal columns and two equal rows")
    cell_w, cell_h = source.width // 3, source.height // 2
    metadata_path = ASSETS / "sprites_data_file.json"
    metadata = json.loads(metadata_path.read_text())
    atlas_path = ASSETS / "sprites_texture_file.png"
    atlas = Image.open(atlas_path).convert("RGBA")
    for row, name in enumerate(("cannon", "scannon")):
        frames = []
        for column in range(3):
            # Fixed cells, never per-frame trimming: the chassis must not jump.
            bounds = (column * cell_w, row * cell_h,
                      (column + 1) * cell_w, (row + 1) * cell_h)
            frame = source.crop(bounds).resize((50, 65), Image.Resampling.LANCZOS)
            key = f"{name}_frame_{column}.png"
            frame.save(ASSETS / "sprites" / key)
            entry = metadata["frames"][key]["frame"]
            entry.update(w=50, h=65)
            # Replace RGBA pixels (including transparent pixels), don't composite
            # over the old silhouette or leave its edges behind.
            atlas.paste(frame, (entry["x"], entry["y"]))
            frames.append(frame)

        # Keep the standalone authoring sheet consistent with the runtime atlas.
        strip = Image.new("RGBA", (150, 65))
        for column, frame in enumerate(frames):
            strip.paste(frame, (column * 50, 0))
        strip.save(ASSETS / f"{name}_sprite_sheet.png")

        # Existing card bounds remain intact; cards/ghosts in the new tray also
        # read frame 0 directly, so all entry points use the same resting art.
        key = f"{name}_card.png"
        card = Image.open(ASSETS / "sprites" / key).convert("RGBA")
        # Preserve the original title strip, replace only the weapon illustration.
        card.paste((216, 216, 216, 255), (0, 30, card.width, card.height))
        card.alpha_composite(frames[0], ((card.width - 50) // 2, card.height - 65))
        card.save(ASSETS / "sprites" / key)
        entry = metadata["frames"][key]["frame"]
        atlas.paste(card, (entry["x"], entry["y"]))

    atlas.save(atlas_path)
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    main()
