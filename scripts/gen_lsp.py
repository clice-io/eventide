#!/usr/bin/env python3

import argparse
import json
import pathlib
import re
import textwrap
import urllib.request
from typing import Dict, List, Set, Tuple

DEFAULT_VERSION = "3.17"
DEFAULT_URL_TEMPLATE = (
    "https://raw.githubusercontent.com/microsoft/language-server-protocol/"
    "gh-pages/_specifications/lsp/{version}/metaModel/metaModel.json"
)
DEFAULT_OUT_TYPES = "lsp_types.h"
DEFAULT_OUT_METHODS = "lsp_methods.inc"
DEFAULT_NAMESPACE = "eventide::language::proto"
DEFAULT_BASE_HEADER = "language/base.h"

CXX_KEYWORDS = {
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "atomic_cancel",
    "atomic_commit",
    "atomic_noexcept",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "char8_t",
    "char16_t",
    "char32_t",
    "class",
    "compl",
    "concept",
    "const",
    "consteval",
    "constexpr",
    "const_cast",
    "continue",
    "co_await",
    "co_return",
    "co_yield",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "reflexpr",
    "register",
    "reinterpret_cast",
    "requires",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "synchronized",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq",
}

IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def cxx_string_literal(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def sanitize_identifier(name: str, used: Set[str]) -> str:
    out = re.sub(r"[^A-Za-z0-9_]", "_", name)
    if not out:
        out = "field"
    if out[0].isdigit():
        out = f"_{out}"
    if out in CXX_KEYWORDS:
        out = f"{out}_"
    base = out
    i = 1
    while out in used:
        out = f"{base}_{i}"
        i += 1
    used.add(out)
    return out


def method_to_identifier(method: str, used: Set[str]) -> str:
    # Replace non-alnum with underscore and split camelCase to snake.
    cleaned = re.sub(r"[^A-Za-z0-9]", "_", method)
    snake = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", cleaned)
    snake = snake.lower()
    if snake[0].isdigit():
        snake = f"m_{snake}"
    if snake in CXX_KEYWORDS:
        snake = f"{snake}_"
    base = snake
    i = 1
    while snake in used:
        snake = f"{base}_{i}"
        i += 1
    used.add(snake)
    return snake


class TypeRenderer:
    def __init__(self, literal_map: Dict[str, str], literal_defs: List[dict]):
        self.literal_map = literal_map
        self.literal_defs = literal_defs

    def render(
        self, t: dict, current_struct: str | None = None, indirect: bool = False
    ) -> str:
        kind = t.get("kind")
        if kind == "base":
            name = t.get("name")
            return {
                "string": "std::string",
                "integer": "std::int32_t",
                "uinteger": "std::uint32_t",
                "decimal": "double",
                "boolean": "bool",
                "null": "LSPAny",
                "DocumentUri": "DocumentUri",
                "URI": "URI",
            }[name]
        if kind == "reference":
            name = t.get("name")
            if current_struct and name == current_struct and not indirect:
                return f"std::shared_ptr<{name}>"
            return name
        if kind == "array":
            return f"std::vector<{self.render(t['element'], current_struct, True)}>"
        if kind == "map":
            return f"std::map<{self.render(t['key'], current_struct, True)}, {self.render(t['value'], current_struct, True)}>"
        if kind == "tuple":
            items = ", ".join(
                self.render(item, current_struct, False) for item in t.get("items", [])
            )
            return f"std::tuple<{items}>"
        if kind == "or":
            items = t.get("items", [])
            has_null = any(
                item.get("kind") == "base" and item.get("name") == "null"
                for item in items
            )
            non_null = [
                item
                for item in items
                if not (item.get("kind") == "base" and item.get("name") == "null")
            ]
            rendered = [self.render(item, current_struct, False) for item in non_null]
            if has_null:
                if len(rendered) == 1:
                    return f"std::optional<{rendered[0]}>"
                inner = ", ".join(rendered)
                return f"std::optional<std::variant<{inner}>>"
            inner = ", ".join(rendered)
            return f"std::variant<{inner}>"
        if kind == "stringLiteral":
            value = t.get("value")
            return f"rfl::Literal<{cxx_string_literal(value)}>"
        if kind == "literal":
            key = json.dumps(t.get("value"), sort_keys=True)
            if key not in self.literal_map:
                name = f"LspLiteral{len(self.literal_defs) + 1}"
                self.literal_map[key] = name
                self.literal_defs.append(
                    {
                        "name": name,
                        "properties": t.get("value", {}).get("properties", []),
                        "extends": [],
                        "mixins": [],
                    }
                )
            return self.literal_map[key]
        raise ValueError(f"Unsupported type kind: {kind}")


def collect_literal_types(types: List[dict], renderer: TypeRenderer) -> None:
    def walk(t: dict) -> None:
        if not isinstance(t, dict):
            return
        kind = t.get("kind")
        if kind == "literal":
            renderer.render(t)
            for prop in t.get("value", {}).get("properties", []):
                walk(prop.get("type"))
        elif kind == "array":
            walk(t.get("element"))
        elif kind == "map":
            walk(t.get("key"))
            walk(t.get("value"))
        elif kind in ("or", "and", "tuple"):
            for item in t.get("items", []):
                walk(item)

    for item in types:
        walk(item)


def download_schema(url: str, path: pathlib.Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url) as resp:
        content = resp.read()
    path.write_bytes(content)


def topo_sort(nodes: List[str], deps: Dict[str, Set[str]]) -> List[str]:
    # deps[node] = set of nodes that must appear before node
    in_deg = {n: len(deps.get(n, set())) for n in nodes}
    dependents: Dict[str, Set[str]] = {n: set() for n in nodes}
    for node, requirements in deps.items():
        for req in requirements:
            dependents.setdefault(req, set()).add(node)

    queue = [n for n, d in in_deg.items() if d == 0]
    order = []
    while queue:
        n = queue.pop()
        order.append(n)
        for dep in dependents.get(n, set()):
            in_deg[dep] -= 1
            if in_deg[dep] == 0:
                queue.append(dep)
    if len(order) != len(nodes):
        missing = [n for n, d in in_deg.items() if d > 0]
        raise RuntimeError(f"Cyclic dependency detected: {missing}")
    return order


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate LSP C++ types from metaModel.json"
    )
    parser.add_argument("--version", default=DEFAULT_VERSION)
    parser.add_argument("--schema", default=None)
    parser.add_argument(
        "--out", required=True, help="Output directory for generated files"
    )
    args = parser.parse_args()

    out_dir = pathlib.Path(args.out)
    if out_dir.exists() and not out_dir.is_dir():
        raise SystemExit(f"Output path must be a directory: {out_dir}")
    out_dir.mkdir(parents=True, exist_ok=True)

    out_types = out_dir / DEFAULT_OUT_TYPES
    out_methods = out_dir / DEFAULT_OUT_METHODS

    for path in (out_types, out_methods):
        if path.exists():
            path.unlink()

    if args.schema:
        schema_path = pathlib.Path(args.schema)
    else:
        schema_path = out_dir / f"metaModel-{args.version}.json"
        url = DEFAULT_URL_TEMPLATE.format(version=args.version)
        if not schema_path.exists():
            download_schema(url, schema_path)

    with schema_path.open("r", encoding="utf-8") as f:
        model = json.load(f)

    structures = list(model.get("structures", []))
    enumerations = list(model.get("enumerations", []))
    type_aliases = list(model.get("typeAliases", []))
    requests = list(model.get("requests", []))
    notifications = list(model.get("notifications", []))

    # Pre-collect literal structs.
    literal_map: Dict[str, str] = {}
    literal_defs: List[dict] = []
    renderer = TypeRenderer(literal_map, literal_defs)

    for st in structures:
        collect_literal_types(
            [p.get("type") for p in st.get("properties", [])], renderer
        )
        collect_literal_types(st.get("extends", []), renderer)
        collect_literal_types(st.get("mixins", []), renderer)

    for ta in type_aliases:
        collect_literal_types([ta.get("type")], renderer)

    for req in requests:
        collect_literal_types([req.get("params"), req.get("result")], renderer)

    for note in notifications:
        collect_literal_types([note.get("params")], renderer)

    # Merge literal structs into structures.
    structures = literal_defs + structures

    struct_names = {s["name"] for s in structures}
    alias_names = {a["name"] for a in type_aliases}

    # Special-case LSPAny/LSPObject/LSPArray.
    special_aliases = {"LSPAny", "LSPObject", "LSPArray"}

    def collect_refs(t: dict, refs: Set[str]) -> None:
        if not isinstance(t, dict):
            return
        kind = t.get("kind")
        if kind == "reference":
            refs.add(t.get("name"))
        elif kind == "array":
            collect_refs(t.get("element"), refs)
        elif kind == "map":
            collect_refs(t.get("key"), refs)
            collect_refs(t.get("value"), refs)
        elif kind in ("or", "and", "tuple"):
            for item in t.get("items", []):
                collect_refs(item, refs)
        elif kind == "literal":
            key = json.dumps(t.get("value"), sort_keys=True)
            literal_name = literal_map.get(key)
            if literal_name:
                refs.add(literal_name)
            for prop in t.get("value", {}).get("properties", []):
                collect_refs(prop.get("type"), refs)

    deps: Dict[str, Set[str]] = {}

    for st in structures:
        name = st["name"]
        refs: Set[str] = set()
        for prop in st.get("properties", []):
            collect_refs(prop.get("type"), refs)
        for ext in st.get("extends", []):
            collect_refs(ext, refs)
        for mix in st.get("mixins", []):
            collect_refs(mix, refs)
        deps[name] = {r for r in refs if r in struct_names or r in alias_names}
        deps[name] -= special_aliases
        deps[name].discard(name)

    for ta in type_aliases:
        name = ta["name"]
        if name in special_aliases:
            deps[name] = set()
            continue
        refs: Set[str] = set()
        collect_refs(ta.get("type"), refs)
        deps[name] = {r for r in refs if r in struct_names or r in alias_names}
        deps[name] -= special_aliases
        deps[name].discard(name)

    # Build order for structs + aliases (excluding enums and special aliases).
    nodes = [n for n in list(struct_names | alias_names) if n not in special_aliases]
    order = topo_sort(nodes, deps)

    # Prepare enum info.
    enum_specs = []
    enum_name_map = {}
    for enum in enumerations:
        name = enum["name"]
        base = enum["type"]["name"]
        supports_custom = enum.get("supportsCustomValues", False)
        if supports_custom and base == "string":
            enum_type_name = f"{name}Enum"
        else:
            enum_type_name = name
        enum_name_map[name] = enum_type_name
        enum_specs.append(
            {
                "name": name,
                "enum_type": enum_type_name,
                "base": base,
                "supports_custom": supports_custom,
                "values": enum.get("values", []),
            }
        )

    # Rewrite references to custom-value enums to use alias name.
    def rewrite_enum_reference(t: dict) -> dict:
        if not isinstance(t, dict):
            return t
        kind = t.get("kind")
        if kind == "reference":
            name = t.get("name")
            return {"kind": "reference", "name": name}
        if kind == "array":
            return {
                "kind": "array",
                "element": rewrite_enum_reference(t.get("element")),
            }
        if kind == "map":
            return {
                "kind": "map",
                "key": rewrite_enum_reference(t.get("key")),
                "value": rewrite_enum_reference(t.get("value")),
            }
        if kind in ("or", "and", "tuple"):
            return {
                "kind": kind,
                "items": [rewrite_enum_reference(i) for i in t.get("items", [])],
            }
        if kind == "literal":
            return t
        return t

    # Prepare renderer with literal map.
    renderer = TypeRenderer(literal_map, literal_defs)

    # Emit header.
    lines: List[str] = []
    lines.append("#pragma once")
    lines.append("")
    lines.append(f'#include "{DEFAULT_BASE_HEADER}"')
    lines.append('#include "serde/serde.h"')
    lines.append("")
    lines.append(f"namespace {DEFAULT_NAMESPACE} {{")
    lines.append("")

    # Enums
    for enum in enum_specs:
        base = enum["base"]
        enum_type = enum["enum_type"]
        values = enum["values"]
        used_members: Set[str] = set()
        members: List[Tuple[str, str, object]] = []
        for val in values:
            raw_name = val["name"]
            member_name = sanitize_identifier(raw_name, used_members)
            json_value = val.get("value", raw_name)
            members.append((member_name, json_value, val.get("value")))
        underlying = (
            "std::int32_t"
            if base == "integer"
            else "std::uint32_t"
            if base == "uinteger"
            else None
        )
        if underlying:
            lines.append(f"enum class {enum_type} : {underlying} {{")
            for member_name, _, value in members:
                lines.append(f"    {member_name} = {value},")
            lines.append("};")
        else:
            lines.append(f"enum class {enum_type} {{")
            for member_name, _, _ in members:
                lines.append(f"    {member_name},")
            lines.append("};")

        if enum["supports_custom"] and base == "string":
            lines.append(
                f"using {enum['name']} = std::variant<{enum_type}, std::string>;"
            )
        lines.append("")

    # Structs and aliases in order
    struct_map = {s["name"]: s for s in structures}
    alias_map = {a["name"]: a for a in type_aliases}

    resolved_props: Dict[str, List[dict]] = {}

    def resolve_properties(name: str) -> List[dict]:
        if name in resolved_props:
            return resolved_props[name]
        st = struct_map[name]
        props: List[dict] = []
        seen: Set[str] = set()
        for base in st.get("extends", []) + st.get("mixins", []):
            if base.get("kind") != "reference":
                continue
            base_name = base.get("name")
            if base_name not in struct_map:
                continue
            for prop in resolve_properties(base_name):
                prop_name = prop.get("name")
                if prop_name in seen:
                    continue
                props.append(prop)
                seen.add(prop_name)
        for prop in st.get("properties", []):
            prop_name = prop.get("name")
            if prop_name in seen:
                props = [p for p in props if p.get("name") != prop_name]
            props.append(prop)
            seen.add(prop_name)
        resolved_props[name] = props
        return props

    for name in order:
        if name in struct_map:
            lines.append(f"struct {name} {{")
            used: Set[str] = set()
            for prop in resolve_properties(name):
                prop_type = renderer.render(prop.get("type"), current_struct=name)
                if prop.get("optional"):
                    if not prop_type.startswith("std::optional<"):
                        prop_type = f"std::optional<{prop_type}>"
                prop_name = prop.get("name")
                field_name = sanitize_identifier(prop_name, used)
                if field_name != prop_name:
                    lines.append(
                        f"    rfl::Rename<{cxx_string_literal(prop_name)}, {prop_type}> {field_name};"
                    )
                else:
                    lines.append(f"    {prop_type} {field_name};")
            lines.append("};")
            lines.append("")
        elif name in alias_map:
            if name in special_aliases:
                continue
            ta = alias_map[name]
            alias_type = renderer.render(ta.get("type"))
            lines.append(f"using {name} = {alias_type};")
            lines.append("")

    lines.append(f"}}  // namespace {DEFAULT_NAMESPACE}")
    lines.append("")
    lines.append("namespace refl::serde {")
    lines.append("")

    for enum in enum_specs:
        name = enum["name"]
        enum_type = enum["enum_type"]
        base = enum["base"]
        values = enum["values"]
        used_members: Set[str] = set()
        members: List[Tuple[str, str, object]] = []
        for val in values:
            raw_name = val["name"]
            member_name = sanitize_identifier(raw_name, used_members)
            json_value = val.get("value", raw_name)
            members.append((member_name, json_value, val.get("value")))
        if base in ("integer", "uinteger"):
            lines.append(
                f"template <> struct enum_traits<{DEFAULT_NAMESPACE}::{enum_type}> {{"
            )
            lines.append("    static constexpr bool enabled = true;")
            lines.append("    static constexpr enum_encoding encoding = enum_encoding::integer;")
            lines.append("};")
            lines.append("")
            continue

        # string enums
        lines.append(
            f"template <> struct enum_traits<{DEFAULT_NAMESPACE}::{enum_type}> {{"
        )
        lines.append("    static constexpr bool enabled = true;")
        lines.append("    static constexpr enum_encoding encoding = enum_encoding::string;")
        lines.append(
            f"    static constexpr std::array<std::pair<{DEFAULT_NAMESPACE}::{enum_type}, std::string_view>, {len(members)}> mapping = {{{{"
        )
        for member_name, json_value, _ in members:
            lines.append(
                f"        {{{DEFAULT_NAMESPACE}::{enum_type}::{member_name}, {cxx_string_literal(json_value)}}},"
            )
        lines.append("    }};")
        lines.append("};")
        lines.append("")

    lines.append("}  // namespace refl::serde")
    lines.append("")

    out_types.write_text("\n".join(lines), encoding="utf-8")

    # Methods macro file
    method_lines: List[str] = []
    method_lines.append("#pragma once")
    method_lines.append("")
    method_lines.append("#include <string_view>")
    method_lines.append("")
    method_lines.append("#define EVENTIDE_LSP_REQUESTS(X) \\")

    used_ids: Set[str] = set()
    for idx, req in enumerate(requests):
        method = req["method"]
        ident = method_to_identifier(method, used_ids)
        params = req.get("params")
        params_type = (
            f"{DEFAULT_NAMESPACE}::no_params"
            if params is None
            else renderer.render(params)
        )
        result = req.get("result")
        result_type = renderer.render(result) if result else "LSPAny"
        suffix = (" " + "\\") if idx < len(requests) - 1 else ""
        method_lines.append(
            f"    X({ident}, {cxx_string_literal(method)}, ({params_type}), ({result_type})){suffix}"
        )

    method_lines.append("")
    method_lines.append("#define EVENTIDE_LSP_NOTIFICATIONS(X) \\")

    used_ids = set()
    for idx, note in enumerate(notifications):
        method = note["method"]
        ident = method_to_identifier(method, used_ids)
        params = note.get("params")
        params_type = (
            f"{DEFAULT_NAMESPACE}::no_params"
            if params is None
            else renderer.render(params)
        )
        suffix = (" " + "\\") if idx < len(notifications) - 1 else ""
        method_lines.append(
            f"    X({ident}, {cxx_string_literal(method)}, ({params_type})){suffix}"
        )

    method_lines.append("")
    out_methods.write_text("\n".join(method_lines), encoding="utf-8")


if __name__ == "__main__":
    main()
