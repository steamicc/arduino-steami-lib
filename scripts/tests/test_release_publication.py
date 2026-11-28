# SPDX-License-Identifier: GPL-3.0-or-later
"""Check that no release is lost between the tag and the Library Manager.

publish-arduino-package.yml has one concurrency group per tag: a run never
cancels the publication of another tag (a fixed group keeps a single pending
run, and the 2026-11-27 burst lost v0.42.1 to v0.43.0 that way). Runs for
different tags then share the distribution repository, so its push is atomic
and retried on the fresh main, main only moves for the highest version, and
Latest is given back to the highest published version after each edit.

release.yml runs one semantic-release at a time. Every App token is created
by actions/create-github-app-token pinned to v3.3.0, with an explicit owner,
repositories and the minimal permissions of the steps that use it.
"""

from __future__ import annotations

import importlib.util
import json
import os
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
WORKFLOWS = ROOT / ".github" / "workflows"
RELEASE = "release.yml"
PUBLISH = "publish-arduino-package.yml"
ARCHIVE = "examples-archive.yml"

TOKEN_ACTION = "actions/create-github-app-token@64ab729a6294000f5aa4a4bc0b73d1f32bc45510"
TOKEN_VERSION = "# v3.3.0"
DIST_REPO = "steamicc/arduino-steami"

PUSH_STEP = "Push the package to the distribution repository"
LATEST_STEP = "Publish the release"


def load(name: str):
    spec = importlib.util.spec_from_file_location(name, HERE / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


pw = load("test_publication_workflows")
wv = pw.wv


def job_steps(workflow: str, job: str) -> dict[str, str]:
    jobs = pw.block(pw.read(workflow), "jobs", 0)
    return pw.named_steps(pw.block(jobs, job, 2))


def permissions(step: str) -> dict[str, str]:
    found = {}
    for line in wv.step_lines(step, "permission-"):
        key, value = line.split(":", 1)
        found[key.removeprefix("permission-")] = value.split("#", 1)[0].strip()
    return found


def token_steps() -> list[tuple[str, str]]:
    """(workflow, step) for every step that creates an App token."""
    found = []
    for workflow in sorted(WORKFLOWS.glob("*.yml")):
        for step in wv.workflow_steps(workflow.read_text(encoding="utf-8")):
            uses = wv.step_uses(step)
            if "github-app-token" in uses:
                found.append((workflow.name, step))
    return found


class ConcurrencyTest(unittest.TestCase):
    def test_release_runs_one_at_a_time(self) -> None:
        concurrency = pw.block(pw.read(RELEASE), "concurrency", 0)
        self.assertIn("  group: release", concurrency.splitlines())
        self.assertIn("  cancel-in-progress: false", concurrency.splitlines())

    def test_publication_has_one_group_per_tag(self) -> None:
        concurrency = pw.block(pw.read(PUBLISH), "concurrency", 0).splitlines()
        # A dry run writes nothing: its own group, so that it never replaces
        # a pending publication of the same tag.
        self.assertIn(
            "  group: publish-arduino-package-${{ github.ref_name }}"
            "${{ inputs.dry_run && '-dry-run' || '' }}",
            concurrency,
        )
        self.assertIn("  cancel-in-progress: false", concurrency)


class TokenTest(unittest.TestCase):
    def test_no_tibdex(self) -> None:
        for workflow in sorted(WORKFLOWS.glob("*.yml")):
            with self.subTest(workflow=workflow.name):
                text = workflow.read_text(encoding="utf-8")
                self.assertFalse("tibdex/" in text, f"tibdex/ in {workflow.name}")

    def test_every_app_token_is_pinned_and_scoped(self) -> None:
        steps = token_steps()
        self.assertGreaterEqual(len(steps), 5)
        for workflow, step in steps:
            with self.subTest(workflow=workflow, step=step.splitlines()[0].strip()):
                uses = wv.step_lines(step, "uses:")[0]
                self.assertEqual(uses, f"uses: {TOKEN_ACTION} {TOKEN_VERSION}")
                self.assertEqual(pw.with_value(step, "client-id"), "${{ secrets.RELEASE_APP_ID }}")
                self.assertEqual(
                    pw.with_value(step, "private-key"), "${{ secrets.RELEASE_APP_SECRET }}"
                )
                self.assertEqual(pw.with_value(step, "owner"), "steamicc")
                self.assertTrue(pw.with_value(step, "repositories"))
                self.assertTrue(permissions(step), "explicit permission-* inputs")
                self.assertEqual(wv.step_lines(step, "app-id:"), [])
                self.assertEqual(wv.step_lines(step, "app_id:"), [])

    def test_minimal_scopes(self) -> None:
        contents = {"contents": "write"}
        expected = {
            # semantic-release pushes the release commit and tag, creates the
            # release, then comments on and labels the released issues and PRs.
            (RELEASE, "release", "Generate token"): (
                "arduino-steami-lib",
                {"contents": "write", "issues": "write", "pull-requests": "write"},
            ),
            # Push to the distribution, create its draft release.
            (PUBLISH, "publish", "Generate token"): ("arduino-steami", contents),
            # Read the asset, publish the draft, set Latest.
            (PUBLISH, "publish-release", "Generate token"): ("arduino-steami", contents),
            # Read the draft, upload or delete the archive.
            (ARCHIVE, "examples-archive", "Generate token"): ("arduino-steami", contents),
        }
        for (workflow, job, name), (repositories, perms) in expected.items():
            with self.subTest(workflow=workflow, job=job):
                step = job_steps(workflow, job)[name]
                self.assertTrue(wv.step_uses(step).startswith(TOKEN_ACTION))
                self.assertEqual(pw.with_value(step, "repositories"), repositories)
                self.assertEqual(permissions(step), perms)


def git(*args: str, cwd: Path, env: dict[str, str]) -> str:
    result = subprocess.run(
        ["git", *args], cwd=cwd, env=env, capture_output=True, text=True, check=True
    )
    return result.stdout.strip()


@unittest.skipUnless(shutil.which("git") and shutil.which("bash"), "needs bash and git")
class PushScriptTest(unittest.TestCase):
    """Run the push step against a local bare repository."""

    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp)
        self.remote = self.tmp / "remote.git"
        self.env = dict(
            os.environ,
            HOME=str(self.tmp),
            GIT_CONFIG_NOSYSTEM="1",
            GIT_CONFIG_GLOBAL=str(self.tmp / "gitconfig"),
            GIT_AUTHOR_NAME="t",
            GIT_AUTHOR_EMAIL="t@example.com",
            GIT_COMMITTER_NAME="t",
            GIT_COMMITTER_EMAIL="t@example.com",
        )
        (self.tmp / "gitconfig").write_text(
            "[init]\n\tdefaultBranch = main\n"
            f'[url "{self.remote.as_uri()}"]\n'
            f"\tinsteadOf = https://x-access-token:tok@github.com/{DIST_REPO}.git\n"
        )
        git("init", "-q", "--bare", str(self.remote), cwd=self.tmp, env=self.env)
        self.seed = self.tmp / "seed"
        git("clone", "-q", str(self.remote), str(self.seed), cwd=self.tmp, env=self.env)
        self.script = pw.run_script(job_steps(PUBLISH, "publish")[PUSH_STEP])

    def release(self, tag: str, push: bool = True) -> str:
        """Commit and tag a release in the seed clone; push it unless told not to."""
        (self.seed / "library.properties").write_text(f"version={tag[1:]}\n")
        git("add", "--all", cwd=self.seed, env=self.env)
        git("commit", "-q", "-m", f"Release {tag}", cwd=self.seed, env=self.env)
        git("tag", tag, cwd=self.seed, env=self.env)
        if push:
            git("push", "-q", "origin", "HEAD:refs/heads/main", tag, cwd=self.seed, env=self.env)
        return git("rev-parse", "HEAD", cwd=self.seed, env=self.env)

    def remote_ref(self, ref: str) -> str:
        return git("rev-parse", "-q", "--verify", ref, cwd=self.remote, env=self.env)

    def remote_file(self, ref: str) -> str:
        return git("show", f"{ref}:library.properties", cwd=self.remote, env=self.env)

    def run_push(self, tag: str, dry_run: str = "false") -> subprocess.CompletedProcess:
        package = self.tmp / "package"
        package.mkdir(exist_ok=True)
        (package / "library.properties").write_text(f"version={tag[1:]}\n")
        env = dict(self.env, GH_TOKEN="tok", DIST_REPO=DIST_REPO, TAG=tag, DRY_RUN=dry_run)
        return subprocess.run(
            ["bash", "-c", self.script],
            cwd=self.tmp,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )

    def test_highest_version_moves_main(self) -> None:
        self.release("v0.41.0")
        result = self.run_push("v0.42.0")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.remote_ref("refs/tags/v0.42.0"), self.remote_ref("refs/heads/main"))
        self.assertEqual(self.remote_file("main"), "version=0.42.0")

    def test_older_tag_is_pushed_without_moving_main(self) -> None:
        self.release("v0.41.0")
        newest = self.release("v0.44.0")
        result = self.run_push("v0.43.0")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.remote_ref("refs/heads/main"), newest)
        self.assertEqual(self.remote_file("v0.43.0"), "version=0.43.0")
        self.assertIn("::notice::", result.stdout)

    def test_existing_tag_is_left_as_it_is(self) -> None:
        self.release("v0.41.0")
        head = self.release("v0.42.0")
        result = self.run_push("v0.42.0")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("already exists", result.stdout)
        self.assertEqual(self.remote_ref("refs/heads/main"), head)
        self.assertEqual(self.remote_ref("refs/tags/v0.42.0^{commit}"), head)

    def test_dry_run_writes_nothing(self) -> None:
        head = self.release("v0.41.0")
        result = self.run_push("v0.42.0", dry_run="true")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Dry run", result.stdout)
        self.assertEqual(self.remote_ref("refs/heads/main"), head)
        self.assertNotIn("v0.42.0", git("tag", cwd=self.remote, env=self.env))

    def concurrent_publication(self, tag: str) -> str:
        """Make the first push fail after another publication pushed tag.

        The pre-receive hook moves main and creates the tag of the other
        publication (prepared in the seed), then refuses the push, as GitHub
        refuses a push whose main is no longer a fast-forward.
        """
        other = self.release(tag, push=False)
        git("push", "-q", "origin", f"{other}:refs/prepared/main", cwd=self.seed, env=self.env)
        hook = self.remote / "hooks" / "pre-receive"
        hook.write_text(
            textwrap.dedent(
                f"""\
                #!/usr/bin/env bash
                cat > /dev/null
                [ -e "$GIT_DIR/raced" ] && exit 0
                touch "$GIT_DIR/raced"
                # Out of the push's quarantine, which forbids ref updates.
                unset GIT_QUARANTINE_PATH GIT_OBJECT_DIRECTORY GIT_ALTERNATE_OBJECT_DIRECTORIES
                git update-ref refs/heads/main {other}
                git update-ref refs/tags/{tag} {other}
                echo "main moved" >&2
                exit 1
                """
            )
        )
        hook.chmod(0o755)
        return other

    def test_refused_push_is_rebuilt_on_the_new_main(self) -> None:
        self.release("v0.41.0")
        other = self.concurrent_publication("v0.42.0")
        result = self.run_push("v0.43.0")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("::warning::", result.stdout)
        main = self.remote_ref("refs/heads/main")
        self.assertEqual(self.remote_ref("refs/tags/v0.43.0"), main)
        self.assertEqual(self.remote_ref("refs/heads/main^"), other)
        self.assertEqual(self.remote_file("main"), "version=0.43.0")

    def test_refused_push_keeps_main_on_a_newer_publication(self) -> None:
        self.release("v0.41.0")
        other = self.concurrent_publication("v0.45.0")
        result = self.run_push("v0.44.0")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.remote_ref("refs/heads/main"), other)
        self.assertEqual(self.remote_file("v0.44.0"), "version=0.44.0")


GH_STUB = r"""#!/usr/bin/env python3
# A stub of the gh commands used by the publish-release step, on a JSON state.
import json, os, subprocess, sys

state_path = os.environ["GH_STATE"]
with open(state_path) as f:
    state = json.load(f)
args = sys.argv[1:]
with open(os.environ["GH_LOG"], "a") as log:
    log.write(" ".join(args) + "\n")


def jq(value):
    query = args[args.index("--jq") + 1]
    out = subprocess.run(["jq", "-r", query], input=json.dumps(value), text=True,
                         capture_output=True, check=True)
    sys.stdout.write(out.stdout)


releases = state["releases"]
if args[:2] == ["release", "view"]:
    tag = args[2]
    found = [r for r in releases if r["tagName"] == tag]
    if not found:
        sys.exit(1)
    jq({"assets": found[0].get("assets", []), "isDraft": found[0]["isDraft"]})
elif args[:2] == ["release", "list"]:
    jq([{k: r[k] for k in ("tagName", "isDraft", "isLatest")} for r in releases])
    # Another publication edits its release right after this first read.
    race = state.pop("after_first_list", None)
    if race:
        for r in releases:
            r["isLatest"] = r["tagName"] == race
            if r["tagName"] == race:
                r["isDraft"] = False
elif args[:2] == ["release", "edit"]:
    tag = args[2]
    for r in releases:
        if r["tagName"] == tag and "--draft=false" in args:
            r["isDraft"] = False
        if "--latest" in args:
            r["isLatest"] = r["tagName"] == tag
else:
    sys.exit(f"unexpected gh call: {args}")
with open(state_path, "w") as f:
    json.dump(state, f)
"""


def published(tag: str, latest: bool = False) -> dict:
    return {"tagName": tag, "isDraft": False, "isLatest": latest, "assets": []}


def draft(tag: str, uploaded: bool = True) -> dict:
    assets = [{"name": f"steami-exemples-{tag}.zip", "state": "uploaded"}] if uploaded else []
    return {"tagName": tag, "isDraft": True, "isLatest": False, "assets": assets}


@unittest.skipUnless(shutil.which("jq") and shutil.which("bash"), "needs bash and jq")
class LatestScriptTest(unittest.TestCase):
    """Run the publish-release script with a stub gh."""

    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp)
        stub = self.tmp / "bin" / "gh"
        stub.parent.mkdir()
        stub.write_text(GH_STUB)
        stub.chmod(0o755)
        self.state = self.tmp / "state.json"
        self.log = self.tmp / "gh.log"
        self.log.write_text("")
        self.script = pw.run_script(job_steps(PUBLISH, "publish-release")[LATEST_STEP])

    def run_publish(self, tag: str, state: dict) -> subprocess.CompletedProcess:
        self.state.write_text(json.dumps(state))
        env = dict(
            os.environ,
            PATH=f"{self.tmp / 'bin'}{os.pathsep}{os.environ['PATH']}",
            GH_STATE=str(self.state),
            GH_LOG=str(self.log),
            GH_TOKEN="tok",
            TAG=tag,
            DIST_REPO=DIST_REPO,
        )
        return subprocess.run(
            ["bash", "-c", self.script], env=env, capture_output=True, text=True, check=False
        )

    def latest(self) -> list[str]:
        releases = json.loads(self.state.read_text())["releases"]
        return [r["tagName"] for r in releases if r["isLatest"]]

    def edits(self) -> list[str]:
        lines = self.log.read_text().splitlines()
        return [line for line in lines if line.startswith("release edit")]

    def test_highest_version_becomes_latest(self) -> None:
        result = self.run_publish(
            "v0.45.0", {"releases": [published("v0.44.0", latest=True), draft("v0.45.0")]}
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.latest(), ["v0.45.0"])
        self.assertEqual(
            self.edits(), [f"release edit v0.45.0 -R {DIST_REPO} --draft=false --latest"]
        )

    def test_older_version_keeps_latest(self) -> None:
        result = self.run_publish(
            "v0.43.0", {"releases": [published("v0.44.0", latest=True), draft("v0.43.0")]}
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.latest(), ["v0.44.0"])
        self.assertEqual(
            self.edits(), [f"release edit v0.43.0 -R {DIST_REPO} --draft=false --latest=false"]
        )

    def test_parallel_publication_gives_latest_back_to_the_highest(self) -> None:
        # v0.46.0 is published by another run between the read and the edit
        # of this one: both see themselves as the highest version.
        state = {
            "releases": [published("v0.44.0", latest=True), draft("v0.45.0"), draft("v0.46.0")],
            "after_first_list": "v0.46.0",
        }
        result = self.run_publish("v0.45.0", state)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.latest(), ["v0.46.0"])
        self.assertEqual(self.edits()[-1], f"release edit v0.46.0 -R {DIST_REPO} --latest")

    def test_missing_archive_keeps_the_draft(self) -> None:
        result = self.run_publish(
            "v0.45.0",
            {"releases": [published("v0.44.0", latest=True), draft("v0.45.0", uploaded=False)]},
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("::error::", result.stdout)
        self.assertEqual(self.edits(), [])


if __name__ == "__main__":
    unittest.main()
