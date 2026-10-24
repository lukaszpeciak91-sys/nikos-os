#!/usr/bin/env python3
"""Compile the production Converter UI and check R3 navigation on host."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("No C++17 host compiler available")

with tempfile.TemporaryDirectory() as tmp:
    binary = Path(tmp) / "converter_navigation"
    subprocess.run([
        compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-I" + str(root / "tools/tests/font_core_stubs"),
        "-I" + str(root / "apps/converter/include"),
        "-I" + str(root / "components/font_core/include"),
        str(root / "tools/tests/test_converter_navigation.cpp"),
        str(root / "apps/converter/converter_app.cpp"),
        str(root / "components/font_core/font_core.cpp"),
        "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)

print("Converter R3 navigation, cancel/commit, invalid edit, suspend/resume PASS")
