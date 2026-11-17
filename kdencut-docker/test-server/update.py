#!/usr/bin/env python3
"""Build the newest eligible GitHub branch and update only kdencut-test."""
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent
REPOSITORY = "https://github.com/TE99SJM/kdencut.git"
CONTAINER = "kdencut-test"
DOCKERFILE = "kdencut-docker/Dockerfile"


def run(*args, capture=False, check=True, **kwargs):
    return subprocess.run(args, text=True, check=check,
                          stdout=subprocess.PIPE if capture else None,
                          **kwargs)


def read_json(path, default=None):
    return json.loads(path.read_text()) if path.exists() else default


def write_json(path, value):
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


def git(*args, **kwargs):
    return run("git", "--git-dir", str(ROOT / "source.git"), *args, **kwargs)


def fetch_source():
    if not (ROOT / "source.git").exists():
        git("init", "--bare")
        git("remote", "add", "origin", REPOSITORY)
    # Fetch branch tips only, including branches created since installation.
    # No working copy and no edits to the user's other repository checkout.
    git("fetch", "--depth=1", "--prune", "origin",
        "+refs/heads/*:refs/heads/*")


def select_revision(branch=None, previous_branch=None):
    rows = git("for-each-ref", "--sort=-committerdate",
               "--format=%(committerdate:unix) %(objectname) %(refname:strip=2)",
               "refs/heads", capture=True).stdout.splitlines()
    candidates = [row.split(" ", 2) for row in rows]
    # Equal timestamps: prefer the currently deployed branch, then its name.
    candidates.sort(key=lambda x: (-int(x[0]), x[2] != previous_branch, x[2]))
    for _, commit, name in candidates:
        if branch and name != branch:
            continue
        result = git("cat-file", "-e", f"{commit}:{DOCKERFILE}", check=False,
                     stderr=subprocess.DEVNULL)
        if result.returncode == 0:
            return {"branch": name, "commit": commit,
                    "image": f"kdencut-test:{commit}"}
    raise RuntimeError("No matching branch contains " + DOCKERFILE)


def build(revision, jobs):
    if run("docker", "image", "inspect", revision["image"], check=False,
           capture=True, stderr=subprocess.DEVNULL).returncode == 0:
        return
    archive = subprocess.Popen(
        ["git", "--git-dir", str(ROOT / "source.git"), "archive",
         "--format=tar", revision["commit"]], stdout=subprocess.PIPE)
    try:
        run("docker", "build", "--progress=plain", "--build-arg",
            f"BUILD_JOBS={jobs}", "--label",
            f"org.opencontainers.image.revision={revision['commit']}",
            "-f", DOCKERFILE, "-t", revision["image"], "-",
            stdin=archive.stdout)
    finally:
        archive.stdout.close()
        result = archive.wait()
    if result:
        raise RuntimeError("git archive failed")


def compose(image):
    environment = os.environ.copy()
    environment["KDENCUT_TEST_IMAGE"] = image
    run("docker", "compose", "--project-directory", str(ROOT),
        "--env-file", str(ROOT / ".env"), "-p", CONTAINER,
        "-f", str(ROOT / "compose.yaml"), "up", "-d", "--no-build",
        "--pull", "never", env=environment)


def ready(timeout=180):
    deadline = time.monotonic() + timeout
    consecutive = 0
    while time.monotonic() < deadline:
        # Check both the web server and a running editor process. A web login
        # challenge is expected. This is a startup check, not an editing test.
        app = run("docker", "exec", CONTAINER, "pgrep", "-x", "kdenlive",
                  capture=True, check=False, stderr=subprocess.DEVNULL)
        web = run("docker", "exec", CONTAINER, "curl", "-s", "-o",
                  "/dev/null", "-w", "%{http_code}", "--max-time", "5",
                  "http://127.0.0.1:3000/", capture=True, check=False,
                  stderr=subprocess.DEVNULL)
        healthy = app.returncode == 0 and web.returncode == 0 and web.stdout in (
            "200", "301", "302", "401")
        consecutive = consecutive + 1 if healthy else 0
        if consecutive >= 3:
            return True
        time.sleep(5)
    return False


def update(retry=False):
    settings = read_json(ROOT / "settings.json", {})
    jobs = int(settings.get("build_jobs", 2))
    if jobs < 1:
        raise ValueError("build_jobs must be positive")
    previous = read_json(ROOT / "deployed.json")
    fetch_source()
    revision = select_revision(settings.get("branch"),
                               previous["branch"] if previous else None)
    print(f"Selected {revision['branch']} at {revision['commit']}", flush=True)
    if previous and revision["commit"] == previous["commit"]:
        print("Already deployed; no rebuild needed.", flush=True)
        return
    failed = read_json(ROOT / "failed.json")
    if failed and failed["commit"] == revision["commit"] and not retry:
        print("This commit failed previously. Waiting for a newer commit; "
              "use update.py --retry to try it again.", flush=True)
        return
    switching = False
    try:
        build(revision, jobs)
        switching = True
        compose(revision["image"])
        if not ready():
            raise RuntimeError("KdenCut failed the 180-second startup check")
    except Exception as error:
        write_json(ROOT / "failed.json", {**revision, "error": str(error)})
        if switching and previous:
            print("Restarting the last successful test image.", flush=True)
            compose(previous["image"])
            if not ready():
                print("Previous image also failed startup; inspect logs.",
                      file=sys.stderr)
        raise
    write_json(ROOT / "deployed.json", revision)
    (ROOT / "failed.json").unlink(missing_ok=True)
    print(f"Deployed {revision['branch']} ({revision['commit'][:12]}) "
          "at https://mserver:3009", flush=True)
    # Remove only our previous test image after a successful deployment.
    # Never prune other apps, volumes, or the shared Docker build cache.
    if previous and previous["image"] != revision["image"]:
        run("docker", "image", "rm", previous["image"], check=False)


def main():
    if set(sys.argv[1:]) - {"--retry"}:
        raise SystemExit("Usage: update.py [--retry]")
    os.umask(0o077)
    with (ROOT / "update.lock").open("w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print("Another update is already running.")
            return
        update(retry="--retry" in sys.argv)


if __name__ == "__main__":
    main()
