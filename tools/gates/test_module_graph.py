import copy
import unittest

from module_graph import Invalid, check_include, validate


def fixture():
    return {"schema": "pangu.modules.v1", "modules": [
        {"id": "api", "role": "interface", "scope": "product", "files": ["api/public.h", "api/private.h"],
         "public_headers": ["api/public.h"], "dependencies": []},
        {"id": "loop", "role": "plugin", "scope": "product", "files": ["loop/main.c"],
         "public_headers": [], "dependencies": ["api"]}]}


class ModuleGraphTests(unittest.TestCase):
    def test_valid(self):
        check_include(fixture(), "loop/main.c", "api/public.h")

    def test_private_header(self):
        with self.assertRaisesRegex(Invalid, "private header"):
            check_include(fixture(), "loop/main.c", "api/private.h")

    def test_unknown_source(self):
        with self.assertRaisesRegex(Invalid, "unknown compiler input"):
            check_include(fixture(), "unknown.c", "api/public.h")

    def test_undeclared_include(self):
        value = fixture()
        value["modules"][1]["dependencies"] = []
        with self.assertRaisesRegex(Invalid, "undeclared"):
            check_include(value, "loop/main.c", "api/public.h")

    def test_rejected_manifests(self):
        mutations = [
            (0, "dependencies", ["missing"], "unknown dependency"),
            (0, "dependencies", ["loop"], "interface depends"),
            (1, "role", "host", None),
            (0, "scope", "development", "non-product"),
            (0, "files", ["../escape.h"], "unsafe"),
            (0, "files", ["references/pi/test.h"], "unsafe"),
            (0, "files", ["api/test.rs"], "language"),
            (0, "files", ["loop/main.c"], "public headers"),
            (0, "public_headers", ["outside.h"], "public headers"),
            (1, "dependencies", ["api", "api"], "duplicate"),
            (1, "role", "unknown", "role"),
        ]
        for index, key, new, message in mutations:
            value = copy.deepcopy(fixture())
            value["modules"][index][key] = new
            if message is None:
                validate(value)
            else:
                with self.subTest(key=key, new=new), self.assertRaisesRegex(Invalid, message):
                    validate(value)

    def test_host_business(self):
        value = fixture()
        value["modules"][0]["role"] = "host"
        value["modules"][0]["dependencies"] = ["loop"]
        with self.assertRaisesRegex(Invalid, "host depends"):
            validate(value)

    def test_cycle(self):
        value = fixture()
        value["modules"][0]["role"] = "plugin"
        value["modules"][0]["dependencies"] = ["loop"]
        with self.assertRaisesRegex(Invalid, "cycle"):
            validate(value)

    def test_duplicate_owner(self):
        value = fixture()
        value["modules"][1]["files"].append("api/public.h")
        with self.assertRaisesRegex(Invalid, "multiple file owners"):
            validate(value)

    def test_unknown_fields(self):
        value = fixture()
        value["approved"] = True
        with self.assertRaisesRegex(Invalid, "unknown manifest fields"):
            validate(value)


if __name__ == "__main__":
    unittest.main()
