#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One-shot backfill for the STeaMi org project.

The project-sync workflow only fires on new events — everything that was
already closed before it landed stays off the board (or worse, on the
board with a stale Status). Similarly, the milestone-from-issue workflow
only stamps milestones on PRs opened or edited after it shipped.

This script walks the repo and brings the existing state in line:

  - PRs without a milestone inherit it from the first Closes/Fixes/Resolves
    reference that points to a milestoned issue (same rule as the CI).
  - Every issue and PR is added to the STeaMi org project (idempotent).
  - Closed issues and closed/merged PRs are moved to Status="Done".
  - Open items are added but their Status is left alone so the maintainer
    can triage them manually.

Dry-run by default — nothing is mutated until --apply is passed. The
script relies on the local `gh` CLI being authenticated with a token
that has repo write access and "Projects: Read and write" on the
steamicc org.

Usage:

    python3 scripts/project-backfill.py               # dry-run
    python3 scripts/project-backfill.py --apply       # commit changes
    python3 scripts/project-backfill.py --limit 50    # cap for a smoke test
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from typing import Any

REPO = "steamicc/arduino-steami-lib"

# IDs harvested from the STeaMi org project (steamicc/projects/1). If the
# project is ever recreated or the Status field reshuffled, re-run the
# discovery GraphQL query and update these constants.
PROJECT_ID = "PVT_kwDOCmCNJs4ApueY"
STATUS_FIELD_ID = "PVTSSF_lADOCmCNJs4ApueYzghFrGs"
STATUS_OPTIONS = {
    "Backlog": "f75ad846",
    "Ready": "08afe404",
    "In progress": "47fc9ee4",
    "In review": "4cc61d42",
    "Done": "98236657",
}

# Matches the same closing keywords the CI workflow uses. `\b` anchors
# the start so `discloses #N` doesn't trip the match.
CLOSING_PATTERN = re.compile(
    r"\b(?:close[sd]?|fix(?:e[sd])?|resolve[sd]?)\s*:?\s*#(\d+)",
    re.IGNORECASE,
)


# ---------- gh plumbing ----------


def run_gh(args: list[str]) -> str:
    """Run `gh <args>` and return stdout, raising on non-zero exit."""
    result = subprocess.run(
        ["gh", *args],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout


def gh_json(args: list[str]) -> Any:
    return json.loads(run_gh(args))


def graphql(query: str, variables: dict[str, Any]) -> dict[str, Any]:
    # gh api graphql -F pairs get interpreted as numbers when they parse
    # as such, which collides with node IDs that happen to be all digits.
    # Pass everything as -f strings except booleans / integers we need
    # as typed (none in this script) to sidestep that.
    args = ["api", "graphql", "-f", f"query={query}"]
    for key, value in variables.items():
        args.extend(["-f", f"{key}={value}"])
    return json.loads(run_gh(args))["data"]


# ---------- data model ----------


@dataclass
class Item:
    number: int
    kind: str  # "issue" or "pr"
    state: str  # "OPEN" or "CLOSED"
    merged: bool
    milestone_number: int | None
    milestone_title: str | None
    body: str
    node_id: str
    on_project: bool
    item_id: str | None
    current_status_option_id: str | None

    @property
    def label(self) -> str:
        return f"{self.kind.upper()} #{self.number}"


# ---------- repo fetch ----------


def fetch_all_items(limit: int | None) -> list[Item]:
    """Pull every issue and PR (state=all) via REST, newest first."""
    per_page = 100
    page = 1
    items: list[Item] = []
    while True:
        raw = run_gh(
            [
                "api",
                f"repos/{REPO}/issues?state=all&per_page={per_page}&page={page}",
            ]
        )
        batch = json.loads(raw)
        if not batch:
            break
        for entry in batch:
            is_pr = "pull_request" in entry
            merged = False
            if is_pr and entry["state"] == "closed":
                # The cheap REST listing doesn't include merged_at on
                # the issue-view of a PR, so hit the PR endpoint.
                pr_data = json.loads(
                    run_gh(["api", f"repos/{REPO}/pulls/{entry['number']}"])
                )
                merged = pr_data.get("merged_at") is not None
            milestone = entry.get("milestone") or {}
            items.append(
                Item(
                    number=entry["number"],
                    kind="pr" if is_pr else "issue",
                    state=entry["state"].upper(),
                    merged=merged,
                    milestone_number=milestone.get("number"),
                    milestone_title=milestone.get("title"),
                    body=entry.get("body") or "",
                    node_id=entry["node_id"],
                    on_project=False,
                    item_id=None,
                    current_status_option_id=None,
                )
            )
            if limit is not None and len(items) >= limit:
                return items
        if len(batch) < per_page:
            break
        page += 1
    return items


def fetch_project_index() -> dict[str, dict[str, Any]]:
    """Map content node_id → {item_id, status_option_id} for items on the project."""
    index: dict[str, dict[str, Any]] = {}
    cursor: str | None = None
    while True:
        query = """
        query($project: ID!, $cursor: String) {
          node(id: $project) {
            ... on ProjectV2 {
              items(first: 100, after: $cursor) {
                pageInfo { hasNextPage endCursor }
                nodes {
                  id
                  content {
                    __typename
                    ... on Issue { id }
                    ... on PullRequest { id }
                  }
                  fieldValues(first: 20) {
                    nodes {
                      __typename
                      ... on ProjectV2ItemFieldSingleSelectValue {
                        field { ... on ProjectV2SingleSelectField { id } }
                        optionId
                      }
                    }
                  }
                }
              }
            }
          }
        }
        """
        variables = {"project": PROJECT_ID}
        if cursor is not None:
            variables["cursor"] = cursor
        data = graphql(query, variables)
        page = data["node"]["items"]
        for node in page["nodes"]:
            content = node.get("content") or {}
            content_id = content.get("id")
            if not content_id:
                continue
            status_option_id = None
            for fv in node["fieldValues"]["nodes"] or []:
                if fv.get("__typename") != "ProjectV2ItemFieldSingleSelectValue":
                    continue
                if fv.get("field", {}).get("id") == STATUS_FIELD_ID:
                    status_option_id = fv.get("optionId")
                    break
            index[content_id] = {
                "item_id": node["id"],
                "status_option_id": status_option_id,
            }
        if not page["pageInfo"]["hasNextPage"]:
            break
        cursor = page["pageInfo"]["endCursor"]
    return index


# ---------- backfill logic ----------


def extract_issue_refs(body: str) -> list[int]:
    seen: set[int] = set()
    refs: list[int] = []
    for match in CLOSING_PATTERN.finditer(body):
        n = int(match.group(1))
        if n not in seen:
            seen.add(n)
            refs.append(n)
    return refs


def find_inheritable_milestone(
    pr: Item, by_number: dict[int, Item]
) -> tuple[int, int, str] | None:
    for n in extract_issue_refs(pr.body):
        candidate = by_number.get(n)
        if candidate and candidate.milestone_number:
            return n, candidate.milestone_number, candidate.milestone_title or "?"
    return None


def desired_status(item: Item) -> str | None:
    if item.state == "CLOSED":
        return "Done"
    # Leave open items untouched — the maintainer may have triaged them.
    return None


# ---------- mutations ----------


def set_milestone(pr_number: int, milestone_number: int) -> None:
    run_gh(
        [
            "api",
            f"repos/{REPO}/issues/{pr_number}",
            "--method",
            "PATCH",
            "-F",
            f"milestone={milestone_number}",
            "--silent",
        ]
    )


def add_to_project(content_node_id: str) -> str:
    query = """
    mutation($project: ID!, $content: ID!) {
      addProjectV2ItemById(input: { projectId: $project, contentId: $content }) {
        item { id }
      }
    }
    """
    data = graphql(query, {"project": PROJECT_ID, "content": content_node_id})
    return data["addProjectV2ItemById"]["item"]["id"]


def set_status(item_id: str, option_id: str) -> None:
    query = """
    mutation($project: ID!, $item: ID!, $field: ID!, $value: String!) {
      updateProjectV2ItemFieldValue(input: {
        projectId: $project
        itemId: $item
        fieldId: $field
        value: { singleSelectOptionId: $value }
      }) { projectV2Item { id } }
    }
    """
    graphql(
        query,
        {
            "project": PROJECT_ID,
            "item": item_id,
            "field": STATUS_FIELD_ID,
            "value": option_id,
        },
    )


# ---------- main ----------


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--apply",
        action="store_true",
        help="actually mutate state (default is a dry-run)",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=None,
        help="cap the number of items processed (handy for smoke tests)",
    )
    args = parser.parse_args()

    mode = "APPLY" if args.apply else "DRY-RUN"
    print(f"[{mode}] Fetching {REPO} issues and PRs…")
    items = fetch_all_items(args.limit)
    print(f"[{mode}] {len(items)} items fetched.")

    print(f"[{mode}] Fetching STeaMi project index…")
    project_index = fetch_project_index()
    print(f"[{mode}] {len(project_index)} items already on the project.")

    by_number = {it.number: it for it in items}
    for it in items:
        entry = project_index.get(it.node_id)
        if entry:
            it.on_project = True
            it.item_id = entry["item_id"]
            it.current_status_option_id = entry["status_option_id"]

    milestone_fixes: list[tuple[Item, int, int, str]] = []
    project_adds: list[Item] = []
    status_sets: list[tuple[Item, str]] = []

    for it in items:
        # 1) milestone inheritance — PRs only, and only when they close
        #    an issue that has a milestone.
        if it.kind == "pr" and it.milestone_number is None:
            inherited = find_inheritable_milestone(it, by_number)
            if inherited:
                source_n, ms_n, ms_title = inherited
                milestone_fixes.append((it, source_n, ms_n, ms_title))

        # 2) project membership — add anything that isn't on the board.
        if not it.on_project:
            project_adds.append(it)

        # 3) status transition — only set Done for closed items. If it's
        #    already Done (from a previous run), skip.
        target = desired_status(it)
        if target:
            target_option = STATUS_OPTIONS[target]
            if it.current_status_option_id != target_option:
                status_sets.append((it, target))

    print()
    print("=" * 72)
    print("Plan")
    print("=" * 72)

    if milestone_fixes:
        print(f"\n-- Milestone inheritance ({len(milestone_fixes)} PR(s)) --")
        for pr, src, _num, title in milestone_fixes:
            print(f"  {pr.label} ← milestone '{title}' (from issue #{src})")
    else:
        print("\n-- Milestone inheritance: nothing to do --")

    if project_adds:
        print(f"\n-- Add to project ({len(project_adds)} item(s)) --")
        for it in project_adds:
            print(f"  {it.label} [{it.state}]")
    else:
        print("\n-- Add to project: nothing to do --")

    if status_sets:
        print(f"\n-- Status transitions ({len(status_sets)} item(s)) --")
        for it, target in status_sets:
            print(f"  {it.label} [{it.state}] → Status = {target}")
    else:
        print("\n-- Status transitions: nothing to do --")

    total = len(milestone_fixes) + len(project_adds) + len(status_sets)
    print()
    print("=" * 72)
    print(f"Total actions: {total}")
    print("=" * 72)

    if not args.apply:
        print(
            "\nDry-run complete. Re-run with --apply to commit the changes above."
        )
        return 0

    if total == 0:
        print("\nNothing to apply.")
        return 0

    print(f"\n[{mode}] Applying {total} action(s)…")

    for pr, src, ms_num, ms_title in milestone_fixes:
        print(f"  milestone → {pr.label} (from issue #{src}, '{ms_title}')")
        set_milestone(pr.number, ms_num)

    for it in project_adds:
        print(f"  project-add → {it.label}")
        it.item_id = add_to_project(it.node_id)
        it.on_project = True

    for it, target in status_sets:
        if not it.item_id:
            # It was added just above; the add returned an item_id.
            # If for some reason it didn't, skip safely.
            print(
                f"  status-skip → {it.label} (no item_id, add likely failed)"
            )
            continue
        print(f"  status → {it.label} = {target}")
        set_status(it.item_id, STATUS_OPTIONS[target])

    print("\nDone.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
