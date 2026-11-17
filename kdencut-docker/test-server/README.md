# Automatic KdenCut test server

This installs a separate `kdencut-test` Docker container on mserver, with HTTP
port 3008 and HTTPS port 3009. A systemd timer on mserver checks GitHub five
minutes after each previous check or build finishes. It resumes after reboot.

## Install

Run as the normal user who already has Docker access. Requires Git, Python 3,
Docker Compose v2, systemd and sudo. Ubuntu 26.04 provides the host tools.

```bash
cd ~/kdencut-src
git switch kdencut-touch-capcut
git pull --ff-only origin kdencut-touch-capcut
bash kdencut-docker/test-server/install.sh
```

The installer starts the first build in the background, so closing SSH does not
cancel it. It prints a generated browser password, also saved in the private
`~/kdencut-test/.env` file. Follow progress with:

```bash
sudo journalctl -fu kdencut-test-update.service
```

After a successful build and startup check, open `https://mserver:3009` (or use
mserver's LAN/Tailscale IP). Accept the local certificate warning. Use this only
on the home network or Tailscale; do not forward these ports to the internet.

## What "newest branch" means

The updater fetches branch tips from **TE99SJM/kdencut**, chooses the branch with
the newest tip **committer timestamp** that contains `kdencut-docker/Dockerfile`,
and builds that exact commit. This is not branch creation time or GitHub push
time. Tied dates prefer the deployed branch, then alphabetical order. Upstream
branches and fork pull requests outside this repository are not fetched.

New branches are discovered automatically. A branch without the Dockerfile is
skipped. The same commit is not rebuilt on every check. To track a particular
branch instead, edit `~/kdencut-test/settings.json`:

```json
{"branch": "kdencut-touch-capcut", "build_jobs": 2}
```

Set `branch` back to `null` for newest-branch selection. Build jobs default to 2
to limit memory pressure. Builds still need free disk space for dependencies,
compiler output and Docker cache; the first build may take 30–90 minutes.

## Data and deployment behavior

- Test settings: `~/kdencut-test/config`.
- Writable test projects and exports: `~/kdencut-test/projects`, shown in the
  editor as `/storage/projects`. Choose this folder when saving a project.
- NAS files: `/mnt/mnas` mounted read-only at `/media`, plus the existing
  `/storage/clips`, `/storage/music`, `/storage/photos`, `/storage/video` paths.
  Existing NAS projects can be opened from `/media/Video editing` and saved as
  a test copy in `/storage/projects`.
- The updater uses its own shallow bare checkout. It does not change the normal
  source checkout or the `kdenlive` / `kdencut` containers.
- A failed image build leaves the running test container in place. After a
  successful build, only the test container is recreated (brief downtime).
- Startup checks require a running `kdenlive` process and a responding web
  server three times. They are not a full editing, rendering or touchscreen test.
- If startup fails, the updater attempts to restart the last successfully
  deployed test image. This does not undo settings or project changes.
- A failed commit is not rebuilt every five minutes. A newer commit triggers
  another attempt; retry the same commit manually as shown below.
- After success, the previous test image tag is removed. No file backups are
  created. Shared Docker build cache is retained; no global prune is performed.
- Deployed branch and commit are recorded in `~/kdencut-test/deployed.json`;
  the last failed attempt is recorded in `failed.json`.

The timer is installed on **mserver**, not as a ChatGPT scheduled task. Docker
builds run there. The test app does not receive the host Docker socket. Branch
Dockerfiles execute during the build, so only trusted contributors should be
able to push branches to this repository. The installed updater and Compose
configuration do not change automatically when application branches change;
rerun the installer to update those files.

## Manage

```bash
# Check timer and current deployment
systemctl status kdencut-test-update.timer
cat ~/kdencut-test/deployed.json

# Check GitHub immediately
sudo systemctl start --no-block kdencut-test-update.service

# Retry a failed commit (keep this SSH session open during a manual build)
python3 ~/kdencut-test/update.py --retry

# Pause automatic updates; the test app stays running
sudo systemctl disable --now kdencut-test-update.timer

# Resume automatic updates
sudo systemctl enable --now kdencut-test-update.timer

# Inspect the app
docker logs --tail 100 kdencut-test
```

Pausing the timer does not interrupt a build already running. The file lock
prevents manual and scheduled updates from running concurrently.
