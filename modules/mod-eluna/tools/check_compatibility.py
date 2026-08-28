#!/usr/bin/env python3
"""Check that the Phase 1 compatibility contract covers the upstream catalog."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[3]
HOOKS = ROOT / "modules/mod-eluna/Eluna/hooks/Hooks.h"
CONTRACT = ROOT / "modules/mod-eluna/COMPATIBILITY.md"
POC = ROOT / "modules/mod-eluna/lua_scripts/eluna_poc.lua"


def upstream_events() -> list[tuple[str, int, str, str]]:
    events = []
    category = None
    category_pattern = re.compile(r"^\s*#define\s+([A-Z]+)_EVENTS_LIST\(X\)")
    event_pattern = re.compile(r"\bX\((\w+),\s*(\d+),\s+\"([^\"]+)\"\)")

    for line in HOOKS.read_text(encoding="utf-8").splitlines():
        category_match = category_pattern.match(line)
        if category_match:
            category = category_match.group(1).lower()
            continue
        if category and line.lstrip().startswith("enum "):
            category = None
            continue
        if category:
            event_match = event_pattern.search(line)
            if event_match:
                symbol, event_id, name = event_match.groups()
                events.append((category, int(event_id), symbol, name))

    return events


def contract_events(contract: str) -> dict[str, tuple[str, int, str]]:
    events = {}
    row_pattern = re.compile(
        r"^\|\s*`(?P<symbol>\w+_EVENT_\w+)`\s*\|\s*"
        r"(?P<event_id>\d+)\s*\|\s*`(?P<name>[^`]+)`\s*\|"
    )
    for line in contract.splitlines():
        match = row_pattern.match(line)
        if match:
            symbol = match.group("symbol")
            events[symbol] = (symbol, int(match.group("event_id")), match.group("name"))
    return events


def upstream_registration_types() -> list[str]:
    registration_types = []
    in_enum = False
    pattern = re.compile(r"^\s*REGTYPE_(\w+),?$")

    for line in HOOKS.read_text(encoding="utf-8").splitlines():
        if line.strip() == "enum RegisterTypes : uint8":
            in_enum = True
            continue
        if in_enum:
            match = pattern.match(line)
            if not match:
                if registration_types:
                    break
                continue
            name = match.group(1)
            if name != "COUNT":
                registration_types.append(name.lower())

    return registration_types


def upstream_hook_table_categories() -> list[str]:
    categories = []
    pattern = re.compile(r'^\s*\{\s*"([a-z]+)"')
    in_table = False

    for line in HOOKS.read_text(encoding="utf-8").splitlines():
        if "static constexpr HookStorage HookTypeTable[]" in line:
            in_table = True
            continue
        if in_table:
            match = pattern.match(line)
            if match:
                categories.append(match.group(1))
            elif line.strip() == "};":
                break

    return categories


def fail(message: str) -> None:
    print(f"compatibility check failed: {message}", file=sys.stderr)


def main() -> int:
    missing_paths = [path for path in (HOOKS, CONTRACT, POC) if not path.is_file()]
    if missing_paths:
        for path in missing_paths:
            fail(f"missing required file: {path}")
        return 1

    contract = CONTRACT.read_text(encoding="utf-8")
    poc = POC.read_text(encoding="utf-8")
    expected = upstream_events()
    documented = contract_events(contract)
    errors = []

    for category, event_id, symbol, name in expected:
        row = documented.get(symbol)
        if row is None:
            errors.append(f"missing event row: {category}:{symbol}")
            continue
        _, documented_id, documented_name = row
        if documented_id != event_id or documented_name != name:
            errors.append(
                f"event mismatch for {symbol}: expected {event_id}/{name}, "
                f"found {documented_id}/{documented_name}"
            )

    required_contract_fragments = (
        "## Supported Now",
        "RegisterPlayerEvent(3, callback)",
        "Player:SendBroadcastMessage(message)",
        "## Lifetime, Thread, and Error Rules",
        "## Registration Category Matrix",
        "## VMangos Method Inventory",
        "## Return and Cancellation Matrix",
        "## POC Regression Contract",
        "adapter required",
        "core gap",
        "Reference only",
        "callback-only",
    )
    for fragment in required_contract_fragments:
        if fragment not in contract:
            errors.append(f"missing contract requirement: {fragment}")

    required_poc_fragments = (
        "RegisterPlayerEvent(3, OnLogin)",
        'player:SendBroadcastMessage("Eluna POC: Lua login event received.")',
    )
    for fragment in required_poc_fragments:
        if fragment not in contract or fragment not in poc:
            errors.append(f"missing POC regression requirement: {fragment}")

    if len(documented) != len(expected):
        errors.append(
            f"event catalog row count differs: expected {len(expected)}, "
            f"found {len(documented)}"
        )

    for category in upstream_registration_types() + upstream_hook_table_categories():
        if f"`{category}`" not in contract:
            errors.append(f"missing registration category: {category}")

    method_files = sorted(path.name for path in (HOOKS.parent.parent / "methods/VMangos").glob("*.h"))
    for method_file in method_files:
        if f"`{method_file}`" not in contract:
            errors.append(f"missing VMangos method row: {method_file}")

    if errors:
        for error in errors:
            fail(error)
        return 1

    print(f"compatibility check passed: {len(expected)} upstream event rows covered")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
