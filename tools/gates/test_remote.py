import copy
import unittest

from remote import validate


class RemoteBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.policy = {"schema": "pangu.github-boundary.v1", "repository": "owner/repo", "visibility": "public", "branch": "main",
                       "ruleset_id": 123, "required_check": "pangu/trusted-gates", "required_approvals": 1}
        self.repo = {"full_name": "owner/repo", "private": False, "visibility": "public", "default_branch": "main", "permissions": {"admin": True}}
        self.ruleset = {"id": 123, "target": "branch", "enforcement": "active", "bypass_actors": [],
                        "conditions": {"ref_name": {"include": ["refs/heads/main"], "exclude": []}}, "rules": [
                            {"type": "deletion"}, {"type": "non_fast_forward"},
                            {"type": "pull_request", "parameters": {"required_approving_review_count": 1,
                                "dismiss_stale_reviews_on_push": True, "require_last_push_approval": True, "required_review_thread_resolution": True}},
                            {"type": "required_status_checks", "parameters": {"strict_required_status_checks_policy": True,
                                "do_not_enforce_on_create": False, "required_status_checks": [{"context": "pangu/trusted-gates"}]}}]}

    def test_active_rules_are_not_privilege_separation(self):
        report = validate(self.policy, self.repo, self.ruleset)
        self.assertEqual(report["remote_configuration"], "PASS")
        self.assertTrue(report["automation_has_admin"])
        self.assertEqual(report["protected_executor"], "NOT_ESTABLISHED")
        self.assertEqual(report["status_check_app_id"], "NOT_PINNED")
        self.assertEqual(report["status"], "BLOCKED")

    def test_no_bypass(self):
        self.ruleset["bypass_actors"] = [{"actor_type": "RepositoryRole", "actor_id": 5}]
        with self.assertRaisesRegex(ValueError, "bypass"):
            validate(self.policy, self.repo, self.ruleset)

    def test_disabled_rules(self):
        self.ruleset["enforcement"] = "disabled"
        with self.assertRaisesRegex(ValueError, "enforcement"):
            validate(self.policy, self.repo, self.ruleset)

    def test_scope_exclusion(self):
        self.ruleset["conditions"]["ref_name"]["exclude"] = ["refs/heads/main"]
        with self.assertRaisesRegex(ValueError, "scope"):
            validate(self.policy, self.repo, self.ruleset)

    def test_rule_removed(self):
        self.ruleset["rules"].pop(0)
        with self.assertRaisesRegex(ValueError, "rule missing"):
            validate(self.policy, self.repo, self.ruleset)

    def test_review_cannot_be_relaxed(self):
        for field, value in (("required_approving_review_count", 0), ("dismiss_stale_reviews_on_push", False), ("require_last_push_approval", False)):
            ruleset = copy.deepcopy(self.ruleset)
            ruleset["rules"][2]["parameters"][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "review"):
                validate(self.policy, self.repo, ruleset)

    def test_check_cannot_be_optional(self):
        self.ruleset["rules"][3]["parameters"]["do_not_enforce_on_create"] = True
        with self.assertRaisesRegex(ValueError, "enforcement weakened"):
            validate(self.policy, self.repo, self.ruleset)

    def test_wrong_status_name(self):
        self.ruleset["rules"][3]["parameters"]["required_status_checks"] = [{"context": "fake/PASS"}]
        with self.assertRaisesRegex(ValueError, "status missing"):
            validate(self.policy, self.repo, self.ruleset)

    def test_visibility_mismatch(self):
        self.repo["private"] = True
        with self.assertRaisesRegex(ValueError, "visibility"):
            validate(self.policy, self.repo, self.ruleset)


if __name__ == "__main__":
    unittest.main()
