#!/usr/bin/env python3
"""Summarize remaining DX8-shaped dependencies in the bgfx build path.

This is intentionally an audit tool, not a linter. Some hits are expected while
the bgfx backend still uses the legacy DX8Wrapper state model. The goal is to
make the dependency surface measurable as the migration progresses.
"""

from __future__ import annotations

import json
import os
import re
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
COMPILE_COMMANDS = ROOT / "build" / "macos-generalsmd-sdl3-bgfx" / "compile_commands.json"

SEARCH_ROOTS = [
    ROOT / "Core" / "GameEngineDevice",
    ROOT / "Core" / "Libraries" / "Source" / "WWVegas" / "WW3D2",
    ROOT / "GeneralsMD" / "Code" / "GameEngineDevice",
    ROOT / "GeneralsMD" / "Code" / "Libraries" / "Source" / "WWVegas" / "WW3D2",
]

# Explicit legacy islands. The audit tracks DX8-shaped coupling that leaks into
# bgfx-facing code; these files are the compatibility boundary itself.
SKIP_FILES = {
    "DX8Backend.cpp",
    "DX8Backend.h",
    "dx8caps.cpp",
    "dx8deviceinterop.h",
    "dx8formatconv.h",
    "dx8standalonetypes.h",
    "dx8texturelegacyd3dtypes.h",
    "dx8texturelegacytypes.h",
    "dx8wrapper.cpp",
    "fixedfunctionlegacytypes.h",
}

FORBIDDEN_BGFX_COMPILED_SOURCES = {
    # Backend/tools files that should only compile for the DX8 reference path.
    "DX8Backend.cpp",
    "dx8texman.cpp",
    "dx8webbrowser.cpp",
    "formconv.cpp",
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

BGFX_BACKEND_CATEGORIES = [
    (
        "bgfx_legacy_cache_read",
        re.compile(r"FixedFunctionState::Cached_(?:Render_State|Texture_Stage_State|Transform)\s*\("),
    ),
    (
        "bgfx_semantic_state_raw_write",
        re.compile(
            r"FixedFunctionState::Set_Cached_Render_State\s*\(\s*RS::(?:"
            r"CULLMODE|LIGHTING|FOGCOLOR|COLORWRITEENABLE|AMBIENT|"
            r"AMBIENTMATERIALSOURCE|DIFFUSEMATERIALSOURCE|EMISSIVEMATERIALSOURCE|"
            r"SRCBLEND|DESTBLEND|BLENDOP|ALPHABLENDENABLE|ALPHATESTENABLE|ALPHAREF|ALPHAFUNC|"
            r"ZBIAS|FILLMODE|SHADEMODE|ZENABLE|ZWRITEENABLE|ZFUNC|FOGENABLE|"
            r"SPECULARENABLE|PATCHSEGMENTS|NORMALIZENORMALS|TEXTUREFACTOR|"
            r"POINTSPRITEENABLE|POINTSCALEENABLE|POINTSIZE|POINTSIZEMIN|POINTSIZEMAX|"
            r"POINTSCALE_A|POINTSCALE_B|POINTSCALE_C|STENCILENABLE|STENCILFUNC|"
            r"STENCILREF|STENCILMASK|STENCILWRITEMASK|STENCILPASS|STENCILFAIL|STENCILZFAIL"
            r")\b"
            r"|FixedFunctionState::Set_Cached_Texture_Stage_State\s*\("
            r"|FixedFunctionState::Set_Cached_Transform\s*\("
        ),
    ),
    (
        "bgfx_d3d_named_helper",
        re.compile(r"\b(?:D3DMatrixIdentity|TextureAddressModeToD3DStageState|TextureSampleFilterToD3DStageState)\b"),
    ),
]

KNOWN_BGFX_MACROS = {
    "GGC_BGFX_STANDALONE": True,
    "GGC_RENDER_BACKEND_BGFX": True,
    "_WIN32": False,
}


def eval_condition_expression(expression: str, macros: dict[str, bool]) -> bool | None:
    expr = expression.strip()
    if not expr:
        return None

    if "&&" in expr and re.search(r"!\s*defined\s*\(?\s*GGC_BGFX_STANDALONE\s*\)?", expr):
        return False

    def replace_defined(match: re.Match[str]) -> str:
        name = match.group(1) or match.group(2)
        if name not in macros:
            raise KeyError(name)
        return "True" if macros[name] else "False"

    try:
        expr = re.sub(r"defined\s*\(\s*(\w+)\s*\)|defined\s+(\w+)", replace_defined, expr)
    except KeyError:
        return None

    tokens = re.findall(r"\w+|&&|\|\||!|\(|\)|==|!=|[01]", expr)
    if "".join(tokens).replace("&&", "").replace("||", "") == "":
        return None

    converted: list[str] = []
    for token in tokens:
        if token == "&&":
            converted.append("and")
        elif token == "||":
            converted.append("or")
        elif token == "!":
            converted.append("not")
        elif token in {"(", ")", "==", "!="}:
            converted.append(token)
        elif token in {"True", "False"}:
            converted.append(token)
        elif token in {"0", "1"}:
            converted.append("False" if token == "0" else "True")
        elif token in macros:
            converted.append("True" if macros[token] else "False")
        else:
            return None

    try:
        return bool(eval(" ".join(converted), {"__builtins__": {}}, {}))
    except Exception:
        return None


def eval_bgfx_standalone_condition(line: str, macros: dict[str, bool]) -> bool | None:
    stripped = line.strip()
    match = re.match(r"#\s*(ifn?def)\s+(\w+)\b", stripped)
    if match:
        macro = match.group(2)
        if macro not in macros:
            return None
        is_defined = macros[macro]
        return is_defined if match.group(1) == "ifdef" else not is_defined

    match = re.match(r"#\s*(?:if|elif)\s+(.+)", stripped)
    if match:
        return eval_condition_expression(match.group(1), macros)

    return None


def active_lines_for_bgfx_standalone(lines: list[str]):
    active_stack: list[dict[str, bool]] = []
    current_active = True
    macros = dict(KNOWN_BGFX_MACROS)

    for line in lines:
        stripped = line.strip()
        if re.match(r"#\s*(?:if|ifdef|ifndef)\b", stripped):
            parent_active = current_active
            condition = eval_bgfx_standalone_condition(line, macros)
            branch_active = parent_active if condition is None else parent_active and condition
            active_stack.append({
                "parent_active": parent_active,
                "branch_active": branch_active,
                "branch_taken": branch_active,
            })
            current_active = branch_active
            yield current_active, line
            continue

        if re.match(r"#\s*else\b", stripped) and active_stack:
            frame = active_stack[-1]
            branch_active = frame["parent_active"] and not frame["branch_taken"]
            frame["branch_active"] = branch_active
            frame["branch_taken"] = frame["branch_taken"] or branch_active
            current_active = branch_active
            yield current_active, line
            continue

        if re.match(r"#\s*elif\b", stripped) and active_stack:
            frame = active_stack[-1]
            condition = eval_bgfx_standalone_condition(line, macros)
            if condition is None:
                branch_active = frame["parent_active"] and not frame["branch_taken"]
            else:
                branch_active = frame["parent_active"] and not frame["branch_taken"] and condition
            frame["branch_active"] = branch_active
            frame["branch_taken"] = frame["branch_taken"] or branch_active
            current_active = branch_active
            yield current_active, line
            continue

        if re.match(r"#\s*endif\b", stripped) and active_stack:
            active_stack.pop()
            current_active = active_stack[-1]["branch_active"] if active_stack else True
            yield current_active, line
            continue

        if current_active:
            define = re.match(r"#\s*define\s+(\w+)(?:\s+([01]))?\b", stripped)
            if define:
                macros[define.group(1)] = define.group(2) != "0"

        yield current_active, line


def resolve_compile_command_path(entry: dict) -> Path | None:
    file_name = entry.get("file")
    if not file_name:
        return None

    path = Path(file_name)
    if not path.is_absolute():
        path = Path(entry.get("directory", ROOT)) / path
    return path.resolve()


def load_compiled_source_files() -> set[Path] | None:
    if not COMPILE_COMMANDS.exists():
        return None

    try:
        entries = json.loads(COMPILE_COMMANDS.read_text())
    except (OSError, json.JSONDecodeError):
        return None

    compiled_sources: set[Path] = set()
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        path = resolve_compile_command_path(entry)
        if path is not None:
            compiled_sources.add(path)

    return compiled_sources


def iter_source_files(compiled_sources: set[Path] | None):
    suffixes = {".cpp", ".h", ".hpp", ".inl"}
    for search_root in SEARCH_ROOTS:
        if not search_root.exists():
            continue
        for path in search_root.rglob("*"):
            if path.suffix not in suffixes:
                continue
            if path.name in SKIP_FILES:
                continue
            if "dx8sdk" in path.parts:
                continue
            if compiled_sources is not None and path.suffix == ".cpp" and path.resolve() not in compiled_sources:
                continue
            yield path


def rel(path: Path) -> str:
    return os.fspath(path.relative_to(ROOT))


def find_forbidden_compiled_sources(compiled_sources: set[Path] | None) -> list[Path]:
    if compiled_sources is None:
        return []

    return sorted(
        path for path in compiled_sources
        if path.name in FORBIDDEN_BGFX_COMPILED_SOURCES
    )


def main() -> int:
    hits_by_category = {name: defaultdict(list) for name, _ in CATEGORIES}
    bgfx_backend_hits_by_category = {name: defaultdict(list) for name, _ in BGFX_BACKEND_CATEGORIES}
    compiled_sources = load_compiled_source_files()
    forbidden_compiled_sources = find_forbidden_compiled_sources(compiled_sources)

    for path in iter_source_files(compiled_sources):
        try:
            lines = path.read_text(errors="ignore").splitlines()
        except OSError:
            continue
        for lineno, (active, line) in enumerate(active_lines_for_bgfx_standalone(lines), 1):
            if not active:
                continue
            for name, pattern in CATEGORIES:
                if pattern.search(line):
                    hits_by_category[name][path].append((lineno, line.strip()))
            if path.name.startswith("BgfxBackend"):
                for name, pattern in BGFX_BACKEND_CATEGORIES:
                    if pattern.search(line):
                        bgfx_backend_hits_by_category[name][path].append((lineno, line.strip()))

    total = 0
    for name, by_file in {**hits_by_category, **bgfx_backend_hits_by_category}.items():
        hit_count = sum(len(v) for v in by_file.values())
        total += hit_count
        print(f"\n{name}: {hit_count} hits in {len(by_file)} files")
        for path in sorted(by_file, key=lambda p: rel(p)):
            samples = by_file[path][:3]
            sample_text = "; ".join(f"{lineno}: {text[:120]}" for lineno, text in samples)
            suffix = "" if len(by_file[path]) <= 3 else f"; +{len(by_file[path]) - 3} more"
            print(f"  {rel(path)} ({len(by_file[path])}) {sample_text}{suffix}")

    print(f"\nforbidden_bgfx_compiled_source: {len(forbidden_compiled_sources)} hits")
    for path in forbidden_compiled_sources:
        print(f"  {rel(path)}")

    print(f"\ntotal: {total} categorized hits")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
