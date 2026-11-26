#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Score the SteamiScreen mockup renders against the tutorials' reference images.

Usage:

    STEAMI_PPM_DIR=/tmp/mockups pio test -e native -f native/test_steami_screen_mockups
    python3 scripts/mockup-review.py --ppm-dir /tmp/mockups --out /tmp/mockup-review \\
        [--refs DIR]

Each mockup is compared at 128 px (grey OLED) and 240 px (colour). The
reference (384x384) is resized with Lanczos; for the 128 px panel it is first
mapped to the 16 OLED grey levels exactly as the render stores them. Both
images are masked outside 0.95 of the panel radius (the bezel ring is the
panel edge, not a widget) and compared with SSIM (scikit-image's defaults).

The script writes one side-by-side PNG (reference, render, difference) per
mockup and size, `ssim.csv` and `index.html` into --out. The scores are
indicative: the exit code is 0 whatever they are. Without --refs the
references are fetched from the pinned tutorials commit.

Exit codes: 0 done, 1 a PPM or a reference is missing, 2 Pillow or NumPy is
missing.
"""

from __future__ import annotations

import argparse
import html
import sys
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path


TUTORIALS_COMMIT = "83d8e2ca23abb0eeca7afca2d001cdd95b1df76b"
MOCKUPS: tuple[str, ...] = ("01_temperature", "02_battery", "03_comfort_dual")
PANELS: tuple[tuple[int, str], ...] = ((128, "gray"), (240, "color"))
MASK_RATIO = 0.95
SSIM_HINT = 0.7

REF_URL = (
    "https://raw.githubusercontent.com/steamicc/micropython-steami-tutorials/"
    "{commit}/docs/mockups/{name}_ref.png"
)


class ReviewError(Exception):
    pass


@dataclass(frozen=True)
class Row:
    name: str
    size: int
    score: float


def ref_url(name: str) -> str:
    return REF_URL.format(commit=TUTORIALS_COMMIT, name=name)


def ppm_name(name: str, size: int, mode: str) -> str:
    return f"steami-mockup-{name}-{size}-{mode}.ppm"


def _expand565(r5, g6, b5):
    return (r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2)


def oled_grey(rgb):
    """Map a uint8 HxWx3 image to the 16 grey levels, as the grey render stores them."""
    import numpy as np

    px = rgb.astype(np.int32)
    r, g, b = _expand565(px[..., 0] >> 3, px[..., 1] >> 2, px[..., 2] >> 3)
    level = ((r * 77 + g * 150 + b * 29) >> 8) >> 4
    v = level * 17
    r, g, b = _expand565(v >> 3, v >> 2, v >> 3)
    return np.stack([r, g, b], axis=-1).astype(np.uint8)


def disc_mask(size: int, ratio: float):
    """True inside the disc of radius ratio * size / 2 around (size / 2, size / 2), indexed [y, x]."""
    import numpy as np

    c = size // 2
    y, x = np.ogrid[:size, :size]
    radius = ratio * size / 2
    return (x - c) ** 2 + (y - c) ** 2 <= radius * radius


def _window_means(x, win: int):
    """Mean over every full win x win window of an HxW array (valid positions only)."""
    import numpy as np

    c = np.cumsum(np.cumsum(x, axis=0), axis=1)
    c = np.pad(c, ((1, 0), (1, 0)))
    s = c[win:, win:] - c[:-win, win:] - c[win:, :-win] + c[:-win, :-win]
    return s / (win * win)


def ssim(a, b) -> float:
    """SSIM of two uint8 HxWx3 images with scikit-image's defaults (7x7 uniform window)."""
    import numpy as np

    win = 7
    pad = (win - 1) // 2
    npix = win * win
    cov_norm = npix / (npix - 1)
    c1 = (0.01 * 255) ** 2
    c2 = (0.03 * 255) ** 2
    total = 0.0
    for ch in range(a.shape[-1]):
        x = a[..., ch].astype(np.float64)
        y = b[..., ch].astype(np.float64)
        ux = _window_means(x, win)
        uy = _window_means(y, win)
        vx = cov_norm * (_window_means(x * x, win) - ux * ux)
        vy = cov_norm * (_window_means(y * y, win) - uy * uy)
        vxy = cov_norm * (_window_means(x * y, win) - ux * uy)
        s = ((2 * ux * uy + c1) * (2 * vxy + c2)) / ((ux * ux + uy * uy + c1) * (vx + vy + c2))
        # skimage filters the whole image then crops pad px; valid windows are the same pixels.
        assert s.shape == (a.shape[0] - 2 * pad, a.shape[1] - 2 * pad)
        total += float(s.mean())
    return total / a.shape[-1]


def _load_ref(name: str, refs_dir: Path | None) -> bytes:
    if refs_dir is not None:
        path = refs_dir / f"{name}_ref.png"
        try:
            return path.read_bytes()
        except OSError as exc:
            raise ReviewError(f"cannot read the reference {path}: {exc}") from exc
    url = ref_url(name)
    try:
        with urllib.request.urlopen(url, timeout=30) as response:
            return response.read()
    except (urllib.error.URLError, OSError) as exc:
        raise ReviewError(f"cannot fetch the reference {url}: {exc}") from exc


def _open_rgb(source, what: str):
    import numpy as np
    from PIL import Image

    try:
        with Image.open(source) as img:
            return np.asarray(img.convert("RGB"), dtype=np.uint8)
    except (OSError, ValueError) as exc:
        raise ReviewError(f"cannot read {what}: {exc}") from exc


def review(ppm_dir: Path, out_dir: Path, refs_dir: Path | None) -> list[Row]:
    import io

    import numpy as np
    from PIL import Image

    out_dir.mkdir(parents=True, exist_ok=True)
    rows: list[Row] = []
    for name in MOCKUPS:
        ref = Image.fromarray(_open_rgb(io.BytesIO(_load_ref(name, refs_dir)), f"{name} ref"))
        for size, mode in PANELS:
            ppm = ppm_dir / ppm_name(name, size, mode)
            render = _open_rgb(ppm, str(ppm))
            if render.shape != (size, size, 3):
                raise ReviewError(f"{ppm} is {render.shape[1]}x{render.shape[0]}, not {size}x{size}")
            expected = np.asarray(ref.resize((size, size), Image.LANCZOS), dtype=np.uint8)
            if mode == "gray":
                expected = oled_grey(expected)
            keep = disc_mask(size, MASK_RATIO)[..., None]
            expected = np.where(keep, expected, 0).astype(np.uint8)
            render = np.where(keep, render, 0).astype(np.uint8)
            diff = np.abs(expected.astype(np.int16) - render.astype(np.int16)).astype(np.uint8)
            side = np.concatenate([expected, render, diff], axis=1)
            Image.fromarray(side).save(out_dir / f"{name}-{size}.png")
            rows.append(Row(name, size, ssim(expected, render)))
    (out_dir / "ssim.csv").write_text(
        "name,size,ssim\n" + "".join(f"{r.name},{r.size},{r.score:.4f}\n" for r in rows)
    )
    (out_dir / "index.html").write_text(_page(rows), encoding="utf-8")
    return rows


def _page(rows: list[Row]) -> str:
    body = []
    for r in rows:
        look = " <strong>look</strong>" if r.score < SSIM_HINT else ""
        title = html.escape(f"{r.name} at {r.size} px")
        body.append(
            f"<tr><td>{title}</td>"
            f'<td><img src="{html.escape(r.name)}-{r.size}.png" alt="{title}: reference, render, difference"></td>'
            f"<td>{r.score:.4f}{look}</td></tr>"
        )
    return (
        "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<title>SteamiScreen mockup review</title>"
        "<style>body{font-family:sans-serif;margin:1em}td{padding:.4em;vertical-align:middle}"
        "img{image-rendering:pixelated;background:#000}</style></head><body>\n"
        "<h1>SteamiScreen mockups against the tutorials</h1>\n"
        "<p>Each image shows the reference, the render and their absolute difference, masked outside "
        f"{MASK_RATIO} of the radius. SSIM is indicative; \"look\" marks scores under {SSIM_HINT}.</p>\n"
        "<table><tr><th>Mockup</th><th>Reference, render, difference</th><th>SSIM</th></tr>\n"
        + "\n".join(body)
        + "\n</table></body></html>\n"
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--ppm-dir", type=Path, required=True, help="folder of the render PPMs")
    parser.add_argument("--out", type=Path, required=True, help="folder for the review page")
    parser.add_argument("--refs", type=Path, help="folder of the *_ref.png (default: fetch)")
    args = parser.parse_args(argv)
    try:
        import numpy  # noqa: F401
        import PIL  # noqa: F401
    except ImportError as exc:
        print(f"mockup-review: Pillow and NumPy are required ({exc})", file=sys.stderr)
        return 2
    try:
        rows = review(args.ppm_dir, args.out, args.refs)
    except ReviewError as exc:
        print(f"mockup-review: {exc}", file=sys.stderr)
        return 1
    for r in rows:
        print(f"{r.name} {r.size} px: SSIM {r.score:.4f}")
    print(f"mockup-review: wrote {args.out / 'index.html'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
