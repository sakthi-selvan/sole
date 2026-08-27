#!/usr/bin/env bash
# START overlay-chat
set -euo pipefail
BIN="${HOME}/.local/bin/overlay-chat"
if [[ ! -x "${BIN}" ]]; then
  BIN="$(cd "$(dirname "$0")/.." && pwd)/overlay-chat"
fi
if [[ ! -x "${BIN}" ]]; then
  echo "overlay-chat is not built. Run: make && make install" >&2
  exit 1
fi
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "${ROOT}/.env" ]]; then
  set -a
  # shellcheck disable=SC1091
  source "${ROOT}/.env"
  set +a
fi
if [[ -f "${HOME}/.config/overlay-chat/env" ]]; then
  set -a
  # shellcheck disable=SC1091
  source "${HOME}/.config/overlay-chat/env"
  set +a
fi
exec "${BIN}" "$@"
