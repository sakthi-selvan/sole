#!/usr/bin/env bash
set -euo pipefail

BIN="${HOME}/.local/bin/overlay-chat"
if [[ ! -x "${BIN}" ]]; then
  BIN="$(cd "$(dirname "$0")/.." && pwd)/overlay-chat"
fi
if [[ ! -x "${BIN}" ]]; then
  echo "overlay-chat is not installed. Run: make install" >&2
  exit 1
fi

SCHEMA="org.gnome.settings-daemon.plugins.media-keys"
ITEM="/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/overlay-chat/"
KEY="${SCHEMA}.custom-keybinding:${ITEM}"

if ! command -v gsettings >/dev/null 2>&1; then
  echo "gsettings not found; skip GNOME Ctrl+Shift+S binding"
  exit 0
fi

existing="$(gsettings get "${SCHEMA}" custom-keybindings 2>/dev/null || echo "@as []")"
if [[ "${existing}" != *overlay-chat* ]]; then
  if [[ "${existing}" == "@as []" || "${existing}" == "[]" ]]; then
    gsettings set "${SCHEMA}" custom-keybindings "['${ITEM}']"
  else
    merged="${existing%]*}, '${ITEM}']"
    gsettings set "${SCHEMA}" custom-keybindings "${merged}"
  fi
fi

gsettings set "${KEY}" name "Overlay Chat hide/show"
gsettings set "${KEY}" command "${BIN} --toggle"
gsettings set "${KEY}" binding "<Control><Shift>s"

echo "GNOME shortcut installed: Ctrl+Shift+S -> ${BIN} --toggle"
