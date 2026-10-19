"""Extract TypeScript declarations with the pinned TS 7 AST API, without executing Pi.

This produces discovery evidence for T03, not a conformance verdict. The parser
is a developer-only dependency installed outside the product checkout.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


PARSER_VERSION = "7.0.2"
NODE_PROGRAM = r"""
import fs from 'node:fs';
import { pathToFileURL } from 'node:url';
const request=JSON.parse(fs.readFileSync(0,'utf8'));
const {API}=await import(pathToFileURL(request.parser+'/dist/api/sync/api.js'));
const {SyntaxKind:K}=await import(pathToFileURL(request.parser+'/dist/ast/index.js'));
const api=new API({cwd:request.cwd});
const output=[];
try {
 const snapshot=api.updateSnapshot({openFiles:request.files});
 for(const file of request.files) {
  const project=snapshot.getDefaultProjectForFile(file);
  if(!project) throw Error('No parser project: '+file);
  const diagnostics=project.program.getSyntacticDiagnostics(file);
  if(diagnostics.length) throw Error('Syntactic diagnostics: '+file);
  const source=project.program.getSourceFile(file);
  if(!source) throw Error('Source unavailable: '+file);
  const rows=[];
  const text=n=>n.getText(source);
  const line=n=>source.text.slice(0,n.getStart(source)).split('\n').length;
  const name=n=>n.name?text(n.name):'<anonymous>';
  const privateMember=n=>(n.modifiers??[]).some(m=>m.kind===K.PrivateKeyword||m.kind===K.ProtectedKeyword)||name(n).startsWith('#');
  const localExports=new Set(source.statements.filter(n=>n.kind===K.ExportDeclaration&&!n.moduleSpecifier)
    .flatMap(n=>(n.exportClause?.elements??[]).map(e=>text(e.propertyName??e.name))));
  const exported=n=>(n.modifiers??[]).some(m=>m.kind===K.ExportKeyword)||localExports.has(name(n));
  function row(kind,n,symbol,contract) {
   const result={kind,symbol,line:line(n),start:n.getStart(source),end:n.end,contract};
   rows.push(result); return result;
  }
  function declaration(n) {
   if(n.kind===K.InterfaceDeclaration&&exported(n)) {
    row('exported-interface',n,name(n),n.heritageClauses?.map(text).join(' ')??'');
    for(const member of n.members) row('interface-member',member,name(n)+'.'+name(member),text(member));
   } else if(n.kind===K.TypeAliasDeclaration&&exported(n)) {
    row('exported-type',n,name(n),text(n.type));
    const variants=n.type.kind===K.UnionType?n.type.types:[n.type];
    for(const variant of variants) if(variant.kind===K.TypeLiteral) {
     const tag=variant.members.find(m=>name(m)==='type'&&m.type?.kind===K.LiteralType
       &&m.type.literal.kind===K.StringLiteral);
     if(tag) {
      const result=row('tagged-variant',variant,name(n)+'.'+tag.type.literal.text,text(variant));
      result.fields=variant.members.map(m=>({name:name(m),optional:m.postfixToken?.kind===K.QuestionToken,contract:text(m)}));
     }
    }
   } else if(n.kind===K.ClassDeclaration&&exported(n)) {
    row('exported-class',n,name(n),n.heritageClauses?.map(text).join(' ')??'');
    for(const member of n.members) if(!privateMember(member)) {
     const signature=member.body?source.text.slice(member.getStart(source),member.body.pos).trim():text(member);
     row('class-public-member',member,name(n)+'.'+name(member),signature);
    }
   } else if(n.kind===K.FunctionDeclaration&&exported(n)) {
    row('exported-function',n,name(n),n.body?source.text.slice(n.getStart(source),n.body.pos).trim():text(n));
   } else if(n.kind===K.ExportDeclaration) {
    const result=row('reexport',n,n.moduleSpecifier?text(n.moduleSpecifier):'<local>',text(n));
    result.module_specifier=n.moduleSpecifier?.text??null;
    result.exported_names=n.exportClause?.elements?.map(e=>text(e.name))??['*'];
   } else if(n.kind===K.VariableStatement) {
    for(const d of n.declarationList.declarations) if(exported(n)||/schema$/i.test(name(d))) {
     row('variable-contract',d,name(d),d.initializer?text(d.initializer):d.type?text(d.type):'<no type>');
    }
   }
  }
  for(const statement of source.statements) declaration(statement);
  function walk(n) {
   if(n.kind===K.VariableDeclaration&&name(n)==='OPTION_ALIASES') {
    row('cli-alias-table',n,name(n),n.initializer?text(n.initializer):text(n));
   }
   if(n.kind===K.CallExpression&&text(n.expression)==='parseOptions'&&n.arguments[1]) {
    function options(value) {
     if(value.kind===K.ObjectLiteralExpression) for(const prop of value.properties) {
      if(prop.name) row('cli-option-schema',prop,'--'+(prop.name.text??name(prop)),text(prop));
     }
     else if(value.kind===K.ConditionalExpression) {options(value.whenTrue);options(value.whenFalse);}
    }
    options(n.arguments[1]);
   }
   if(n.kind===K.CallExpression&&text(n.expression)==='Type.Object'&&n.arguments[0]?.kind===K.ObjectLiteralExpression) {
    let owner=n.parent;
    while(owner&&owner.kind!==K.VariableDeclaration&&owner.kind!==K.FunctionDeclaration) owner=owner.parent;
    for(const prop of n.arguments[0].properties) row('schema-field',prop,(owner?name(owner):'Type.Object@'+line(n))+'.'+name(prop),text(prop));
   }
   if(n.kind===K.StringLiteral&&/^(--[a-z][a-z0-9-]*|-[a-z]+|--)$/.test(n.text)
      &&(n.parent.kind===K.BinaryExpression||n.parent.kind===K.CaseClause
         ||n.parent.kind===K.CallExpression&&n.parent.expression.kind===K.PropertyAccessExpression
           &&['includes','indexOf','startsWith','has'].includes(n.parent.expression.name.text))) {
    row('flag-comparison',n,n.text,text(n.parent));
   }
   n.forEachChild(walk);
  }
  walk(source);
  output.push({path:file,syntactic_diagnostics:0,items:rows});
 }
 snapshot.dispose();
 process.stdout.write(JSON.stringify(output));
} finally {api.close();}
"""

DEFAULT_FILES = [
    "packages/coding-agent/src/cli/args.ts",
    "packages/coding-agent/src/main.ts",
    "packages/coding-agent/src/core/settings-schema.ts",
    "packages/coding-agent/src/core/settings-defaults.ts",
    "packages/coding-agent/src/core/settings-manager.ts",
    "packages/coding-agent/src/core/session-manager.ts",
    "packages/coding-agent/src/core/messages.ts",
    "packages/coding-agent/src/core/agent-session.ts",
    "packages/coding-agent/src/core/agent-session-runtime.ts",
    "packages/coding-agent/src/core/sdk.ts",
    "packages/coding-agent/src/core/extensions/types.ts",
    "packages/coding-agent/src/core/extensions/index.ts",
    "packages/coding-agent/src/modes/rpc/rpc-types.ts",
    "packages/coding-agent/src/modes/json-event.ts",
    "packages/coding-agent/src/index.ts",
    "packages/agent/src/types.ts",
    "packages/agent/src/agent.ts",
    "packages/agent/src/index.ts",
    "packages/ai/src/types.ts",
    "packages/ai/src/index.ts",
] + [f"packages/coding-agent/src/core/tools/{name}.ts" for name in (
    "read", "write", "edit", "bash", "powershell", "grep", "find", "ls")]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_entries(pi):
    """Enumerate declared entry patterns, including wildcard subpaths and CLI bins."""
    seeds = set()
    records = []
    for package in ("coding-agent", "agent", "ai"):
        folder = pi / "packages" / package
        manifest = json.loads((folder / "package.json").read_text())

        def strings(value):
            if isinstance(value, str):
                return [value]
            if isinstance(value, dict):
                return [s for v in value.values() for s in strings(v)]
            if isinstance(value, list):
                return [s for v in value for s in strings(v)]
            return []

        declarations = [("export:" + name, target) for name, target in manifest.get("exports", {}).items()]
        declarations += [("bin:" + name, target) for name, target in manifest.get("bin", {}).items()]
        for public_name, target in declarations:
            for emitted in sorted(set(strings(target))):
                if emitted == "./package.json":
                    records.append({"package": manifest["name"], "public_name": public_name,
                                    "emitted": emitted, "source_paths": [],
                                    "manifest": str((folder / "package.json").resolve()),
                                    "manifest_sha256": sha(folder / "package.json"),
                                    "status": "PACKAGE_METADATA", "sha256": sha(folder / "package.json")})
                    continue
                relative = emitted.removeprefix("./dist/").removeprefix("dist/")
                if relative.endswith(".d.ts"):
                    relative = relative[:-5] + ".ts"
                elif relative.endswith(".js"):
                    relative = relative[:-3] + ".ts"
                patterns = [relative]
                if relative.startswith("bundle/"):
                    patterns.append(relative.removeprefix("bundle/"))
                matches = sorted({p.resolve() for pattern in patterns
                                  for p in (folder / "src").glob(pattern) if p.is_file()})
                records.append({"package": manifest["name"], "public_name": public_name,
                                "emitted": emitted, "source_paths": [str(p) for p in matches],
                                "manifest": str((folder / "package.json").resolve()),
                                "manifest_sha256": sha(folder / "package.json"),
                                "status": "RESOLVED" if matches else "UNRESOLVED_SOURCE_MAPPING"})
                seeds.update(matches)
    return seeds, records


def extract(parser, files, cwd):
    parser = parser.resolve()
    package_path = parser / "package.json"
    package = json.loads(package_path.read_text())
    if package.get("name") != "typescript" or package.get("version") != PARSER_VERSION:
        raise ValueError("Expected developer-only typescript@7.0.2")
    paths = [p.resolve() for p in files]
    for path in paths:
        if not path.is_file():
            raise ValueError(f"Missing source: {path}")
    source_digests = {path: sha(path) for path in paths}
    request = {"parser": str(parser), "files": [str(p) for p in paths], "cwd": str(cwd.resolve())}
    result = subprocess.run(
        ["node", "--input-type=module", "--eval", NODE_PROGRAM],
        input=json.dumps(request), text=True, capture_output=True, timeout=120,
    )
    if result.returncode:
        raise ValueError(f"AST extraction failed: {result.stderr.strip()}")
    data = json.loads(result.stdout)
    if [x["path"] for x in data] != request["files"]:
        raise ValueError("Parser did not return all requested sources")
    if any(sha(path) != before for path, before in source_digests.items()):
        raise ValueError("Source changed during AST extraction")
    return data


def extract_closure(parser, files, cwd, source_root):
    """Follow relative public reexports; retain external edges as explicit dependencies."""
    source_root = source_root.resolve()
    queue = sorted({p.resolve() for p in files})
    seen = set()
    sources = []
    edges = []
    while queue:
        batch = extract(parser, queue, cwd)
        seen.update(queue)
        sources.extend(batch)
        pending = set()
        for source in batch:
            for item in source["items"]:
                if item["kind"] != "reexport" or not item["module_specifier"]:
                    continue
                spec = item["module_specifier"]
                edge = {"from": source["path"], "specifier": spec,
                        "line": item["line"], "exported_names": item["exported_names"]}
                if not spec.startswith("."):
                    edge["status"] = "EXTERNAL_DEPENDENCY_REQUIRES_T07"
                else:
                    target = (Path(source["path"]).parent / spec).resolve()
                    candidates = [target, target.with_suffix(".ts"), target / "index.ts"]
                    target = next((p for p in candidates if p.is_file()), None)
                    if target is None or not target.is_relative_to(source_root):
                        raise ValueError(f"Missing or escaping public reexport: {spec}")
                    edge.update({"status": "RESOLVED", "to": str(target)})
                    if target not in seen:
                        pending.add(target)
                edges.append(edge)
        queue = sorted(pending - seen)
    return sorted(sources, key=lambda s: s["path"]), edges


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--typescript", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        driver_digest = sha(Path(__file__))
        pi = args.root / "references/pi"
        entry_seeds, entries = package_entries(pi)
        cli_seeds = list((pi / "packages/coding-agent/src/cli").glob("*.ts"))
        cli_seeds += [pi / "packages/coding-agent/src/package-manager-cli.ts",
                      pi / "packages/coding-agent/src/extensions/mcp/cli.ts"]
        data, edges = extract_closure(args.typescript, [pi / f for f in DEFAULT_FILES] + list(entry_seeds) + cli_seeds,
                                      args.typescript.parent.parent, pi)
        for entry in entries:
            entry["manifest"] = Path(entry["manifest"]).relative_to(args.root.resolve()).as_posix()
            entry["source_paths"] = [Path(p).relative_to(args.root.resolve()).as_posix()
                                     for p in entry["source_paths"]]
        for edge in edges:
            for key in ("from", "to"):
                if key in edge:
                    edge[key] = Path(edge[key]).relative_to(args.root.resolve()).as_posix()
        for source in data:
            path = Path(source["path"])
            source["path"] = path.relative_to(args.root.resolve()).as_posix()
            source["sha256"] = sha(path)
            for i, item in enumerate(source["items"]):
                item["id"] = hashlib.sha256(
                    (source["path"] + ":" + str(item["start"]) + ":" + item["kind"]
                     + ":" + item["symbol"]).encode()
                ).hexdigest()[:20]
                item["review_status"] = "UNREVIEWED"
        output = {
            "schema": "pangu.migration.ast-discovery.v1", "task": "T03",
            "status": "DISCOVERY_ONLY_T03_NOT_COMPLETE", "parser_version": PARSER_VERSION,
            "parser_package_sha256": sha(args.typescript / "package.json"),
            "driver_sha256": driver_digest, "sources": data,
            "reexport_edges": edges,
            "package_entries": entries,
            "limitations": ["Not a runtime or compatibility test.",
                             "Relative reexport closure is resolved; external package edges require T07 dependency review.",
                             "Defaults/constraints/precedence and document-only surfaces need T03 semantic reconciliation.",
                             "Raw declared types/schema expressions are preserved; no fabricated resolved type claims."],
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        if sha(Path(__file__)) != driver_digest:
            raise ValueError("AST driver changed during extraction")
        args.output.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n")
    except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        print(f"SURFACE FAIL: {error}")
        return 1
    print(f"SURFACE DISCOVERED: {len(data)} source files, {sum(len(s['items']) for s in data)} AST items")
    print("T03 completion and compatibility: NOT VERIFIED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
