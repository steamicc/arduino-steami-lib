# SPDX-License-Identifier: GPL-3.0-or-later
"""Unit tests for scripts/mockup-review.py."""

from __future__ import annotations

import importlib.util
import io
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import skipUnless


ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT = ROOT / "scripts" / "mockup-review.py"

try:
    import numpy as np
    from PIL import Image

    HAVE_NUMPY_PIL = True
except ImportError:
    HAVE_NUMPY_PIL = False

try:
    from skimage.metrics import structural_similarity

    HAVE_SKIMAGE = True
except ImportError:
    HAVE_SKIMAGE = False


def load_script():
    spec = importlib.util.spec_from_file_location("mockup_review", SCRIPT)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


mr = load_script()


def noise_pair():
    rng = np.random.default_rng(0)
    a = rng.integers(0, 256, (64, 64, 3), dtype=np.uint8)
    b = np.clip(a + rng.normal(0, 40, a.shape), 0, 255).astype(np.uint8)
    return a, b


def make_inputs(root: Path, skip: tuple[str, ...] = ()) -> tuple[Path, Path]:
    """Synthetic refs and PPMs, unrelated to each other."""
    refs = root / "refs"
    ppms = root / "ppm"
    refs.mkdir()
    ppms.mkdir()
    rng = np.random.default_rng(1)
    for name in mr.MOCKUPS:
        pixels = rng.integers(0, 256, (384, 384, 3), dtype=np.uint8)
        Image.fromarray(pixels).save(refs / f"{name}_ref.png")
        for size, mode in mr.PANELS:
            file = ppms / mr.ppm_name(name, size, mode)
            if file.name in skip:
                continue
            pixels = rng.integers(0, 256, (size, size, 3), dtype=np.uint8)
            Image.fromarray(pixels).save(file, format="PPM")
    return refs, ppms


class MockupReviewTest(unittest.TestCase):
    def test_ref_url_is_pinned(self) -> None:
        self.assertEqual(
            mr.ref_url("01_temperature"),
            "https://raw.githubusercontent.com/steamicc/micropython-steami-"
            "tutorials/83d8e2ca23abb0eeca7afca2d001cdd95b1df76b/docs/mockups/"
            "01_temperature_ref.png",
        )

    def test_ppm_name(self) -> None:
        self.assertEqual(
            mr.ppm_name("02_battery", 240, "color"), "steami-mockup-02_battery-240-color.ppm"
        )

    @skipUnless(HAVE_NUMPY_PIL, "Pillow and NumPy")
    def test_ssim_identity_symmetry_and_noise(self) -> None:
        a, b = noise_pair()
        self.assertAlmostEqual(mr.ssim(a, a), 1.0, places=6)
        self.assertAlmostEqual(mr.ssim(a, b), mr.ssim(b, a), places=12)
        self.assertLess(mr.ssim(a, b), 0.9)

    @skipUnless(HAVE_SKIMAGE, "scikit-image")
    def test_ssim_matches_scikit_image(self) -> None:
        a, b = noise_pair()
        expected = structural_similarity(a, b, channel_axis=-1)
        self.assertAlmostEqual(mr.ssim(a, b), expected, delta=1e-6)

    @skipUnless(HAVE_NUMPY_PIL, "Pillow and NumPy")
    def test_oled_grey_levels(self) -> None:
        px = np.array([[[85, 85, 255], [51, 51, 51], [255, 255, 255]]], dtype=np.uint8)
        out = mr.oled_grey(px)
        self.assertEqual(out.dtype, np.uint8)
        self.assertEqual(out[0, 0].tolist(), [99, 101, 99])
        self.assertEqual(out[0, 1].tolist(), [49, 48, 49])
        self.assertEqual(out[0, 2].tolist(), [255, 255, 255])

    @skipUnless(HAVE_NUMPY_PIL, "Pillow and NumPy")
    def test_disc_mask(self) -> None:
        mask = mr.disc_mask(128, 0.95)
        self.assertEqual(mask.shape, (128, 128))
        self.assertTrue(mask[64, 64])
        self.assertFalse(mask[0, 0])
        self.assertFalse(mask[2, 64])
        self.assertTrue(mask[4, 64])

    @skipUnless(HAVE_NUMPY_PIL, "Pillow and NumPy")
    def test_review_writes_the_page_and_never_fails_on_scores(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            refs, ppms = make_inputs(root)
            out = root / "out"
            with redirect_stdout(io.StringIO()):
                code = mr.main(["--ppm-dir", str(ppms), "--out", str(out), "--refs", str(refs)])
            self.assertEqual(code, 0)
            self.assertTrue((out / "index.html").is_file())
            rows = (out / "ssim.csv").read_text().strip().splitlines()
            self.assertEqual(len(rows), 7)
            self.assertEqual(rows[0], "name,size,ssim")
            self.assertEqual(len(list(out.glob("*.png"))), 6)
            self.assertIn("look", (out / "index.html").read_text())

    @skipUnless(HAVE_NUMPY_PIL, "Pillow and NumPy")
    def test_missing_ppm_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            refs, ppms = make_inputs(root, skip=(mr.ppm_name("02_battery", 128, "gray"),))
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                code = mr.main(
                    ["--ppm-dir", str(ppms), "--out", str(root / "out"), "--refs", str(refs)]
                )
            self.assertEqual(code, 1)


if __name__ == "__main__":
    unittest.main()
