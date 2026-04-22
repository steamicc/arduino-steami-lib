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
    """Run `gh <args>` and return stdout.

    On failure, prints a human-readable diagnosis (command, stderr, a hint
    about common auth/scope issues) and exits. Raw Python tracebacks are
    useless to somebody running the script for the first time.
    """
    try:
        result = subprocess.run(
            ["gh", *args],
            capture_output=True,
            text=True,
            check=True,
        )
    except FileNotFoundError:
        print("error: `gh` CLI not found in PATH.", file=sys.stderr)
        print("       Install it: https://cli.github.com/", file=sys.stderr)
        sys.exit(127)
    except subprocess.CalledProcessError as exc:
        print(f"error: gh command failed (exit {exc.returncode})", file=sys.stderr)
        print(f"  command: gh {' '.join(args)}", file=sys.stderr)
        stderr = (exc.stderr or "").strip()
        if stderr:
            print(f"  stderr: {stderr}", file=sys.stderr)
        print(
            "  hint:   run `gh auth status` and make sure your token has\n"
            "          repo write + org \"Projects: Read and write\" scopes.",
            file=sys.stderr,
        )
        sys.exit(exc.returncode or 1)
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


def _fetch_merged_map() -> dict[int, bool]:
    """Build pr_number → merged flag via the paginated pulls endpoint.

    The issues listing doesn't expose `merged_at`, so we need another
    source for the "merged vs just closed" bit. Walking /pulls once with
    pagination is O(pages), which beats the N+1 shape of hitting
    /pulls/{n} per closed PR on a repo with hundreds of entries.
    """
    per_page = 100
    page = 1
    merged: dict[int, bool] = {}
    while True:
        batch = json.loads(
            run_gh(
                [
                    "api",
                    f"repos/{REPO}/pulls?state=closed&per_page={per_page}&page={page}",
                ]
            )
        )
        if not batch:
            break
        for pr in batch:
            merged[pr["number"]] = pr.get("merged_at") is not None
        if len(batch) < per_page:
            break
        page += 1
    return merged


def fetch_all_items(limit: int | None) -> list[Item]:
    """Pull every issue and PR (state=all) via REST, newest first."""
    merged_map = _fetch_merged_map()
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
            merged = merged_map.get(entry["number"], False) if is_pr else False
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


_issue_milestone_cache: dict[int, tuple[int, str] | None] = {}


def _issue_milestone(n: int, by_number: dict[int, Item]) -> tuple[int, str] | None:
    """Return (milestone_number, milestone_title) for issue #n, or None.

    Looks in the pre-fetched local map first, then falls back to a REST
    lookup. The fallback matters when --limit truncates the in-memory
    view but a PR within the limit references an older issue outside it;
    without it, inheritance would silently fail to plan updates.
    """
    item = by_number.get(n)
    if item is not None:
        return (
            (item.milestone_number, item.milestone_title or "?")
            if item.milestone_number
            else None
        )
    if n in _issue_milestone_cache:
        return _issue_milestone_cache[n]
    # Direct subprocess call (not run_gh) so a 404 on a deleted/unknown
    # issue doesn't abort the whole backfill. Auth / network failures
    # will still blow up loudly on the next run_gh call downstream.
    probe = subprocess.run(
        ["gh", "api", f"repos/{REPO}/issues/{n}"],
        capture_output=True,
        text=True,
    )
    if probe.returncode != 0:
        _issue_milestone_cache[n] = None
        return None
    data = json.loads(probe.stdout)
    milestone = data.get("milestone") or {}
    result = (
        (milestone["number"], milestone.get("title") or "?")
        if milestone.get("number") is not None
        else None
    )
    _issue_milestone_cache[n] = result
    return result


def find_inheritable_milestone(
    pr: Item, by_number: dict[int, Item]
) -> tuple[int, int, str] | None:
    for n in extract_issue_refs(pr.body):
        hit = _issue_milestone(n, by_number)
        if hit:
            ms_number, ms_title = hit
            return n, ms_number, ms_title
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
