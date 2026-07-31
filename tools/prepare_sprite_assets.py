from pathlib import Path
import os

import numpy as np
from PIL import Image, ImageFilter


def propagate(seed: np.ndarray, allowed: np.ndarray) -> np.ndarray:
    current = seed.copy()
    while True:
        grown = current.copy()
        grown[1:, :] |= current[:-1, :]
        grown[:-1, :] |= current[1:, :]
        grown[:, 1:] |= current[:, :-1]
        grown[:, :-1] |= current[:, 1:]
        grown &= allowed
        if np.array_equal(grown, current):
            return current
        current = grown


def prepare(source: Path, destination: Path) -> None:
    image = Image.open(source).convert("RGB")
    rgb = np.asarray(image)
    maximum = rgb.max(axis=2)
    minimum = rgb.min(axis=2)

    # The baked checkerboard is bright and nearly neutral. Propagating only
    # from the canvas border prevents the cat's enclosed cream fur being lost.
    background_candidate = (minimum >= 235) & ((maximum - minimum) <= 9)
    border = np.zeros(background_candidate.shape, dtype=bool)
    border[0, :] = border[-1, :] = True
    border[:, 0] = border[:, -1] = True
    background = propagate(border & background_candidate, background_candidate)
    possible_subject = ~background

    # Keep only the connected region containing the cat's central body. This
    # removes detached emotion symbols and decorative particles.
    h, w = possible_subject.shape
    seed = np.zeros_like(possible_subject)
    for dy in range(-30, 31):
        for dx in range(-30, 31):
            y, x = h // 2 + dy, w // 2 + dx
            if possible_subject[y, x]:
                seed[y, x] = True
    subject = propagate(seed, possible_subject)

    alpha = Image.fromarray((subject * 255).astype(np.uint8), "L")
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.65))
    rgba = image.convert("RGBA")
    rgba.putalpha(alpha)

    box = alpha.getbbox()
    if not box:
        raise RuntimeError(f"No subject detected in {source}")
    sprite = rgba.crop(box)
    scale = min(880 / sprite.width, 880 / sprite.height)
    sprite = sprite.resize(
        (round(sprite.width * scale), round(sprite.height * scale)),
        Image.Resampling.LANCZOS,
    )
    canvas = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    canvas.alpha_composite(sprite, ((1024 - sprite.width) // 2, 952 - sprite.height))
    destination.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(destination, "PNG", optimize=True)


if __name__ == "__main__":
    prepare(Path(os.environ["SPRITE_SOURCE"]), Path(os.environ["SPRITE_DESTINATION"]))
