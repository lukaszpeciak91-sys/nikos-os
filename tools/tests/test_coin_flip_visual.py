#!/usr/bin/env python3
"""Raster-test production Coin Flip drawing with real font assets and themes."""
from pathlib import Path
import subprocess
import shutil
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("SKIP: no host C++17 compiler")
with tempfile.TemporaryDirectory() as path:
    exe = str(Path(path) / "coin_flip_visual_test")
    subprocess.run([
        compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I" + str(root / "tools/tests/coin_flip_stubs"),
        "-I" + str(root / "apps/coin_flip/include"),
        "-I" + str(root / "components/font_core/include"),
        "-I" + str(root / "components/ui_theme/include"),
        str(root / "tools/tests/coin_flip_visual_test.cpp"),
        str(root / "apps/coin_flip/coin_flip_app.cpp"),
        str(root / "components/font_core/font_core.cpp"),
        str(root / "components/ui_theme/ui_theme.cpp"),
        "-o", exe,
    ], check=True)
    subprocess.run([exe], check=True)
# Existing source-contract guards remain independently enforced by test_coin_flip.py.
print("Coin Flip production raster host test PASS")
