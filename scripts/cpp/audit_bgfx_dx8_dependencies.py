#!/usr/bin/env python3
"""Summarize remaining DX8-shaped dependencies in the bgfx build path.

This is intentionally an audit tool, not a linter. Some hits are expected while
the bgfx backend still uses the legacy DX8Wrapper state model. The goal is to
make the dependency surface measurable as the migration progresses.
"""

from __future__ import annotations

import os
import re
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

SEARCH_ROOTS = [
    ROOT / "Core" / "GameEngineDevice",
    ROOT / "Core" / "Libraries" / "Source" / "WWVegas" / "WW3D2",
    ROOT / "GeneralsMD" / "Code" / "GameEngineDevice",
    ROOT / "GeneralsMD" / "Code" / "Libraries" / "Source" / "WWVegas" / "WW3D2",
]

SKIP_FILES = {
    "DX8Backend.cpp",
    "DX8Backend.h",
    "StubD3D8Device.cpp",
    "StubD3D8Device.h",
    "D3DXStandaloneStubs.cpp",
}

CATEGORIES = [
    (
        "raw_device",
        re.compile(r"_Get_D3D_Device8\s*\(|_Get_D3D8\s*\(|->(?:SetRenderState|GetRenderState|SetTexture|SetPixelShader|SetPixelShaderConstant|CreatePixelShader|DeletePixelShader)\s*\("),
    ),
    (
        "dx8wrapper_low_level",
        re.compile(r"DX8Wrapper::(?:Set_DX8_|Get_DX8_|_Set_DX8_|_Get_DX8_|_Create_DX8_|Set_Render_Target\s*\(|_Copy_DX8_Rects)"),
    ),
    (
        "dx8wrapper_high_level",
        re.compile(r"DX8Wrapper::(?:Set_Transform|Get_Transform|Set_Light|Set_Texture|Set_Material|Set_Shader|Apply_Render_State_Changes|Set_Render_State|Set_Vertex_Shader|Set_Pixel_Shader|Set_[VP]ixel_Shader_Constant)"),
    ),
    (
        "d3d_public_type",
        re.compile(r"\b(?:IDirect3D\w*8|LPDIRECT3D\w*8|D3D[A-Z0-9_]+|D3DX[A-Z0-9_]+)\b"),
    ),
    (
        "bgfx_dx8backend_base_call",
        re.compile(r"\bDX8Backend::"),
    ),
    (
        "bgfx_peek_dx8_state",
        re.compile(r"DX8Wrapper::Peek_Render_State\s*\("),
    ),
]


def iter_source_files():
    suffixes = {".cpp", ".h", ".hpp", ".inl"}
    for search_root in SEARCH_ROOTS:
        if not search_root.exists():
            continue
        for path in search_root.rglob("*"):
            if path.suffix not in suffixes:
                continue
            if path.name in SKIP_FILES:
                continue
            yield path


def rel(path: Path) -> str:
    return os.fspath(path.relative_to(ROOT))


def main() -> int:
    hits_by_category = {name: defaultdict(list) for name, _ in CATEGORIES}

    for path in iter_source_files():
        try:
            lines = path.read_text(errors="ignore").splitlines()
        except OSError:
            continue
        for lineno, line in enumerate(lines, 1):
            for name, pattern in CATEGORIES:
                if pattern.search(line):
                    hits_by_category[name][path].append((lineno, line.strip()))

    total = 0
    for name, by_file in hits_by_category.items():
        hit_count = sum(len(v) for v in by_file.values())
        total += hit_count
        print(f"\n{name}: {hit_count} hits in {len(by_file)} files")
        for path in sorted(by_file, key=lambda p: rel(p)):
            samples = by_file[path][:3]
            sample_text = "; ".join(f"{lineno}: {text[:120]}" for lineno, text in samples)
            suffix = "" if len(by_file[path]) <= 3 else f"; +{len(by_file[path]) - 3} more"
            print(f"  {rel(path)} ({len(by_file[path])}) {sample_text}{suffix}")

    print(f"\ntotal: {total} categorized hits")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
