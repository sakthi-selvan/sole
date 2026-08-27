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
ITEM_ALT="/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/overlay-chat-alt/"
KEY="${SCHEMA}.custom-keybinding:${ITEM}"
KEY_ALT="${SCHEMA}.custom-keybinding:${ITEM_ALT}"

if ! command -v gsettings >/dev/null 2>&1; then
  echo "gsettings not found; skip GNOME shortcut binding"
  exit 0
fi

existing="$(gsettings get "${SCHEMA}" custom-keybindings 2>/dev/null || echo "@as []")"
add_item() {
  local path="$1"
  if [[ "${existing}" == *"${path}"* ]]; then
    return
  fi
  if [[ "${existing}" == "@as []" || "${existing}" == "[]" ]]; then
    existing="['${path}']"
  else
    existing="${existing%]*}, '${path}']"
  fi
  gsettings set "${SCHEMA}" custom-keybindings "${existing}"
}

add_item "${ITEM}"
existing="$(gsettings get "${SCHEMA}" custom-keybindings 2>/dev/null || echo "@as []")"
add_item "${ITEM_ALT}"

gsettings set "${KEY}" name "Overlay Chat hide/show"
gsettings set "${KEY}" command "${BIN} --toggle"
gsettings set "${KEY}" binding "<Super><Shift>o"

gsettings set "${KEY_ALT}" name "Overlay Chat hide/show (backup)"
gsettings set "${KEY_ALT}" command "${BIN} --toggle"
gsettings set "${KEY_ALT}" binding "<Primary><Alt>o"

echo "GNOME shortcut installed: Super+Shift+O -> ${BIN} --toggle"
echo "Backup shortcut:          Ctrl+Alt+O    -> ${BIN} --toggle"
