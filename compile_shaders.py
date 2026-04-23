#!/usr/bin/env python3
"""
compile_shaders.py

Recursively searches src/ for GLSL, HLSL, and Slang shaders and compiles
them to SPIR-V using the appropriate compiler tool.

Tools required:
  - glslc   (GLSL / HLSL)  — part of the Shaderc package
  - slangc  (Slang)        — part of the Slang compiler

Output:
  Each compiled .spv file is placed next to the source file.
"""

import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SRC_DIR = Path("src")

# glslc stage flags keyed by file extension
GLSL_EXTENSIONS: dict[str, str] = {
    ".vert": "vert",
    ".frag": "frag",
    ".comp": "comp",
    ".geom": "geom",
    ".tesc": "tesc",
    ".tese": "tese",
    ".mesh": "mesh",
    ".task": "task",
    ".rgen": "rgen",
    ".rint": "rint",
    ".rahit": "rahit",
    ".rchit": "rchit",
    ".rmiss": "rmiss",
    ".rcall": "rcall",
}

# HLSL entry-point conventions keyed by file extension
# Adjust these to match your own project's naming conventions.
HLSL_EXTENSIONS: dict[str, str] = {
    ".hlsl": None,          # generic — glslc will infer stage from #pragma or -fshader-stage
    ".vs.hlsl": "vert",
    ".ps.hlsl": "frag",
    ".cs.hlsl": "comp",
    ".gs.hlsl": "geom",
    ".hs.hlsl": "tesc",
    ".ds.hlsl": "tese",
}

SLANG_EXTENSIONS: set[str] = {".slang"}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def run(cmd: list[str], source: Path) -> bool:
    """Run *cmd*, print a compact status line, return True on success."""
    print(f"  compiling  {source}")
    print(f"  command    {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  [FAILED]\n{result.stderr.strip()}\n")
        return False
    if result.stderr.strip():
        print(f"  [WARNING] {result.stderr.strip()}")
    print(f"  [OK]")
    return True


def spv_path(src: Path) -> Path:
    """Return the output .spv path (same directory as source).

    The output name is just the innermost extension without a leading dot,
    e.g. src/shaders/shader.vert  ->  src/shaders/vert.spv
         src/shaders/shader.frag  ->  src/shaders/frag.spv
         src/shaders/effect.slang ->  src/shaders/slang.spv
    """
    stage = src.suffix.lstrip(".")   # e.g. "vert", "frag", "hlsl", "slang"
    return src.parent / f"{stage}.spv"


# ---------------------------------------------------------------------------
# Per-language compilers
# ---------------------------------------------------------------------------

def compile_glsl(src: Path, stage: str) -> bool:
    out = spv_path(src)
    cmd = [
        "glslc",
        f"-fshader-stage={stage}",
        "--target-env=vulkan1.3",
        "-O",
        str(src),
        "-o", str(out),
    ]
    return run(cmd, src)


def compile_hlsl(src: Path, stage: str | None) -> bool:
    out = spv_path(src)
    cmd = ["glslc", "-x", "hlsl", "--target-env=vulkan1.3", "-O"]
    if stage:
        cmd += [f"-fshader-stage={stage}"]
    cmd += [str(src), "-o", str(out)]
    return run(cmd, src)


def compile_slang(src: Path) -> bool:
    out = spv_path(src)
    cmd = [
        "slangc",
        str(src),
        "-target", "spirv",
        "-o", str(out),
    ]
    return run(cmd, src)


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

def match_hlsl_stage(src: Path) -> str | None:
    """
    Try multi-part suffixes first (.vs.hlsl) then the bare .hlsl catch-all.
    Returns the stage string or None (meaning glslc will infer it).
    """
    name = src.name.lower()
    for ext, stage in HLSL_EXTENSIONS.items():
        if ext != ".hlsl" and name.endswith(ext):
            return stage
    return None   # bare .hlsl — let glslc figure it out


def collect_shaders(src_dir: Path):
    """Yield (path, language) tuples for every shader found under *src_dir*."""
    for path in sorted(src_dir.rglob("*")):
        if not path.is_file():
            continue

        suffix = path.suffix.lower()
        name_lower = path.name.lower()

        # Check HLSL first (multi-part suffixes like .vs.hlsl overlap .hlsl)
        is_hlsl = any(name_lower.endswith(ext) for ext in HLSL_EXTENSIONS)
        if is_hlsl:
            yield path, "hlsl"
        elif suffix in GLSL_EXTENSIONS:
            yield path, "glsl"
        elif suffix in SLANG_EXTENSIONS:
            yield path, "slang"


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    if not SRC_DIR.exists():
        print(f"Error: source directory '{SRC_DIR}' does not exist.", file=sys.stderr)
        return 1

    shaders = list(collect_shaders(SRC_DIR))
    if not shaders:
        print(f"No shaders found under '{SRC_DIR}'.")
        return 0

    # Delete any existing .spv files before recompiling
    existing_spv = list(SRC_DIR.rglob("*.spv"))
    if existing_spv:
        print(f"\nCleaning {len(existing_spv)} existing .spv file(s)...")
        for spv in existing_spv:
            spv.unlink()
            print(f"  deleted  {spv}")

    total = len(shaders)
    passed = 0
    failed: list[Path] = []

    print(f"\nFound {total} shader(s) under '{SRC_DIR}'\n")
    print("=" * 60)

    for src, lang in shaders:
        print(f"\n[{lang.upper()}] {src.relative_to(SRC_DIR.parent)}")

        ok = False
        if lang == "glsl":
            stage = GLSL_EXTENSIONS[src.suffix.lower()]
            ok = compile_glsl(src, stage)
        elif lang == "hlsl":
            stage = match_hlsl_stage(src)
            ok = compile_hlsl(src, stage)
        elif lang == "slang":
            ok = compile_slang(src)

        if ok:
            passed += 1
        else:
            failed.append(src)

    # Summary
    print("\n" + "=" * 60)
    print(f"Results: {passed}/{total} succeeded")
    if failed:
        print(f"\nFailed shaders ({len(failed)}):")
        for f in failed:
            print(f"  {f}")
        return 1

    print("All shaders compiled successfully.")
    return 0


if __name__ == "__main__":
    sys.exit(main())