#!/usr/bin/env bash
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
  printf 'Run this as your normal Docker user, without sudo.\n' >&2
  exit 1
fi
for program in docker git python3 systemctl sudo; do
  command -v "$program" >/dev/null
done
docker info >/dev/null
docker compose version >/dev/null

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
target_dir="$HOME/kdencut-test"
umask 077
mkdir -p "$target_dir" "$target_dir/config" "$target_dir/projects"
if docker container inspect kdencut-test >/dev/null 2>&1; then
  owner=$(docker inspect kdencut-test --format '{{index .Config.Labels "com.docker.compose.project"}}')
  if [ "$owner" != kdencut-test ]; then
    printf 'A different container already uses the name kdencut-test.\n' >&2
    exit 1
  fi
fi

# Stop a prior updater before replacing its program. The app keeps running.
sudo systemctl stop kdencut-test-update.timer kdencut-test-update.service 2>/dev/null || true
install -m 600 "$source_dir/update.py" "$target_dir/update.py"
install -m 600 "$source_dir/compose.yaml" "$target_dir/compose.yaml"
python3 - "$target_dir" <<'PY'
import json, os, secrets, sys
from pathlib import Path
root = Path(sys.argv[1])
env = root / '.env'
if not env.exists():
    env.write_text(f'PUID={os.getuid()}\nPGID={os.getgid()}\n'
                   'CUSTOM_USER=samuel\n'
                   f'PASSWORD={secrets.token_hex(16)}\n'
                   'MEDIA_DIR=/mnt/mnas\n')
settings = root / 'settings.json'
if not settings.exists():
    settings.write_text(json.dumps({'branch': None, 'build_jobs': 2}, indent=2) + '\n')
PY

sudo tee /etc/systemd/system/kdencut-test-update.service >/dev/null <<EOF
[Unit]
Description=Build and deploy the newest KdenCut test branch
Wants=network-online.target
After=network-online.target docker.service
Requires=docker.service

[Service]
Type=oneshot
User=$(id -un)
Group=$(id -gn)
Environment=PYTHONUNBUFFERED=1
WorkingDirectory="$target_dir"
ExecStart=/usr/bin/python3 "$target_dir/update.py"
TimeoutStartSec=infinity
UMask=0077
EOF

sudo tee /etc/systemd/system/kdencut-test-update.timer >/dev/null <<'EOF'
[Unit]
Description=Check KdenCut branches five minutes after each update completes

[Timer]
OnBootSec=2min
OnUnitInactiveSec=5min
Unit=kdencut-test-update.service

[Install]
WantedBy=timers.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now kdencut-test-update.timer
sudo systemctl start --no-block kdencut-test-update.service
printf '\nKdenCut test updater installed. First build is starting.\n'
printf 'Browser: https://mserver:3009\n'
sed -n '/^CUSTOM_USER=/p; /^PASSWORD=/p' "$target_dir/.env"
printf '\nFollow progress: sudo journalctl -fu kdencut-test-update.service\n'
