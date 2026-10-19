"""Read back GitHub settings; never infer privilege separation from rules alone."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess


def validate(policy, repository, ruleset):
    if not isinstance(policy, dict) or set(policy) != {"schema", "repository", "visibility", "branch", "ruleset_id", "required_check", "required_approvals"}:
        raise ValueError("invalid remote policy fields")
    if (policy["schema"] != "pangu.github-boundary.v1" or policy["visibility"] != "public"
            or not isinstance(policy["repository"], str) or not re.fullmatch(r"[\w.-]+/[\w.-]+", policy["repository"])
            or policy["branch"] != "main" or type(policy["ruleset_id"]) is not int
            or policy["required_approvals"] != 1 or policy["required_check"] != "pangu/trusted-gates"):
        raise ValueError("unsupported remote policy")
    if repository["full_name"] != policy["repository"] or repository["private"] is not False or repository["visibility"] != "public":
        raise ValueError("repository identity or visibility mismatch")
    if ruleset["id"] != policy["ruleset_id"] or ruleset["target"] != "branch" or ruleset["enforcement"] != "active":
        raise ValueError("ruleset identity or enforcement mismatch")
    if ruleset.get("bypass_actors") != []:
        raise ValueError("ruleset bypass actor forbidden")
    if ruleset["conditions"] != {"ref_name": {"include": ["refs/heads/" + policy["branch"]], "exclude": []}}:
        raise ValueError("protected branch scope mismatch")
    rules = {}
    for item in ruleset["rules"]:
        if item["type"] in rules:
            raise ValueError("duplicate remote rule")
        rules[item["type"]] = item
    if not {"deletion", "non_fast_forward", "pull_request", "required_status_checks"} <= set(rules):
        raise ValueError("required remote rule missing")
    review = rules["pull_request"]["parameters"]
    if (type(review["required_approving_review_count"]) is not int
            or review["required_approving_review_count"] < policy["required_approvals"]
            or any(review.get(key) is not True for key in ("dismiss_stale_reviews_on_push", "require_last_push_approval", "required_review_thread_resolution"))):
        raise ValueError("independent review requirements weakened")
    checks = rules["required_status_checks"]["parameters"]
    if checks.get("strict_required_status_checks_policy") is not True or checks.get("do_not_enforce_on_create") is not False:
        raise ValueError("required check enforcement weakened")
    matches = [entry for entry in checks["required_status_checks"] if entry["context"] == policy["required_check"]]
    if len(matches) != 1:
        raise ValueError("trusted gate status missing or ambiguous")
    return {"status": "BLOCKED", "remote_configuration": "PASS", "repository": policy["repository"],
            "visibility": repository["visibility"], "ruleset_id": ruleset["id"],
            "default_branch": repository.get("default_branch"),
            "automation_has_admin": repository.get("permissions", {}).get("admin", "UNKNOWN"),
            "status_check_app_id": matches[0].get("integration_id", "NOT_PINNED"),
            "protected_executor": "NOT_ESTABLISHED", "independent_approval": "NOT_VERIFIED", "release": "BLOCKED"}


def audit(policy):
    # Validate all endpoint components before invoking the authenticated read-only CLI.
    if (not isinstance(policy, dict) or not isinstance(policy.get("repository"), str)
            or not re.fullmatch(r"[\w.-]+/[\w.-]+", policy["repository"])
            or type(policy.get("ruleset_id")) is not int or policy["ruleset_id"] <= 0):
        raise ValueError("invalid GitHub endpoint")
    gh = shutil.which("gh")
    if not gh:
        raise ValueError("GitHub CLI unavailable")
    def fetch(endpoint, allow_absent=False):
        result = subprocess.run([gh, "api", endpoint], capture_output=True, text=True, timeout=30)
        if result.returncode:
            if allow_absent:
                error = json.loads(result.stdout)
                if str(error.get("status")) in {"404", "409"}:
                    return None
            raise ValueError("GitHub settings read failed: " + result.stderr)
        return json.loads(result.stdout)
    repository = fetch("repos/" + policy["repository"])
    ruleset = fetch("repos/" + policy["repository"] + "/rulesets/" + str(policy["ruleset_id"]))
    report = validate(policy, repository, ruleset)
    head = fetch("repos/" + policy["repository"] + "/git/ref/heads/" + policy["branch"], allow_absent=True)
    report["protected_branch_head"] = head["object"]["sha"] if head else "NOT_CREATED"
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, default=Path(__file__).resolve().parents[2] / "governance/remote-policy.json")
    args = parser.parse_args()
    try:
        print(json.dumps(audit(json.loads(args.policy.read_text())), indent=2))
    except (ValueError, TypeError, KeyError, OSError, subprocess.SubprocessError) as error:
        print("remote boundary FAIL:", error)
        return 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
