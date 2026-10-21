"""UBSan suppression audit of complete preprocessed units with content-addressed reuse."""
from collections import Counter
import hashlib
import re
import subprocess
import tempfile
from pathlib import Path

from module_graph import Invalid
from receipt import canonical


FORBIDDEN = {"NoSanitizeAttr", "DisableSanitizerInstrumentationAttr"}
NODE = re.compile(r"^[ |`-]*([A-Za-z_][A-Za-z_0-9]*)\b")
_CACHE = {}


def audit_source(root, source, compiler, flags, compiler_pin):
    root = Path(root).resolve(strict=True)
    environment = {"PATH": "/usr/bin:/bin", "LC_ALL": "C"}
    # The caller validates the compiler pin against its fixed toolchain before/after builds.
    # Re-preprocess on every call: file-existence predicates, headers and predefined macros
    # can change even when a TU's own bytes and recorded include list do not.
    with tempfile.TemporaryDirectory(prefix="pangu-sanitizer-ast-") as temporary:
        preprocessed = Path(temporary) / ("unit.ii" if source.endswith(".cpp") else "unit.i")
        command = [str(compiler)] + flags + ["-E", "-fno-color-diagnostics", "./" + source, "-o", str(preprocessed)]
        result = subprocess.run(command, cwd=root, env=environment, capture_output=True, text=True, timeout=30)
        if result.returncode or result.stderr.strip():
            raise Invalid("sanitizer preprocessing failed: " + result.stderr)
        pp_sha = hashlib.sha256(preprocessed.read_bytes()).hexdigest()
        stable_flags = [value.replace(str(root), "<candidate>") for value in flags]
        key = (compiler_pin, hashlib.sha256(canonical(stable_flags)).hexdigest(), pp_sha)
        cached = _CACHE.get(key)
        if cached is None:
            language = "c++-cpp-output" if source.endswith(".cpp") else "cpp-output"
            # Search paths are consumed by the complete preprocessing pass; the parser
            # reads that snapshot and must not re-include files. Keep warnings and all
            # language/sanitizer/code semantics flags (no unused-argument warning waiver).
            parse_flags, skip = [], False
            for flag in flags:
                if skip:
                    skip = False
                elif flag == "-I":
                    skip = True
                elif flag not in {"-nostdinc", "-nostdinc++"}:
                    parse_flags.append(flag)
            command = [str(compiler)] + parse_flags + ["-x", language, "-fsyntax-only", "-fno-color-diagnostics",
                                                      "-Xclang", "-ast-dump", str(preprocessed)]
            kinds, first = Counter(), None
            with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as ast:
                result = subprocess.run(command, cwd=root, env=environment, stdout=ast, stderr=subprocess.PIPE,
                                        text=True, timeout=30)
                if result.returncode or result.stderr.strip():
                    raise Invalid("sanitizer AST audit failed: " + result.stderr)
                ast.seek(0)
                for line in ast:
                    match = NODE.match(line)
                    if not match:
                        continue
                    kind = match[1]
                    if first is None:
                        first = kind
                    if kind in FORBIDDEN:
                        raise Invalid("sanitizer suppression attribute forbidden: " + kind)
                    kinds[kind] += 1
            if first != "TranslationUnitDecl" or kinds["TranslationUnitDecl"] != 1:
                raise Invalid("unsupported sanitizer AST root")
            cached = {"sanitizer_source_audit": "PASS", "preprocessed_sha256": pp_sha,
                      "ast_format": "clang-full-preprocessed-text-tree",
                      "ast_kind_histogram_sha256": hashlib.sha256(canonical(dict(kinds))).hexdigest(),
                      "ast_nodes": sum(kinds.values()),
                      "function_declarations": sum(kinds[k] for k in ("FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl")),
                      "forbidden_attributes": [],
                      "scope": "undefined-behavior trap instrumentation, no source suppression; not proof of no UB"}
            _CACHE[key] = cached
        return dict(cached, source_sha256=hashlib.sha256((root / source).read_bytes()).hexdigest())
