#!/usr/bin/env bash
# END overlay-chat
set -euo pipefail
BIN="${HOME}/.local/bin/overlay-chat"
if [[ ! -x "${BIN}" ]]; then
  BIN="$(cd "$(dirname "$0")/.." && pwd)/overlay-chat"
fi
if [[ ! -x "${BIN}" ]]; then
  echo "overlay-chat is not built. Run: make && make install" >&2
  exit 1
fi
exec "${BIN}" --quit
