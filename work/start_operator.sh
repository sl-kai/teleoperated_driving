#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

if [[ ! -f .env ]]; then
  echo "ERROR: .env is missing in $(pwd)."
  exit 1
fi

display="${DISPLAY:-}"
if [[ -z "$display" ]]; then
  display="$(sed -n 's/^DISPLAY=//p' .env | head -n 1)"
fi
export DISPLAY="${display:-:0}"

xauth_file="$(find "/run/user/$(id -u)" -maxdepth 1 -type f \
  -name '.mutter-Xwaylandauth.*' -print -quit)"
if [[ -z "$xauth_file" ]]; then
  echo "ERROR: Xwayland authorization file not found. Run this from the graphical desktop session."
  exit 1
fi
export XAUTHORITY="$xauth_file"

set_env() {
  local key="$1"
  local value="$2"

  if grep -q "^${key}=" .env; then
    sed -i "s#^${key}=.*#${key}=${value}#" .env
  else
    printf '\n%s=%s\n' "$key" "$value" >> .env
  fi
}

xhost +si:localuser:root
set_env DISPLAY "$DISPLAY"
set_env XAUTHORITY "$XAUTHORITY"

docker compose config >/dev/null
docker compose up -d --no-deps --force-recreate tod_operator
