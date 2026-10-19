"""Reconcile AST/source and documented surfaces into a traceable planning catalogue.

This verifies inventory provenance and completeness against its declared inputs,
not runtime compatibility. Unknowns remain blocking review work, never PASS cases.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re


# Explicit area owners describe planning responsibility, never compatibility proof.
SOURCE_OWNERS = {
    "cli.ts": ["CLI-01"], "cli/args.ts": ["CLI-01"], "config.ts": ["CFG-01", "CFG-03"],
    "core/agent-session-runtime.ts": ["SESSION-03", "EXT-03"],
    "core/agent-session-services.ts": ["SDK-01"], "core/cache-warmer.ts": ["LLM-07"],
    "core/diagnostics.ts": ["OPS-01"], "core/event-bus.ts": ["EXT-02"],
    "core/exec.ts": ["TOOL-05"], "core/footer-data-provider.ts": ["TUI-02"],
    "core/keybindings.ts": ["TUI-01"], "core/mcp-servers.ts": ["MCP-01"],
    "core/messages.ts": ["SESSION-02"], "core/model-registry.ts": ["LLM-03"],
    "core/model-resolver.ts": ["CLI-04"], "core/model-runtime.ts": ["LLM-02", "LLM-03"],
    "core/package-manager.ts": ["CLI-06", "RESOURCE-04"],
    "core/prompt-templates.ts": ["RESOURCE-03"], "core/provider-composer.ts": ["LLM-02"],
    "core/resource-loader.ts": ["RESOURCE-01", "RESOURCE-02", "RESOURCE-03", "RESOURCE-04"],
    "core/sdk.ts": ["SDK-01"], "core/settings-defaults.ts": ["CFG-02"],
    "core/settings-manager.ts": ["CFG-02"], "core/settings-schema.ts": ["CFG-02"],
    "core/skills.ts": ["RESOURCE-02"], "core/slash-commands.ts": ["TUI-03"],
    "core/source-info.ts": ["RESOURCE-04"], "core/system-prompt.ts": ["RESOURCE-01"],
    "core/trust-manager.ts": ["CFG-04"], "core/virtual-models.ts": ["LLM-06"],
    "extensions/codemode/index.ts": ["CODEMODE-01"], "extensions/codemode/tool.ts": ["CODEMODE-01"],
    "extensions/tool-search/index.ts": ["CODEMODE-01", "MCP-01"],
    "index.ts": ["SDK-01"], "main.ts": ["CLI-01", "CLI-03"],
    "modes/index.ts": ["CLI-03", "WIRE-01", "WIRE-02"], "modes/json-event.ts": ["WIRE-01"],
    "modes/print-mode.ts": ["CLI-03"], "modes/rpc/rpc-client.ts": ["WIRE-02", "SDK-01"],
    "modes/rpc/rpc-mode.ts": ["WIRE-02"], "modes/rpc/rpc-types.ts": ["WIRE-02"],
    "rpc-entry.ts": ["WIRE-02"], "utils/clipboard.ts": ["TUI-04"],
    "utils/frontmatter.ts": ["RESOURCE-02", "RESOURCE-03"],
    "utils/image-convert.ts": ["TOOL-08"], "utils/image-resize-core.ts": ["TOOL-08"],
    "utils/image-resize.ts": ["TOOL-08"], "utils/mime.ts": ["TOOL-08"],
    "utils/shell.ts": ["TOOL-05"],
}
DOC_OWNERS = {
    "containerization.md": ["SEC-01"], "custom-provider.md": ["LLM-02"],
    "how-pi-works.md": ["AGENT-01", "SESSION-01"], "index.md": ["CLI-03"],
    "message-types.md": ["LLM-01", "SESSION-02"], "nvidia-rtx-llama-cpp.md": ["LOCAL-01"],
    "quickstart.md": ["CLI-03"], "shell-aliases.md": ["TOOL-05", "TUI-04"],
    "terminal-setup.md": ["TUI-01", "TUI-02"], "termux.md": ["PLATFORM-01"],
    "themes.md": ["TUI-02"], "tmux.md": ["TUI-01"], "windows.md": ["PLATFORM-01", "TOOL-07"],
}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def identifier(*parts):
    return hashlib.sha256("\0".join(parts).encode()).hexdigest()[:24]


def classify_unknown(unknown):
    """Classify observed discovery differences; unknown new behavior stays unresolved."""
    symbol = unknown.get("symbol")
    if unknown["id"].startswith("UNKNOWN-DEPENDENCY-"):
        return {"features": ["TOOL-01", "SDK-01"], "steps": ["T07", "T12", "T39"],
                "disposition": "EXTERNAL_TYPE_DEPENDENCY", "severity": "high",
                "reason": "TypeBox reexports describe schema/SDK dependencies, not a native implementation; review exact schema subset and TS bridge separately."}
    if unknown.get("source") == "settings_source_only":
        if symbol in {"queueMode", "websockets"}:
            return {"features": ["CFG-02"], "steps": ["T17", "T37"],
                    "disposition": "LEGACY_CONFIG_INPUT", "severity": "high",
                    "reason": "Pi settings-manager migrates queueMode to steeringMode and websockets to transport; modern keys take precedence."}
        if symbol in {"$schema", "deviceId", "trackingId", "lastChangelogVersion"}:
            return {"features": ["CFG-02", "OPS-01"], "steps": ["T17", "T37", "T40"],
                    "disposition": "METADATA_OR_PERSISTED_PRODUCT_STATE", "severity": "medium",
                    "reason": "Preserve/import deliberately; schema hints and Pi telemetry/update identifiers are not implicit Pangu networking or telemetry consent."}
    if unknown.get("source") == "document_flags_without_comparison":
        if symbol in {"--checkpoint-min-step", "--ctx-checkpoints", "--ctx-size", "--host", "--jinja",
                      "--models-dir", "--models-preset", "--no-models-autoload", "--port",
                      "--spec-draft-model", "-hf", "-hff", "-hfrd", "-m", "-ngl"}:
            return {"features": ["LOCAL-01"], "steps": ["T47"], "disposition": "EXTERNAL_LLAMA_OPTION",
                    "severity": "medium", "reason": "Documented llama.cpp server arguments; not flags accepted by Pi's own top-level parser."}
        if symbol in {"-bash", "-write", "-name"}:
            return {"features": ["CLI-05", "CFG-02"], "steps": ["T17", "T24", "T28"],
                    "disposition": "TOOL_SELECTION_VALUE", "severity": "high",
                    "reason": "Minus-prefixed tool-selection values must be tested as list data, not invented standalone CLI options."}
        if symbol in {"-builtin", "-path"}:
            return {"features": ["RESOURCE-04", "EXT-01"], "steps": ["T31", "T32", "T40"],
                    "disposition": "RESOURCE_SELECTOR_VALUE", "severity": "medium",
                    "reason": "Resource exclusion syntax with a path/name suffix, not a top-level CLI option."}
        if symbol == "-default":
            return {"features": ["RESOURCE-03"], "steps": ["T31"], "disposition": "TEMPLATE_PARAMETER_SYNTAX",
                    "severity": "medium", "reason": "Shell-style template default expansion; preserve the full expression rather than a regex-derived flag fragment."}
        if symbol == "-la":
            return {"features": ["TUI-04", "TOOL-05"], "steps": ["T26", "T34"],
                    "disposition": "EXTERNAL_SHELL_ARGUMENT", "severity": "medium",
                    "reason": "The ls -la example passes arguments to a subprocess, not Pi's CLI parser."}
    return None


def traceability(catalogue, features, scope):
    """Build open implementation gaps and an explicit requirement denominator."""
    by_id = {f["id"]: f for f in features["features"]}
    gaps = []
    for item in catalogue["items"]:
        families = [by_id[f] for f in item["feature_ids"]]
        if not families:
            raise ValueError(f"Unclassified surface cannot enter T04: {item['id']}")
        ranks = {"medium": 1, "high": 2, "critical": 3}
        gaps.append({"id": "GAP-" + item["id"], "surface_id": item["id"],
                     "feature_ids": item["feature_ids"], "case_id": item["proposed_case_id"],
                     "strategies": {f["id"]: f["compatibility_strategy"] for f in families},
                     "severity": max((f["risk"] for f in families), key=ranks.get),
                     "steps": sorted({step for f in families for step in f["steps"]}),
                     "owner_role": "responsible service implementer + independent reviewer",
                     "status": "OPEN_IMPLEMENTATION", "verification": "NOT_RUN", "evidence": []})
    resolutions = []
    for unknown in catalogue["unknowns"]:
        result = classify_unknown(unknown)
        if result is None:
            raise ValueError(f"Unclassified discovery difference: {unknown['id']}")
        resolutions.append({"id": unknown["id"], **result,
                            "classification": "STATIC_SOURCE_REVIEW", "implementation_status": "OPEN",
                            "approval": "NOT_A_COMPATIBILITY_OR_RELEASE_APPROVAL"})
    requirements = []
    for profile in scope["profiles"]:
        for field in ("required_features", "required_cli_subset", "required_settings_subset", "required_env_subset",
                      "model_protocols", "tools", "session_support", "permissions"):
            for value in profile.get(field, []):
                ident = "REQ-" + identifier(profile["id"], field, value)
                if field == "required_features" and value not in by_id:
                    raise ValueError("Scope requires unknown feature")
                requirements.append({"id": ident, "profile": profile["id"], "source_field": field,
                                     "requirement": value, "case_id": "CASE-" + ident,
                                     "status": "NOT_IMPLEMENTED", "evidence": [],
                                     "owner_role": "profile implementer + independent acceptance reviewer",
                                     "claim": "This requirement must be implemented/tested; family-level catalogue counts are not certification."})
    return {"schema": "pangu.migration.surface-gap-ledger.v1", "task": "T04",
            "status": "TRACKING_ESTABLISHED_IMPLEMENTATION_GAPS_OPEN",
            "gaps": gaps, "discovery_classifications": resolutions, "scope_requirements": requirements,
            "denominator_rule": "All requirement IDs for the approved profile and its ancestors, plus actual workflow-specific needs; none excluded merely because unimplemented.",
            "workflow_specific_requirements": "P3 scope is not universal; actual SDK/OAuth/extensions become required when used. T02 scope records the candidate feature IDs.",
            "release_rule": "FAIL/BLOCKED/missing/NOT_IMPLEMENTED required evidence is NO-GO; no pass-rate escape. Empty/future scopes and unregistered actual P3 workflows cannot be certified by a zero-case pass."}


def build(root, discovery, features):
    if discovery.get("schema") != "pangu.migration.ast-discovery.v1":
        raise ValueError("Unsupported AST discovery schema")
    if discovery.get("driver_sha256") != sha(root / "tools/migration/surface_inventory.py"):
        raise ValueError("Stale AST driver evidence")
    feature_ids = {f["id"] for f in features["features"]}
    path_families = {}
    for family in features["features"]:
        for source in family["sources"]:
            path_families.setdefault(str((root / source).resolve()), []).append(family["id"])
    items = []
    unknowns = []
    documents = []
    seen = set()

    def classify(path):
        matches = path_families.get(str(path.resolve()), [])
        if matches:
            return sorted(set(matches)), "DIRECT_FEATURE_SOURCE"
        relative = path.relative_to(root).as_posix()
        prefix = "references/pi/packages/coding-agent/"
        if relative.startswith(prefix + "src/"):
            local = relative.removeprefix(prefix + "src/")
            if local in SOURCE_OWNERS:
                return SOURCE_OWNERS[local], "EXPLICIT_AREA_OWNER_NOT_COMPATIBILITY_APPROVAL"
            if local.startswith("extensions/mcp/"):
                return ["MCP-01"], "EXPLICIT_AREA_OWNER_NOT_COMPATIBILITY_APPROVAL"
            if local.startswith("cli/auth") or local == "cli/credential-print.ts":
                return ["CLI-07", "AUTH-01", "AUTH-02"], "EXPLICIT_AREA_OWNER_NOT_COMPATIBILITY_APPROVAL"
            if local.startswith("cli/"):
                return ["CLI-01", "TUI-03"], "EXPLICIT_AREA_OWNER_NOT_COMPATIBILITY_APPROVAL"
            if local == "package-manager-cli.ts":
                return ["CLI-06"], "EXPLICIT_AREA_OWNER_NOT_COMPATIBILITY_APPROVAL"
        if relative.startswith(prefix + "docs/") and path.name in DOC_OWNERS:
            return DOC_OWNERS[path.name], "EXPLICIT_DOCUMENT_OWNER_NOT_COMPATIBILITY_APPROVAL"
        rules = [
            ("/core/extensions/", ["EXT-01", "EXT-02", "EXT-03"]),
            ("/core/compaction/", ["SESSION-05"]),
            ("/modes/interactive/", ["TUI-01", "TUI-02", "TUI-03"]),
            ("/core/tools/", ["TOOL-01"]),
            ("/packages/ai/", ["LLM-01", "LLM-02"]),
            ("/packages/agent/", ["AGENT-01", "AGENT-03"]),
        ]
        for token, families in rules:
            if token in relative:
                return families, "PROPOSED_SOURCE_AREA_MAPPING"
        return [], "UNCLASSIFIED_REQUIRES_T04"

    for source in discovery["sources"]:
        path = root / source["path"]
        if not path.resolve().is_relative_to(root.resolve()) or sha(path) != source["sha256"]:
            raise ValueError(f"Source provenance mismatch: {source['path']}")
        if source["syntactic_diagnostics"] != 0:
            raise ValueError("Syntactic errors cannot count as inventory evidence")
        families, mapping = classify(path)
        for row in source["items"]:
            if row["id"] in seen:
                raise ValueError("Duplicate AST item ID")
            seen.add(row["id"])
            ident = "SRC-" + row["id"]
            items.append({"id": ident, "origin": "AST", "source": source["path"],
                          "line": row["line"], "symbol": row["symbol"], "kind": row["kind"],
                          "feature_ids": families, "mapping": mapping,
                          "proposed_case_id": "CASE-" + ident,
                          "verification": "NOT_RUN", "review": "PENDING_INDEPENDENT_APPROVAL",
                          "boundary": "EXPOSED_OR_SUPPORTING_DECLARATION; not all exports are root API"})
        if not families:
            unknowns.append({"id": "UNKNOWN-SOURCE-" + identifier(source["path"]),
                             "source": source["path"], "reason": "Public/supporting declarations need feature classification",
                             "owner_role": "T04 surface reviewer", "steps": ["T04"], "status": "OPEN"})

    for edge in discovery["reexport_edges"]:
        if edge["status"] == "RESOLVED":
            if edge["to"] not in {s["path"] for s in discovery["sources"]}:
                raise ValueError("Uninventoried relative reexport")
        elif edge["status"] == "EXTERNAL_DEPENDENCY_REQUIRES_T07":
            unknowns.append({"id": "UNKNOWN-DEPENDENCY-" + identifier(edge["from"], edge["specifier"], str(edge["line"])),
                             "source": edge["from"], "specifier": edge["specifier"],
                             "reason": "External type/package surface requires dependency review, not native compatibility assumed",
                             "owner_role": "T07 dependency reviewer", "steps": ["T07", "T39"], "status": "OPEN"})
        else:
            raise ValueError("Unknown reexport status")

    source_paths = {source["path"] for source in discovery["sources"]}
    for entry in discovery["package_entries"]:
        if "manifest" in entry and sha(root / entry["manifest"]) != entry["manifest_sha256"]:
            raise ValueError("Package manifest changed since AST discovery")
        if entry["status"] == "PACKAGE_METADATA":
            continue
        if entry["status"] == "RESOLVED":
            if not set(entry["source_paths"]) <= source_paths:
                raise ValueError("Declared package entry missing from inventory")
        else:
            unknowns.append({"id": "UNKNOWN-ENTRY-" + identifier(entry["package"], entry["public_name"], entry["emitted"]),
                             "source": entry, "reason": "Build-time entry requires explicit source mapping",
                             "owner_role": "T04 interface reviewer", "steps": ["T04", "T06"], "status": "OPEN"})

    docs_root = root / "references/pi/packages/coding-agent/docs"
    for path in sorted(docs_root.glob("*.md")):
        relative = path.relative_to(root).as_posix()
        text = path.read_text()
        families, mapping = classify(path)
        documents.append({"path": relative, "sha256": sha(path), "semantic_review": "REQUIRED_T04"})
        fence = False
        for line_no, line in enumerate(text.splitlines(), 1):
            if line.startswith("```"):
                fence = not fence
                continue
            if fence:
                continue
            first_cell = re.match(r"^\|\s*(`[^|]+)\|", line)
            option = re.match(r"^-\s+`", line)
            if first_cell:
                symbols = re.findall(r"`([^`]+)`", first_cell[1])
            elif option:
                symbols = [s for s in re.findall(r"`([^`]+)`", line.split("<br>", 1)[0])
                           if re.match(r"^(-[a-z]|--[a-z]|--$)", s)]
            else:
                symbols = []
            table_symbols = set(symbols) if first_cell else set()
            symbols += [s for s in re.findall(r"`([^`]+)`", line)
                        if re.search(r"(?<![\w-])(?:--[a-z][a-z0-9-]*|-[a-z]+|--)(?![\w-])", s)]
            for symbol in sorted(set(symbols)):
                ident = "DOC-" + identifier(relative, str(line_no), symbol)
                items.append({"id": ident, "origin": "DOCUMENT", "source": relative,
                              "line": line_no, "symbol": symbol, "contract": line,
                              "kind": "document-table-member" if symbol in table_symbols else "document-inline-or-option",
                              "feature_ids": families, "mapping": mapping,
                              "proposed_case_id": "CASE-" + ident,
                              "verification": "NOT_RUN", "review": "PENDING_INDEPENDENT_APPROVAL"})
        if not families:
            unknowns.append({"id": "UNKNOWN-DOC-" + identifier(relative), "source": relative,
                             "reason": "Document prose and table surfaces require explicit semantic classification",
                             "owner_role": "T04 surface reviewer", "steps": ["T04"], "status": "OPEN"})

    if any(not set(item["feature_ids"]) <= feature_ids for item in items):
        raise ValueError("Unknown feature ID in source mapping")
    ids = [item["id"] for item in items]
    if len(set(ids)) != len(ids):
        raise ValueError("Duplicate catalogue ID")
    flag_pattern = r"(?<![\w-])(?:--[a-z][a-z0-9-]*|-[a-z]+|--)(?![\w-])"
    source_flags = {row["symbol"] for source in discovery["sources"] for row in source["items"]
                    if row["kind"] in {"flag-comparison", "cli-option-schema"}}
    source_flags.update(token for source in discovery["sources"] for row in source["items"]
                        if row["kind"] == "cli-alias-table" for token in re.findall(flag_pattern, row["contract"]))
    source_flags = sorted(source_flags)
    documented_flags = sorted({token for item in items if item["origin"] == "DOCUMENT"
                               for token in re.findall(flag_pattern, item["symbol"])})
    settings_fields = sorted({row["symbol"].split(".", 1)[1]
                              for source in discovery["sources"] for row in source["items"]
                              if row["kind"] == "schema-field" and row["symbol"].startswith("SettingsSchema.")})
    documented_settings = sorted({item["symbol"].split(".", 1)[0] for item in items
                                  if item["origin"] == "DOCUMENT" and item["source"].endswith("/settings.md")
                                  and item["kind"] == "document-table-member"})
    reconciliation = {
        "cli_source_flags": source_flags, "cli_documented_flags": documented_flags,
        "document_flags_without_comparison": sorted(set(documented_flags) - set(source_flags)),
        "source_flags_without_table_or_option_doc": sorted(set(source_flags) - set(documented_flags)),
        "settings_schema_root_fields": settings_fields,
        "settings_documented_root_fields": documented_settings,
        "settings_source_only": sorted(set(settings_fields) - set(documented_settings)),
        "settings_doc_only": sorted(set(documented_settings) - set(settings_fields)),
        "meaning": "Discovery differences, not bugs or compatibility verdicts; T04 must classify provenance and behavior.",
    }
    for category in ("document_flags_without_comparison", "source_flags_without_table_or_option_doc",
                     "settings_source_only", "settings_doc_only"):
        for symbol in reconciliation[category]:
            unknowns.append({"id": "UNKNOWN-RECONCILE-" + identifier(category, symbol),
                             "source": category, "symbol": symbol, "reason": "Source/document inventory mismatch needs semantic classification",
                             "owner_role": "T04 source/document reviewer", "steps": ["T04"], "status": "OPEN"})
    return {"schema": "pangu.migration.surface-catalogue.v1",
            "status": "STATIC_INVENTORY_ONLY_NOT_COMPATIBILITY_CERTIFIED",
            "scope": "Explicit seeds, declared exports/bin patterns for coding-agent/agent/ai, relative reexport closure; all coding-agent docs/*.md",
            "not_claimed": ["External dependency types resolved", "Runtime compatibility", "T04 classification and approval completed"],
            "items": items, "documents": documents, "unknowns": unknowns,
            "source_document_reconciliation": reconciliation,
            "counts": {"ast_items": len(seen), "document_items": len(items) - len(seen),
                       "sources": len(discovery["sources"]), "documents": len(documents),
                       "unknowns": len(unknowns), "proposed_cases": len(items)}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ledger-output", type=Path)
    args = parser.parse_args()
    try:
        root = args.root.resolve()
        discovery_path = root / "docs/migration/pi-public-surface-discovery.json"
        features_path = root / "docs/migration/feature-inventory.json"
        result = build(root, json.loads(discovery_path.read_text()), json.loads(features_path.read_text()))
        result["input_digests"] = {"ast": sha(discovery_path), "features": sha(features_path),
                                   "review_driver": sha(Path(__file__))}
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
        if args.ledger_output:
            scope_path = root / "docs/migration/implementation-scope.json"
            ledger = traceability(result, json.loads(features_path.read_text()), json.loads(scope_path.read_text()))
            ledger["input_digests"] = {"catalogue": sha(args.output), "features": sha(features_path),
                                      "scope": sha(scope_path), "driver": sha(Path(__file__))}
            args.ledger_output.write_text(json.dumps(ledger, ensure_ascii=False, indent=2) + "\n")
    except (OSError, ValueError, KeyError) as error:
        print(f"SURFACE REVIEW FAIL: {error}")
        return 1
    print(json.dumps(result["counts"]))
    print("Unknowns are OPEN; semantic T03/T04 completion is not asserted.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
