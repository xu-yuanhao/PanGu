"""Check real AST discovery; missing parser support must fail rather than skip."""

import argparse
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
from surface_inventory import extract, extract_closure, package_entries


class SurfaceInventoryTests(unittest.TestCase):
    parser_path = None

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pangu-ast-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "fixture.ts"

    def parse(self, text):
        self.source.write_text(text)
        return extract(self.parser_path, [self.source], self.root)[0]["items"]

    def test_flags_are_ast_comparisons_not_comments_or_help_text(self):
        rows = self.parse('''
// arg === "--comment-only"
const help="--not-a-flag";
console.log("--printed-text");
function parse(arg:string) { if(arg==="--real" || arg==="-r") return true; }
''')
        self.assertEqual({r["symbol"] for r in rows if r["kind"] == "flag-comparison"},
                         {"--real", "-r"})

    def test_interfaces_and_nested_contracts_are_preserved(self):
        rows = self.parse('export interface API { invoke(x: {name:string}): Promise<void>; }')
        member = next(r for r in rows if r["kind"] == "interface-member")
        self.assertEqual(member["symbol"], "API.invoke")
        self.assertIn("Promise<void>", member["contract"])

    def test_public_class_members_exclude_private_and_protected(self):
        rows = self.parse('''export class Client {
 private secret(){return 1;} protected hidden=2;
 #internal=3;
 public prompt(text:string):void {throw new Error(text);}
}''')
        members = [r for r in rows if r["kind"] == "class-public-member"]
        self.assertEqual([r["symbol"] for r in members], ["Client.prompt"])
        self.assertNotIn("throw", members[0]["contract"])

    def test_source_is_parsed_without_executing_top_level_code(self):
        rows = self.parse('throw new Error("DO NOT EXECUTE"); export type Mode="text"|"json";')
        self.assertTrue(any(r["symbol"] == "Mode" for r in rows))

    def test_syntax_error_blocks_inventory(self):
        with self.assertRaisesRegex(ValueError, "Syntactic diagnostics"):
            self.parse('export interface Broken { x: ;')

    def test_schema_fields_reexports_and_defaults_are_retained(self):
        rows = self.parse('''
declare const Type:any;
const ConfigSchema=Type.Object({enabled:Type.Optional(Type.Boolean({default:true}))});
export const DEFAULTS={enabled:true};
export {something} from './missing-module.js';
''')
        kinds = {r["kind"] for r in rows}
        self.assertTrue({"schema-field", "reexport", "variable-contract"} <= kinds)
        self.assertTrue(any("default:true" in r["contract"] for r in rows))

    def test_missing_source_is_a_failure(self):
        with self.assertRaisesRegex(ValueError, "Missing source"):
            extract(self.parser_path, [self.source], self.root)

    def test_relative_reexport_closure_and_cycle(self):
        self.source.write_text("export * from './next.js';")
        (self.root / "next.ts").write_text("export * from './fixture.js'; export interface Deep {x:string}")
        sources, edges = extract_closure(self.parser_path, [self.source], self.root, self.root)
        self.assertEqual(len(sources), 2)
        self.assertEqual(len(edges), 2)
        self.assertTrue(all(e["status"] == "RESOLVED" for e in edges))

    def test_missing_reexport_blocks_closure(self):
        self.source.write_text("export * from './missing.js';")
        with self.assertRaisesRegex(ValueError, "Missing or escaping"):
            extract_closure(self.parser_path, [self.source], self.root, self.root)

    def test_external_reexports_remain_explicit_not_fabricated(self):
        self.source.write_text("export type {External} from 'external-package';")
        _, edges = extract_closure(self.parser_path, [self.source], self.root, self.root)
        self.assertEqual(edges[0]["status"], "EXTERNAL_DEPENDENCY_REQUIRES_T07")

    def test_local_named_export_captures_declaration(self):
        rows = self.parse('interface Hidden {value:number} export {Hidden as Public};')
        self.assertTrue(any(r["symbol"] == "Hidden.value" for r in rows))

    def test_tagged_union_variants_include_optional_fields(self):
        rows = self.parse('export type Command={type:"prompt";message:string;id?:string}|{type:"abort"};')
        variants = [r for r in rows if r["kind"] == "tagged-variant"]
        self.assertEqual([r["symbol"] for r in variants], ["Command.prompt", "Command.abort"])
        self.assertTrue(next(f for f in variants[0]["fields"] if f["name"] == "id")["optional"])

    def test_schema_fields_keep_stable_owner(self):
        rows = self.parse('declare const Type:any; const SettingsSchema=Type.Object({enabled:Type.Boolean()});')
        self.assertTrue(any(r["symbol"] == "SettingsSchema.enabled" for r in rows))

    def test_array_membership_flag_checks_are_captured(self):
        rows = self.parse('const args:string[]=[]; if(args.includes("--offline")) {}')
        self.assertTrue(any(r["kind"] == "flag-comparison" and r["symbol"] == "--offline" for r in rows))

    def test_option_dictionary_conditional_and_alias_table(self):
        rows = self.parse('''
declare const args:any,kind:any,parseOptions:any;
const OPTION_ALIASES=new Map([["-l","--local"]]);
parseOptions(args,kind?{url:"value"}:{timeout:"value"});
''')
        self.assertEqual({r["symbol"] for r in rows if r["kind"] == "cli-option-schema"}, {"--url", "--timeout"})
        self.assertTrue(any(r["kind"] == "cli-alias-table" for r in rows))

    def test_package_export_wildcards_and_cli_bundle_mapping(self):
        for name in ("coding-agent", "agent", "ai"):
            folder = self.root / "packages" / name
            (folder / "src").mkdir(parents=True)
            (folder / "src/index.ts").write_text("export {};")
            (folder / "package.json").write_text(json.dumps({
                "name": name, "exports": {".": {"import": "./dist/index.js"}},
            }))
        folder = self.root / "packages/coding-agent"
        (folder / "src/cli.ts").write_text("export {};")
        (folder / "src/public").mkdir()
        (folder / "src/public/one.ts").write_text("export {};")
        (folder / "package.json").write_text(json.dumps({
            "name": "coding-agent", "exports": {"./public/*": "./dist/public/*.js"},
            "bin": {"pi": "dist/bundle/cli.js"},
        }))
        seeds, entries = package_entries(self.root)
        self.assertIn((folder / "src/public/one.ts").resolve(), seeds)
        self.assertIn((folder / "src/cli.ts").resolve(), seeds)
        self.assertTrue(all(e["status"] == "RESOLVED" for e in entries))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--typescript", type=Path, required=True)
    options, remaining = parser.parse_known_args()
    SurfaceInventoryTests.parser_path = options.typescript
    unittest.main(argv=[sys.argv[0], *remaining])
